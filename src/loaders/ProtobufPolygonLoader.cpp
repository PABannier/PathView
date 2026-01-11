#include "ProtobufPolygonLoader.h"
#include "cell_polygons.pb.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <cmath>
#include <zlib.h>

namespace {

// Decompress zlib-compressed data
// Returns true on success, false on failure
bool DecompressZlib(const std::string& compressed, std::vector<uint8_t>& decompressed, size_t expectedSize) {
    // Check for zlib header (0x78 followed by 0x01, 0x9c, or 0xda)
    if (compressed.size() < 2) {
        return false;
    }

    uint8_t firstByte = static_cast<uint8_t>(compressed[0]);
    if (firstByte != 0x78) {
        // Not zlib compressed, treat as raw data
        decompressed.assign(
            reinterpret_cast<const uint8_t*>(compressed.data()),
            reinterpret_cast<const uint8_t*>(compressed.data()) + compressed.size()
        );
        return true;
    }

    // Initialize zlib stream
    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    stream.avail_in = static_cast<uInt>(compressed.size());
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressed.data()));

    if (inflateInit(&stream) != Z_OK) {
        std::cerr << "Failed to initialize zlib decompression" << std::endl;
        return false;
    }

    // Allocate output buffer
    decompressed.resize(expectedSize);
    stream.avail_out = static_cast<uInt>(expectedSize);
    stream.next_out = decompressed.data();

    // Decompress
    int ret = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);

    if (ret != Z_STREAM_END) {
        std::cerr << "Zlib decompression failed with code: " << ret << std::endl;
        // Try to provide more context
        if (ret == Z_DATA_ERROR) {
            std::cerr << "  -> Data error (corrupted or invalid compressed data)" << std::endl;
        } else if (ret == Z_MEM_ERROR) {
            std::cerr << "  -> Memory allocation error" << std::endl;
        } else if (ret == Z_BUF_ERROR) {
            std::cerr << "  -> Buffer too small for decompressed data" << std::endl;
        }
        return false;
    }

    // Verify we got the expected amount of data
    size_t decompressedSize = expectedSize - stream.avail_out;
    if (decompressedSize != expectedSize) {
        std::cerr << "Decompressed size mismatch: expected " << expectedSize
                  << ", got " << decompressedSize << std::endl;
        decompressed.resize(decompressedSize);
    }

    return true;
}

} // anonymous namespace

bool ProtobufPolygonLoader::Load(const std::string& filepath,
                                 std::vector<Polygon>& outPolygons,
                                 std::map<int, SDL_Color>& outClassColors,
                                 std::map<int, std::string>& outClassNames) {
    // Read file into string
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open protobuf file: " << filepath << std::endl;
        return false;
    }

    // Parse protobuf message
    DataProtoPolygon::SlideSegmentationData slideData;
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

bool ProtobufPolygonLoader::LoadWithTissue(const std::string& filepath,
                                           std::vector<Polygon>& outPolygons,
                                           std::map<int, SDL_Color>& outClassColors,
                                           std::map<int, std::string>& outClassNames,
                                           TissueMapData& outTissueData) {
    // Read file into string
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open protobuf file: " << filepath << std::endl;
        return false;
    }

    // Parse protobuf message
    DataProtoPolygon::SlideSegmentationData slideData;
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
    outTissueData.tiles.clear();
    outTissueData.classMapping.clear();

    int maxDeepZoomLevel = static_cast<int>(slideData.max_level());
    outTissueData.maxLevel = maxDeepZoomLevel;

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

    // Extract tissue class mapping from protobuf
    std::cout << "Extracting tissue class mapping..." << std::endl;
    for (const auto& pair : slideData.tissue_class_mapping()) {
        outTissueData.classMapping[pair.first] = pair.second;
        std::cout << "  Tissue class " << pair.first << " -> " << pair.second << std::endl;
    }

    // Second pass: extract polygons and tissue maps from all tiles
    outPolygons.reserve(totalMasks);
    outTissueData.tiles.reserve(slideData.tiles_size());

    int tissueMapCount = 0;

    for (int i = 0; i < slideData.tiles_size(); ++i) {
        const auto& tile = slideData.tiles(i);

        // Extract cell polygons
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

        // Extract tissue segmentation map if present
        if (tile.has_tissue_segmentation_map()) {
            const auto& tissueMap = tile.tissue_segmentation_map();

            TissueTile tissueTile;
            tissueTile.level = tile.level();
            tissueTile.tileX = static_cast<int>(tile.x());
            tissueTile.tileY = static_cast<int>(tile.y());
            tissueTile.width = tissueMap.width();
            tissueTile.height = tissueMap.height();

            // Decompress the data (handles both compressed and uncompressed)
            const std::string& data = tissueMap.data();
            size_t expectedSize = static_cast<size_t>(tissueMap.width()) * tissueMap.height();

            if (!DecompressZlib(data, tissueTile.classData, expectedSize)) {
                std::cerr << "Failed to decompress tissue map for tile at ("
                          << tile.x() << ", " << tile.y() << ")" << std::endl;
                continue;  // Skip this tile
            }

            tissueTile.texture = nullptr;
            tissueTile.textureValid = false;

            outTissueData.tiles.push_back(std::move(tissueTile));
            tissueMapCount++;
        }

        // Progress update every 10 tiles
        if ((i + 1) % 10 == 0) {
            std::cout << "  Processed " << (i + 1) << " / " << slideData.tiles_size()
                      << " tiles..." << std::endl;
        }
    }

    std::cout << "Successfully loaded " << outPolygons.size() << " polygons" << std::endl;
    std::cout << "Successfully loaded " << tissueMapCount << " tissue map tiles" << std::endl;
    std::cout << "==================================\n" << std::endl;

    return true;
}
