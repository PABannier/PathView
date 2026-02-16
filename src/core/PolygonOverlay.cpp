#include "PolygonOverlay.h"
#include "PolygonLoader.h"
#include "PolygonLoaderFactory.h"
#include "PolygonIndex.h"
#include "PolygonTriangulator.h"
#include "PolygonColorPalette.h"
#include "Viewport.h"
#include <iostream>
#include <set>
#include <algorithm>

PolygonOverlay::PolygonOverlay(SDL_Renderer* renderer)
    : renderer_(renderer)
    , visible_(false)  // Start hidden
    , opacity_(0.5f)   // 50% opacity by default
    , slideWidth_(0)
    , slideHeight_(0) {
}

PolygonOverlay::~PolygonOverlay() {
}

void PolygonOverlay::SetSlideDimensions(double width, double height) {
    slideWidth_ = width;
    slideHeight_ = height;

    // Rebuild spatial index if polygons are already loaded
    BuildSpatialIndex();
}

bool PolygonOverlay::LoadPolygons(const std::string& filepath) {
    std::cout << "\n=== Loading Polygons ===" << std::endl;
    std::cout << "File: " << filepath << std::endl;

    // Select the polygon loader based on the file extension
    std::unique_ptr<PolygonLoader> polygonLoader = PolygonLoaderFactory::CreateLoader(filepath);
    if (!polygonLoader) {
        std::cerr << "Could not find a loader to load the polygon data." << std::endl;
        return false;
    }

    // Load polygons
    std::map<int, SDL_Color> loadedColors;
    std::map<int, std::string> loadedClassNames;
    if (!polygonLoader->Load(filepath, polygons_, loadedColors, loadedClassNames)) {
        return false;
    }

    // Store class names
    classNames_ = loadedClassNames;

    // Use loaded colors or initialize defaults
    if (!loadedColors.empty()) {
        classColors_ = loadedColors;
    } else {
        InitializeDefaultColors();
    }

    // Build list of class IDs
    classIds_.clear();
    for (const auto& pair : classColors_) {
        classIds_.push_back(pair.first);
    }

    // Compute per-class counts
    classCounts_.clear();
    for (const auto& polygon : polygons_) {
        classCounts_[polygon.classId]++;
    }

    // Build spatial index if we have slide dimensions
    BuildSpatialIndex();

    std::cout << "Polygon overlay ready with " << polygons_.size() << " polygons" << std::endl;
    std::cout << "Classes: " << classIds_.size() << std::endl;
    std::cout << "===================" << std::endl;

    return true;
}

void PolygonOverlay::SetPolygonData(std::vector<Polygon>&& polygons,
                                     std::map<int, SDL_Color>&& colors,
                                     std::map<int, std::string>&& classNames) {
    std::cout << "\n=== Setting Polygon Data ===" << std::endl;
    std::cout << "Polygons: " << polygons.size() << std::endl;

    // Clear existing data
    Clear();

    // Move data
    polygons_ = std::move(polygons);
    classNames_ = std::move(classNames);

    // Use provided colors or initialize defaults
    if (!colors.empty()) {
        classColors_ = std::move(colors);
    } else {
        InitializeDefaultColors();
    }

    // Build list of class IDs
    classIds_.clear();
    for (const auto& pair : classColors_) {
        classIds_.push_back(pair.first);
    }

    // Compute per-class counts
    classCounts_.clear();
    for (const auto& polygon : polygons_) {
        classCounts_[polygon.classId]++;
    }

    // Build spatial index if we have slide dimensions
    BuildSpatialIndex();

    std::cout << "Polygon overlay ready with " << polygons_.size() << " polygons" << std::endl;
    std::cout << "Classes: " << classIds_.size() << std::endl;
    std::cout << "===================" << std::endl;
}

