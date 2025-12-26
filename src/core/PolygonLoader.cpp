#include "PolygonLoader.h"
#include "cell_polygons.pb.h"
#include <fstream>
#include <iostream>
#include <set>
#include <cmath>

bool PolygonLoader::Load(const std::string& filepath,
                         std::vector<Polygon>& outPolygons,
                         std::map<int, SDL_Color>& outClassColors,
                         std::map<int, std::string>& outClassNames) {
    std::cout << "\n=== Loading Protobuf Polygon Data ===" << std::endl;
    std::cout << "File: " << filepath << std::endl;

    // Read file into string
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open protobuf file: " << filepath << std::endl;
        return false;
    }

    // Parse protobuf message
    histowmics::SlideSegmentationData slideData;
    if (!slideData.ParseFromIstream(&file)) {
        std::cerr << "Failed to parse protobuf message" << std::endl;
        return false;
    }

    std::cout << "Slide ID: " << slideData.slide_id() << std::endl;
    std::cout << "Tiles: " << slideData.tiles_size() << std::endl;

    // Clear output containers
    outPolygons.clear();
    outClassColors.clear();
    outClassNames.clear();

    int maxDeepZoomLevel = static_cast<int>(slideData.max_level());

    // First pass: collect unique cell types
    std::set<std::string> uniqueCellTypes;
    int totalMasks = 0;

    for (int i = 0; i < slideData.tiles_size(); ++i) {
        const auto& tile = slideData.tiles(i);
        totalMasks += tile.masks_size();

        for (int j = 0; j < tile.masks_size(); ++j) {
            const auto& mask = tile.masks(j);
            uniqueCellTypes.insert(mask.cell_type());
        }
    }

    std::cout << "Total polygons: " << totalMasks << std::endl;
    std::cout << "Unique cell types: " << uniqueCellTypes.size() << std::endl;

    // Build class name to ID mapping
    std::map<std::string, int> classMapping;
    BuildClassMapping(uniqueCellTypes, classMapping);

    // Build reverse mapping (ID to name) for output
    for (const auto& pair : classMapping) {
        outClassNames[pair.second] = pair.first;
    }

    // Print mapping
    for (const auto& pair : classMapping) {
        std::cout << "  " << pair.first << " -> Class " << pair.second << std::endl;
    }

    // Generate colors based on class names
    std::cout << "Assigning colors to cell types:" << std::endl;
    GenerateColorsFromClassNames(classMapping, outClassColors);

    // Second pass: extract polygons from all tiles
    outPolygons.reserve(totalMasks);

    for (int i = 0; i < slideData.tiles_size(); ++i) {
        const auto& tile = slideData.tiles(i);

        for (int j = 0; j < tile.masks_size(); ++j) {
            const auto& mask = tile.masks(j);

            // Skip if no coordinates
            if (mask.coordinates_size() < 3) {
                continue;
            }

            double scaleFactor = std::pow(2, maxDeepZoomLevel - tile.level());

            Polygon polygon;
            polygon.classId = classMapping[mask.cell_type()];

            // Convert Point (float) to Vec2 (double)
            polygon.vertices.reserve(mask.coordinates_size());
            for (int k = 0; k < mask.coordinates_size(); ++k) {
                const auto& point = mask.coordinates(k);
                polygon.vertices.emplace_back(
                    static_cast<double>((point.x() + tile.x() * tile.width()) * scaleFactor),
                    static_cast<double>((point.y() + tile.y() * tile.height()) * scaleFactor)
                );
            }

            // Compute bounding box
            polygon.ComputeBoundingBox();

            outPolygons.push_back(std::move(polygon));
        }

        // Progress update every 10 tiles
        if ((i + 1) % 10 == 0) {
            std::cout << "  Processed " << (i + 1) << " / " << slideData.tiles_size()
                      << " tiles..." << std::endl;
        }
    }

    std::cout << "Successfully loaded " << outPolygons.size() << " polygons" << std::endl;
    std::cout << "==================================\n" << std::endl;

    return true;
}

