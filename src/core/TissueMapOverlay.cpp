#include "TissueMapOverlay.h"
#include <cmath>
#include <algorithm>
#include <iostream>


TissueTileIndex::TissueTileIndex(int gridWidth, int gridHeight,
                                   double slideWidth, double slideHeight)
    : gridWidth_(gridWidth)
    , gridHeight_(gridHeight) {
    cellWidth_ = slideWidth / gridWidth;
    cellHeight_ = slideHeight / gridHeight;

    // Initialize 2D grid
    grid_.resize(gridHeight_);
    for (auto& row : grid_) {
        row.resize(gridWidth_);
    }
}

void TissueTileIndex::Build(std::vector<TissueTile>& tiles) {
    Clear();

    // Insert each tile into overlapping grid cells
    for (auto& tile : tiles) {
        // Get grid cell range that intersects the tile's bounds
        int minCellX, minCellY, maxCellX, maxCellY;
        SlideToGridCell(tile.bounds.x, tile.bounds.y, minCellX, minCellY);
        SlideToGridCell(tile.bounds.x + tile.bounds.width,
                        tile.bounds.y + tile.bounds.height, maxCellX, maxCellY);

        // Insert into all overlapping cells
        for (int cellY = minCellY; cellY <= maxCellY; ++cellY) {
            for (int cellX = minCellX; cellX <= maxCellX; ++cellX) {
                grid_[cellY][cellX].push_back(&tile);
            }
        }
    }

    // Print statistics
    size_t totalEntries = 0;
    size_t maxPerCell = 0;
    size_t nonEmptyCells = 0;

    for (const auto& row : grid_) {
        for (const auto& cell : row) {
            if (!cell.empty()) {
                ++nonEmptyCells;
                totalEntries += cell.size();
                maxPerCell = std::max(maxPerCell, cell.size());
            }
        }
    }

    double avgPerCell = nonEmptyCells > 0 ?
                        static_cast<double>(totalEntries) / nonEmptyCells : 0.0;

    std::cout << "TissueTileIndex built: "
              << nonEmptyCells << " / " << (gridWidth_ * gridHeight_) << " cells occupied"
              << ", avg " << avgPerCell << " tiles/cell"
              << ", max " << maxPerCell << " tiles/cell" << std::endl;
}

std::vector<TissueTile*> TissueTileIndex::QueryRegion(const Rect& region) const {
    // Get grid cell range
    int minCellX, minCellY, maxCellX, maxCellY;
    SlideToGridCell(region.x, region.y, minCellX, minCellY);
    SlideToGridCell(region.x + region.width, region.y + region.height, maxCellX, maxCellY);

    // Collect candidates, then deduplicate
    std::vector<TissueTile*> candidates;
    candidates.reserve((maxCellX - minCellX + 1) * (maxCellY - minCellY + 1) * 4);

    for (int cellY = minCellY; cellY <= maxCellY; ++cellY) {
        for (int cellX = minCellX; cellX <= maxCellX; ++cellX) {
            for (TissueTile* tile : grid_[cellY][cellX]) {
                candidates.push_back(tile);
            }
        }
    }

    // Deduplicate using sort + unique
    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());

    // Filter by actual intersection
    std::vector<TissueTile*> result;
    result.reserve(candidates.size());
    for (TissueTile* tile : candidates) {
        if (tile->bounds.Intersects(region)) {
            result.push_back(tile);
        }
    }

    return result;
}

void TissueTileIndex::Clear() {
    for (auto& row : grid_) {
        for (auto& cell : row) {
            cell.clear();
        }
    }
}

void TissueTileIndex::SlideToGridCell(double x, double y, int& outCellX, int& outCellY) const {
    outCellX = static_cast<int>(x / cellWidth_);
    outCellY = static_cast<int>(y / cellHeight_);

    // Clamp to grid bounds
    outCellX = std::max(0, std::min(outCellX, gridWidth_ - 1));
    outCellY = std::max(0, std::min(outCellY, gridHeight_ - 1));
}

