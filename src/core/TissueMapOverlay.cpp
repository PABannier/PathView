#include "TissueMapOverlay.h"
#include <cmath>
#include <algorithm>
#include <iostream>

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

    // Store new data
    tiles_ = std::move(tiles);
    maxLevel_ = maxLevel;

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

    std::cout << "Loaded " << tiles_.size() << " tissue tiles with "
              << classes_.size() << " classes" << std::endl;
}

void TissueMapOverlay::Clear() {
    DestroyAllTextures();
    tiles_.clear();
    classes_.clear();
    classIdToIndex_.clear();
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

    // Render each visible tile
    for (auto& tile : tiles_) {
        // Calculate tile bounds in level-0 slide coordinates
        double scaleFactor = std::pow(2, maxLevel_ - tile.level);
        double tileX0 = tile.tileX * tile.width * scaleFactor;
        double tileY0 = tile.tileY * tile.height * scaleFactor;
        double tileWidth = tile.width * scaleFactor;
        double tileHeight = tile.height * scaleFactor;

        Rect tileBounds(tileX0, tileY0, tileWidth, tileHeight);

        // Skip tiles outside visible region
        if (!tileBounds.Intersects(visibleRegion)) {
            continue;
        }

        // Ensure texture is generated and valid
        RenderTileTexture(tile);

        if (!tile.texture) {
            continue;
        }

        // Transform tile corners to screen coordinates
        Vec2 topLeft = viewport.SlideToScreen(Vec2(tileX0, tileY0));
        Vec2 bottomRight = viewport.SlideToScreen(Vec2(tileX0 + tileWidth, tileY0 + tileHeight));

        // Calculate destination rectangle (use floor/ceil to avoid gaps)
        SDL_Rect dstRect = {
            static_cast<int>(std::floor(topLeft.x)),
            static_cast<int>(std::floor(topLeft.y)),
            static_cast<int>(std::ceil(bottomRight.x - std::floor(topLeft.x))),
            static_cast<int>(std::ceil(bottomRight.y - std::floor(topLeft.y)))
        };

        // Set texture alpha based on opacity
        SDL_SetTextureAlphaMod(tile.texture, static_cast<uint8_t>(opacity_ * 255));

        // Render the tile
        SDL_RenderCopy(renderer_, tile.texture, nullptr, &dstRect);
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

    // Create RGBA pixel buffer
    std::vector<uint32_t> pixels(tile.width * tile.height);

    for (int i = 0; i < tile.width * tile.height; ++i) {
        uint8_t classId = tile.classData[i];
        SDL_Color color = GetPixelColor(classId);

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

SDL_Color TissueMapOverlay::GetPixelColor(uint8_t classId) const {
    auto it = classIdToIndex_.find(classId);
    if (it == classIdToIndex_.end()) {
        // Unknown class - return transparent
        return {0, 0, 0, 0};
    }

    const TissueClass& tc = classes_[it->second];

    // If class is not visible, return transparent
    if (!tc.visible) {
        return {0, 0, 0, 0};
    }

    return tc.color;
}

void TissueMapOverlay::SetOpacity(float opacity) {
    opacity_ = std::max(0.0f, std::min(1.0f, opacity));
}

void TissueMapOverlay::SetClassVisible(int classId, bool visible) {
    auto it = classIdToIndex_.find(classId);
    if (it != classIdToIndex_.end()) {
        classes_[it->second].visible = visible;
        InvalidateAllTextures();  // Need to regenerate textures
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
    InvalidateAllTextures();
}

void TissueMapOverlay::SetClassColor(int classId, SDL_Color color) {
    auto it = classIdToIndex_.find(classId);
    if (it != classIdToIndex_.end()) {
        classes_[it->second].color = color;
        InvalidateAllTextures();  // Need to regenerate textures
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
}

SDL_Color TissueMapOverlay::GetDefaultTissueColor(int classId) {
    // Use modulo to cycle through palette for any class ID
    size_t idx = static_cast<size_t>(classId) % kDefaultTissuePalette.size();
    return kDefaultTissuePalette[idx];
}