void PolygonLoader::BuildClassMapping(const std::set<std::string>& cellTypes,
                                      std::map<std::string, int>& outMapping) {
    outMapping.clear();
    int classId = 0;

    for (const auto& cellType : cellTypes) {
        outMapping[cellType] = classId++;
    }
}

// Cell type color map - maps cell type names to RGB colors
static const std::map<std::string, SDL_Color> CELL_TYPE_COLORS = {
    {"Background",         {0, 0, 0, 255}},         // Black
    {"Cancer cell",        {230, 0, 0, 255}},       // Bright Red
    {"Lymphocytes",        {0, 150, 0, 255}},       // Medium Green
    {"Fibroblasts",        {0, 0, 230, 255}},       // Bright Blue
    {"Plasmocytes",        {255, 255, 0, 255}},     // Bright Yellow
    {"Macrophages",        {153, 51, 255, 255}},    // Purple
    {"Eosinophils",        {255, 102, 178, 255}},   // Pink
    {"Muscle Cell",        {102, 51, 0, 255}},      // Brown
    {"Neutrophils",        {255, 153, 51, 255}},    // Orange
    {"Endothelial Cell",   {51, 204, 204, 255}},    // Light Cyan
    {"Red blood cell",     {128, 0, 0, 255}},       // Dark Red
    {"Epithelial",         {0, 102, 0, 255}},       // Dark Green
    {"Mitotic Figures",    {102, 255, 102, 255}},   // Light Green
    {"Apoptotic Body",     {102, 204, 255, 255}},   // Light Blue
    {"Minor Stromal Cell", {255, 153, 102, 255}},   // Light Orange
    {"Other",              {255, 255, 255, 255}}    // White
};

// Fallback colors for unmapped cell types
static const SDL_Color FALLBACK_COLORS[] = {
    {255, 0, 0, 255},      // Red
    {0, 255, 0, 255},      // Green
    {0, 0, 255, 255},      // Blue
    {255, 255, 0, 255},    // Yellow
    {255, 0, 255, 255},    // Magenta
    {0, 255, 255, 255},    // Cyan
    {255, 128, 0, 255},    // Orange
    {128, 0, 255, 255},    // Purple
    {255, 192, 203, 255},  // Pink
    {128, 128, 128, 255}   // Gray
};

static constexpr size_t NUM_FALLBACK_COLORS = sizeof(FALLBACK_COLORS) / sizeof(FALLBACK_COLORS[0]);

void PolygonLoader::GenerateDefaultColors(int numClasses,
                                          std::map<int, SDL_Color>& outColors) {
    // This function is now a fallback - primary coloring uses class names
    outColors.clear();
    for (int i = 0; i < numClasses; ++i) {
        outColors[i] = FALLBACK_COLORS[i % NUM_FALLBACK_COLORS];
    }
}

void PolygonLoader::GenerateColorsFromClassNames(
    const std::map<std::string, int>& classMapping,
    std::map<int, SDL_Color>& outColors) {
    
    outColors.clear();
    int fallbackIndex = 0;
    
    for (const auto& pair : classMapping) {
        const std::string& className = pair.first;
        int classId = pair.second;
        
        // Look up color in the cell type color map
        auto colorIt = CELL_TYPE_COLORS.find(className);
        if (colorIt != CELL_TYPE_COLORS.end()) {
            outColors[classId] = colorIt->second;
            std::cout << "  Color for '" << className << "' (ID " << classId << "): "
                      << "RGB(" << (int)colorIt->second.r << ", " 
                      << (int)colorIt->second.g << ", " 
                      << (int)colorIt->second.b << ")" << std::endl;
        } else {
            // Use fallback color for unknown cell types
            outColors[classId] = FALLBACK_COLORS[fallbackIndex % NUM_FALLBACK_COLORS];
            std::cout << "  Unknown cell type '" << className << "' (ID " << classId 
                      << "), using fallback color" << std::endl;
            ++fallbackIndex;
        }
    }
}