// ========== TissueMapOverlay Implementation ==========

// Default tissue color palette - distinguishable colors for common tissue types
static const std::array<SDL_Color, 12> kDefaultTissuePalette = {{
    {255, 99, 71, 255},    // 0: Tomato red - tumor
    {144, 238, 144, 255},  // 1: Light green - stroma
    {135, 206, 235, 255},  // 2: Sky blue - necrosis
    {255, 218, 185, 255},  // 3: Peach - background/adipose
    {221, 160, 221, 255},  // 4: Plum - lymphocyte aggregate
    {240, 230, 140, 255},  // 5: Khaki - mucus
    {188, 143, 143, 255},  // 6: Rosy brown - blood
    {175, 238, 238, 255},  // 7: Pale turquoise - epithelium
    {255, 182, 193, 255},  // 8: Light pink - muscle
    {211, 211, 211, 255},  // 9: Light gray - cartilage
    {152, 251, 152, 255},  // 10: Pale green - nerve
    {255, 160, 122, 255},  // 11: Light salmon - other
}};

TissueMapOverlay::TissueMapOverlay(SDL_Renderer* renderer)
    : renderer_(renderer)
    , visible_(false)
    , opacity_(0.5f)
    , slideWidth_(0)
    , slideHeight_(0)
    , maxLevel_(0) {
}

TissueMapOverlay::~TissueMapOverlay() {
    DestroyAllTextures();
}

void TissueMapOverlay::SetTissueData(std::vector<TissueTile>&& tiles,
                                      const std::map<int, std::string>& classMapping,
                                      int maxLevel) {
    // Clean up existing data
    DestroyAllTextures();
    tiles_.clear();
    classes_.clear();
    classIdToIndex_.clear();
    spatialIndex_.reset();

    // Store new data
    tiles_ = std::move(tiles);
    maxLevel_ = maxLevel;

    // Pre-compute scale factors and bounds for each tile (Fix 3)
    for (auto& tile : tiles_) {
        tile.scaleFactor = std::pow(2, maxLevel_ - tile.level);
        double tileX0 = tile.tileX * tile.width * tile.scaleFactor;
        double tileY0 = tile.tileY * tile.height * tile.scaleFactor;
        double tileWidth = tile.width * tile.scaleFactor;
        double tileHeight = tile.height * tile.scaleFactor;
        tile.bounds = Rect(tileX0, tileY0, tileWidth, tileHeight);
    }

    // Build class list from mapping
    for (const auto& [classId, className] : classMapping) {
        TissueClass tc;
        tc.classId = classId;
        tc.name = className;
        tc.color = GetDefaultTissueColor(classId);
        tc.visible = true;

        classIdToIndex_[classId] = classes_.size();
        classes_.push_back(tc);
    }

    // Scan tiles to find any class IDs not in mapping
    for (const auto& tile : tiles_) {
        for (uint8_t classId : tile.classData) {
            if (classIdToIndex_.find(classId) == classIdToIndex_.end()) {
                TissueClass tc;
                tc.classId = classId;
                tc.name = "Class " + std::to_string(classId);
                tc.color = GetDefaultTissueColor(classId);
                tc.visible = true;

                classIdToIndex_[classId] = classes_.size();
                classes_.push_back(tc);
            }
        }
    }

    // Build color lookup table (Fix 4)
    RebuildColorLUT();

    std::cout << "Loaded " << tiles_.size() << " tissue tiles with "
              << classes_.size() << " classes" << std::endl;

    // Note: Spatial index built when slide dimensions are set
}

void TissueMapOverlay::Clear() {
    DestroyAllTextures();
    tiles_.clear();
    classes_.clear();
    classIdToIndex_.clear();
    spatialIndex_.reset();
    visible_ = false;
}