void PolygonOverlay::Render(const Viewport& viewport) {
    if (!visible_ || polygons_.empty()) {
        return;
    }

    // Get visible region for culling
    Rect visibleRegion = viewport.GetVisibleRegion();

#if !defined(NDEBUG)
    // Debug output (print once per 60 frames to avoid spam)
    static int frameCount = 0;
    if (frameCount++ % 60 == 0) {
        std::cout << "[PolygonOverlay] Rendering - Total polygons: " << polygons_.size()
                  << ", Visible region: [" << visibleRegion.x << ", " << visibleRegion.y
                  << ", " << visibleRegion.width << ", " << visibleRegion.height << "]" << std::endl;
        if (!polygons_.empty()) {
            std::cout << "[PolygonOverlay] Sample polygon bbox: ["
                      << polygons_[0].boundingBox.x << ", " << polygons_[0].boundingBox.y
                      << ", " << polygons_[0].boundingBox.width << ", " << polygons_[0].boundingBox.height << "]" << std::endl;
        }
    }
#endif

    // Query spatial index for visible polygons
    std::vector<Polygon*> visiblePolygons;
    if (spatialIndex_) {
        visiblePolygons = spatialIndex_->QueryRegion(visibleRegion);
    } else {
        // Fallback: brute force culling (less efficient)
        for (auto& polygon : polygons_) {
            if (polygon.boundingBox.Intersects(visibleRegion)) {
                visiblePolygons.push_back(&polygon);
            }
        }
    }

    // Phase 1: Size-based culling to skip tiny polygons
    const double zoom = viewport.GetZoom();
    std::vector<Polygon*> lodFilteredPolygons;
    lodFilteredPolygons.reserve(visiblePolygons.size());

    for (auto* polygon : visiblePolygons) {
        double screenSize = std::max(
            polygon->boundingBox.width * zoom,
            polygon->boundingBox.height * zoom
        );

        if (screenSize >= minScreenSizePixels_) {
            lodFilteredPolygons.push_back(polygon);
        }
    }

    visiblePolygons = std::move(lodFilteredPolygons);

#if !defined(NDEBUG)
    if (frameCount % 60 == 1) {
        std::cout << "[PolygonOverlay] Visible polygons: " << visiblePolygons.size() << std::endl;
    }
#endif

    if (visiblePolygons.empty()) {
        return;
    }

    // Group polygons by class for batching (filter by class visibility)
    std::map<int, std::vector<Polygon*>> batchesByClass;
    for (auto* polygon : visiblePolygons) {
        if (IsClassVisible(polygon->classId)) {
            batchesByClass[polygon->classId].push_back(polygon);
        }
    }

    // Set blend mode for opacity
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

    // Render each class batch
    for (const auto& pair : batchesByClass) {
        int classId = pair.first;
        const std::vector<Polygon*>& batch = pair.second;
        RenderPolygonBatch(batch, classId, viewport);
    }
}

void PolygonOverlay::Clear() {
    polygons_.clear();
    classColors_.clear();
    classNames_.clear();
    classVisibility_.clear();
    classIds_.clear();
    classCounts_.clear();
    spatialIndex_.reset();
    visible_ = false;
}

void PolygonOverlay::RenderPolygonBatch(const std::vector<Polygon*>& batch,
                                        int classId,
                                        const Viewport& viewport) {
    // Phase 2: Group polygons by LOD level
    std::vector<Polygon*> pointPolygons;
    std::vector<Polygon*> boxPolygons;
    std::vector<Polygon*> simplifiedPolygons;
    std::vector<Polygon*> fullPolygons;

    for (auto* polygon : batch) {
        LODLevel lod = DeterminePolygonLOD(polygon, viewport);
        switch (lod) {
            case LODLevel::SKIP: break;
            case LODLevel::POINT: pointPolygons.push_back(polygon); break;
            case LODLevel::BOX: boxPolygons.push_back(polygon); break;
            case LODLevel::SIMPLIFIED: simplifiedPolygons.push_back(polygon); break;
            case LODLevel::FULL: fullPolygons.push_back(polygon); break;
        }
    }

    SDL_Color color = GetClassColor(classId);
    uint8_t alpha = static_cast<uint8_t>(opacity_ * 255);

    // Render each LOD group with optimized method
    if (!pointPolygons.empty()) RenderAsPoints(pointPolygons, color, alpha, viewport);
    if (!boxPolygons.empty()) RenderAsBoxes(boxPolygons, color, alpha, viewport);
    if (!simplifiedPolygons.empty()) RenderFull(simplifiedPolygons, color, alpha, viewport); // Simplified = full for now
    if (!fullPolygons.empty()) RenderFull(fullPolygons, color, alpha, viewport);
}