void TissueMapOverlay::Render(const Viewport& viewport) {
    if (!visible_ || tiles_.empty()) {
        return;
    }

    // Get visible region in slide coordinates
    Rect visibleRegion = viewport.GetVisibleRegion();

    // Enable alpha blending
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

    // Query spatial index for visible tiles (O(k) instead of O(N))
    std::vector<TissueTile*> visibleTiles;
    if (spatialIndex_) {
        visibleTiles = spatialIndex_->QueryRegion(visibleRegion);
    } else {
        // Fallback to linear scan if spatial index not built
        for (auto& tile : tiles_) {
            if (tile.bounds.Intersects(visibleRegion)) {
                visibleTiles.push_back(&tile);
            }
        }
    }

    // Render visible tiles using pre-computed bounds (Fix 2 & 3)
    for (TissueTile* tile : visibleTiles) {
        // Ensure texture is generated and valid
        RenderTileTexture(*tile);

        if (!tile->texture) {
            continue;
        }

        // Use pre-computed bounds (no std::pow per tile per frame)
        Vec2 topLeft = viewport.SlideToScreen(Vec2(tile->bounds.x, tile->bounds.y));
        Vec2 bottomRight = viewport.SlideToScreen(
            Vec2(tile->bounds.x + tile->bounds.width, tile->bounds.y + tile->bounds.height));

        // Calculate destination rectangle (use floor/ceil to avoid gaps)
        SDL_Rect dstRect = {
            static_cast<int>(std::floor(topLeft.x)),
            static_cast<int>(std::floor(topLeft.y)),
            static_cast<int>(std::ceil(bottomRight.x - std::floor(topLeft.x))),
            static_cast<int>(std::ceil(bottomRight.y - std::floor(topLeft.y)))
        };

        // Set texture alpha based on opacity
        SDL_SetTextureAlphaMod(tile->texture, static_cast<uint8_t>(opacity_ * 255));

        // Render the tile
        SDL_RenderCopy(renderer_, tile->texture, nullptr, &dstRect);
    }
}

void TissueMapOverlay::RenderTileTexture(TissueTile& tile) {
    // Return if texture is already valid
    if (tile.texture && tile.textureValid) {
        return;
    }

    // Validate tile data
    if (tile.classData.empty() || tile.width <= 0 || tile.height <= 0) {
        return;
    }

    size_t expectedSize = static_cast<size_t>(tile.width) * tile.height;
    if (tile.classData.size() != expectedSize) {
        std::cerr << "Tissue tile data size mismatch: expected " << expectedSize
                  << ", got " << tile.classData.size() << std::endl;
        return;
    }

    // Create RGBA pixel buffer using color LUT for O(1) lookup per pixel (Fix 4)
    std::vector<uint32_t> pixels(tile.width * tile.height);

    for (int i = 0; i < tile.width * tile.height; ++i) {
        uint8_t classId = tile.classData[i];
        // Direct array access instead of map lookup (O(1) vs O(log N))
        const SDL_Color& color = colorLUT_[classId];

        // Pack as RGBA8888 (SDL uses this format on most platforms)
        // Note: SDL_PIXELFORMAT_RGBA8888 is R in highest byte
        pixels[i] = (static_cast<uint32_t>(color.r) << 24) |
                    (static_cast<uint32_t>(color.g) << 16) |
                    (static_cast<uint32_t>(color.b) << 8) |
                    static_cast<uint32_t>(color.a);
    }

    // Create texture if it doesn't exist
    if (!tile.texture) {
        tile.texture = SDL_CreateTexture(renderer_,
                                         SDL_PIXELFORMAT_RGBA8888,
                                         SDL_TEXTUREACCESS_STATIC,
                                         tile.width, tile.height);
        if (!tile.texture) {
            std::cerr << "Failed to create tissue tile texture: " << SDL_GetError() << std::endl;
            return;
        }
        SDL_SetTextureBlendMode(tile.texture, SDL_BLENDMODE_BLEND);
    }

    // Update texture with pixel data
    int result = SDL_UpdateTexture(tile.texture, nullptr, pixels.data(),
                                   tile.width * sizeof(uint32_t));
    if (result != 0) {
        std::cerr << "Failed to update tissue tile texture: " << SDL_GetError() << std::endl;
        return;
    }

    tile.textureValid = true;
}

void TissueMapOverlay::InvalidateAllTextures() {
    for (auto& tile : tiles_) {
        tile.textureValid = false;
    }
}

void TissueMapOverlay::DestroyAllTextures() {
    for (auto& tile : tiles_) {
        if (tile.texture) {
            SDL_DestroyTexture(tile.texture);
            tile.texture = nullptr;
        }
        tile.textureValid = false;
    }
}

void TissueMapOverlay::RebuildColorLUT() {
    // Fill all entries with transparent by default
    colorLUT_.fill({0, 0, 0, 0});

    // Set colors for known classes based on visibility
    for (const auto& tc : classes_) {
        if (tc.classId >= 0 && tc.classId < 256) {
            if (tc.visible) {
                colorLUT_[tc.classId] = tc.color;
            }
            // If not visible, stays transparent (already filled)
        }
    }
}

void TissueMapOverlay::BuildSpatialIndex() {
    if (slideWidth_ <= 0 || slideHeight_ <= 0 || tiles_.empty()) {
        spatialIndex_.reset();
        return;
    }

    std::cout << "Building tissue tile spatial index..." << std::endl;
    spatialIndex_ = std::make_unique<TissueTileIndex>(
        DEFAULT_GRID_SIZE, DEFAULT_GRID_SIZE, slideWidth_, slideHeight_);
    spatialIndex_->Build(tiles_);
}

void TissueMapOverlay::SetOpacity(float opacity) {
    opacity_ = std::max(0.0f, std::min(1.0f, opacity));
}

void TissueMapOverlay::SetClassVisible(int classId, bool visible) {
    auto it = classIdToIndex_.find(classId);
    if (it != classIdToIndex_.end()) {
        classes_[it->second].visible = visible;
        RebuildColorLUT();         // Update color LUT
        InvalidateAllTextures();   // Need to regenerate textures
    }
}

bool TissueMapOverlay::IsClassVisible(int classId) const {
    auto it = classIdToIndex_.find(classId);
    if (it != classIdToIndex_.end()) {
        return classes_[it->second].visible;
    }
    return false;
}

void TissueMapOverlay::SetAllClassesVisible(bool visible) {
    for (auto& tc : classes_) {
        tc.visible = visible;
    }
    RebuildColorLUT();
    InvalidateAllTextures();
}

void TissueMapOverlay::SetClassColor(int classId, SDL_Color color) {
    auto it = classIdToIndex_.find(classId);
    if (it != classIdToIndex_.end()) {
        classes_[it->second].color = color;
        RebuildColorLUT();         // Update color LUT
        InvalidateAllTextures();   // Need to regenerate textures
    }
}

SDL_Color TissueMapOverlay::GetClassColor(int classId) const {
    auto it = classIdToIndex_.find(classId);
    if (it != classIdToIndex_.end()) {
        return classes_[it->second].color;
    }
    return {128, 128, 128, 255};  // Default gray
}

std::vector<int> TissueMapOverlay::GetClassIds() const {
    std::vector<int> ids;
    ids.reserve(classes_.size());
    for (const auto& tc : classes_) {
        ids.push_back(tc.classId);
    }
    return ids;
}

std::string TissueMapOverlay::GetClassName(int classId) const {
    auto it = classIdToIndex_.find(classId);
    if (it != classIdToIndex_.end()) {
        return classes_[it->second].name;
    }
    return "Unknown";
}

void TissueMapOverlay::SetSlideDimensions(double width, double height) {
    slideWidth_ = width;
    slideHeight_ = height;

    // Build spatial index now that we have dimensions
    BuildSpatialIndex();
}

SDL_Color TissueMapOverlay::GetDefaultTissueColor(int classId) {
    // Use modulo to cycle through palette for any class ID
    size_t idx = static_cast<size_t>(classId) % kDefaultTissuePalette.size();
    return kDefaultTissuePalette[idx];
}