// Phase 2: Determine appropriate LOD level based on screen size
LODLevel PolygonOverlay::DeterminePolygonLOD(
    const Polygon* polygon,
    const Viewport& viewport) const {

    const double zoom = viewport.GetZoom();
    const double screenSize = std::max(
        polygon->boundingBox.width * zoom,
        polygon->boundingBox.height * zoom
    );

    if (screenSize < minScreenSizePixels_) return LODLevel::SKIP;
    if (screenSize < lodPointThreshold_) return LODLevel::POINT;
    if (screenSize < lodBoxThreshold_) return LODLevel::BOX;
    if (screenSize < lodSimplifiedThreshold_) return LODLevel::SIMPLIFIED;
    return LODLevel::FULL;
}

// Phase 2.4.3: Render polygons at full geometric detail
void PolygonOverlay::RenderFull(
    const std::vector<Polygon*>& polygons,
    SDL_Color color, uint8_t alpha,
    const Viewport& viewport) {

    std::vector<SDL_Vertex> vertices;
    std::vector<int> indices;
    vertices.reserve(polygons.size() * 20);
    indices.reserve(polygons.size() * 54);

    for (auto* polygon : polygons) {
        if (polygon->vertices.size() < 3) continue;

        // Triangulate if not cached
        if (polygon->triangleIndices.empty()) {
            polygon->triangleIndices = PolygonTriangulator::Triangulate(polygon->vertices);
        }
        if (polygon->triangleIndices.empty()) continue;

        int baseIndex = static_cast<int>(vertices.size());

        // Transform vertices to screen space
        for (const auto& vertex : polygon->vertices) {
            Vec2 screenPos = viewport.SlideToScreen(vertex);
            SDL_Vertex sdlVertex;
            sdlVertex.position = {static_cast<float>(screenPos.x), static_cast<float>(screenPos.y)};
            sdlVertex.color = {color.r, color.g, color.b, alpha};
            sdlVertex.tex_coord = {0.0f, 0.0f};
            vertices.push_back(sdlVertex);
        }

        // Add indices with offset
        for (int idx : polygon->triangleIndices) {
            indices.push_back(baseIndex + idx);
        }
    }

    if (!vertices.empty() && !indices.empty()) {
        SDL_RenderGeometry(renderer_, nullptr,
            vertices.data(), static_cast<int>(vertices.size()),
            indices.data(), static_cast<int>(indices.size()));
    }
}

// Phase 2.4.1: Render polygons as single points (ultra-fast for tiny polygons)
void PolygonOverlay::RenderAsPoints(
    const std::vector<Polygon*>& polygons,
    SDL_Color color, uint8_t alpha,
    const Viewport& viewport) {

    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, alpha);

    for (auto* polygon : polygons) {
        // Render center point
        Vec2 center(
            polygon->boundingBox.x + polygon->boundingBox.width * 0.5,
            polygon->boundingBox.y + polygon->boundingBox.height * 0.5
        );
        Vec2 screenPos = viewport.SlideToScreen(center);
        SDL_RenderDrawPoint(renderer_,
            static_cast<int>(screenPos.x),
            static_cast<int>(screenPos.y));
    }
}

// Phase 2.4.2: Render polygons as bounding box rectangles (fast for small polygons)
void PolygonOverlay::RenderAsBoxes(
    const std::vector<Polygon*>& polygons,
    SDL_Color color, uint8_t alpha,
    const Viewport& viewport) {

    std::vector<SDL_Vertex> vertices;
    vertices.reserve(polygons.size() * 6);  // 2 triangles = 6 vertices per box

    for (auto* polygon : polygons) {
        const Rect& bbox = polygon->boundingBox;

        // Transform 4 corners to screen space
        Vec2 tl = viewport.SlideToScreen(Vec2(bbox.x, bbox.y));
        Vec2 tr = viewport.SlideToScreen(Vec2(bbox.x + bbox.width, bbox.y));
        Vec2 bl = viewport.SlideToScreen(Vec2(bbox.x, bbox.y + bbox.height));
        Vec2 br = viewport.SlideToScreen(Vec2(bbox.x + bbox.width, bbox.y + bbox.height));

        SDL_Vertex v0 = {{(float)tl.x, (float)tl.y}, {color.r, color.g, color.b, alpha}, {0,0}};
        SDL_Vertex v1 = {{(float)tr.x, (float)tr.y}, {color.r, color.g, color.b, alpha}, {0,0}};
        SDL_Vertex v2 = {{(float)bl.x, (float)bl.y}, {color.r, color.g, color.b, alpha}, {0,0}};
        SDL_Vertex v3 = {{(float)br.x, (float)br.y}, {color.r, color.g, color.b, alpha}, {0,0}};

        // Triangle 1: TL, TR, BL
        vertices.push_back(v0);
        vertices.push_back(v1);
        vertices.push_back(v2);

        // Triangle 2: TR, BR, BL
        vertices.push_back(v1);
        vertices.push_back(v3);
        vertices.push_back(v2);
    }

    if (!vertices.empty()) {
        SDL_RenderGeometry(renderer_, nullptr,
            vertices.data(), static_cast<int>(vertices.size()),
            nullptr, 0);
    }
}

void PolygonOverlay::SetOpacity(float opacity) {
    opacity_ = std::max(0.0f, std::min(1.0f, opacity));
}

void PolygonOverlay::SetClassColor(int classId, SDL_Color color) {
    classColors_[classId] = color;
}

SDL_Color PolygonOverlay::GetClassColor(int classId) const {
    auto it = classColors_.find(classId);
    if (it != classColors_.end()) {
        return it->second;
    }

    // Return default color if not found
    return pathview::polygons::kDefaultPalette[
        classId % pathview::polygons::kDefaultPalette.size()
    ];
}

std::string PolygonOverlay::GetClassName(int classId) const {
    auto it = classNames_.find(classId);
    if (it != classNames_.end()) {
        return it->second;
    }

    // Fallback to generic name if not found
    return "Class " + std::to_string(classId);
}

int PolygonOverlay::GetClassCount(int classId) const {
    auto it = classCounts_.find(classId);
    if (it != classCounts_.end()) {
        return it->second;
    }
    return 0;
}

void PolygonOverlay::SetClassVisible(int classId, bool visible) {
    classVisibility_[classId] = visible;
}

bool PolygonOverlay::IsClassVisible(int classId) const {
    auto it = classVisibility_.find(classId);
    // Default to visible if not explicitly set
    return it == classVisibility_.end() || it->second;
}

void PolygonOverlay::SetAllClassesVisible(bool visible) {
    for (int classId : classIds_) {
        classVisibility_[classId] = visible;
    }
}

void PolygonOverlay::InitializeDefaultColors() {
    classColors_.clear();

    // Assign default colors to classes found in polygons
    std::set<int> uniqueClasses;
    for (const auto& polygon : polygons_) {
        uniqueClasses.insert(polygon.classId);
    }

    size_t colorIndex = 0;
    for (int classId : uniqueClasses) {
        classColors_[classId] =
            pathview::polygons::kDefaultPalette[colorIndex % pathview::polygons::kDefaultPalette.size()];
        ++colorIndex;
    }
}

void PolygonOverlay::BuildSpatialIndex() {
    // Clear any existing index if we cannot build a new one yet
    if (slideWidth_ <= 0.0 || slideHeight_ <= 0.0 || polygons_.empty()) {
        spatialIndex_.reset();
        return;
    }

    std::cout << "Building spatial index..." << std::endl;

    // Create spatial index with a fixed grid for now
    spatialIndex_ = std::make_unique<PolygonIndex>(DEFAULT_GRID_SIZE, DEFAULT_GRID_SIZE, slideWidth_, slideHeight_);
    spatialIndex_->Build(polygons_);

    std::cout << "Spatial index built successfully" << std::endl;
}
