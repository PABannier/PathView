#include "ProtobufPolygonLoader.h"
#include <fstream>
#include <iostream>
#include <cmath>
#include <cstring>
#include <zlib.h>
#include <zstd.h>

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

// Decompress zstd-compressed data
// Returns true on success, false on failure
bool DecompressZstd(const std::string& compressed, std::vector<uint8_t>& decompressed) {
    if (compressed.empty()) {
        decompressed.clear();
        return true;
    }

    unsigned long long const decompressedSize = ZSTD_getFrameContentSize(compressed.data(), compressed.size());
    if (decompressedSize == ZSTD_CONTENTSIZE_ERROR) {
        std::cerr << "Data is not zstd compressed" << std::endl;
        return false;
    }
    if (decompressedSize == ZSTD_CONTENTSIZE_UNKNOWN) {
        std::cerr << "Zstd frame does not contain content size" << std::endl;
        return false;
    }

    decompressed.resize(static_cast<size_t>(decompressedSize));
    size_t const result = ZSTD_decompress(decompressed.data(), decompressed.size(),
                                          compressed.data(), compressed.size());
    if (ZSTD_isError(result)) {
        std::cerr << "Zstd decompression failed: " << ZSTD_getErrorName(result) << std::endl;
        return false;
    }

    decompressed.resize(result);
    return true;
}

} // anonymous namespace

bool ProtobufPolygonLoader::Load(const std::string& filepath,
                                 std::vector<Polygon>& outPolygons,
                                 std::map<int, SDL_Color>& outClassColors,
                                 std::map<int, std::string>& outClassNames) {
    // Delegate to LoadWithTissue with a throwaway tissue data container
    TissueMapData unusedTissueData;
    return LoadWithTissue(filepath, outPolygons, outClassColors, outClassNames, unusedTissueData);
}

bool ProtobufPolygonLoader::LoadWithTissue(const std::string& filepath,
                                           std::vector<Polygon>& outPolygons,
                                           std::map<int, SDL_Color>& outClassColors,
                                           std::map<int, std::string>& outClassNames,
                                           TissueMapData& outTissueData) {
    // Read entire file into string buffer
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open protobuf file: " << filepath << std::endl;
        return false;
    }

    std::string fileData((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
    file.close();

    // Try old format first (DataProtoPolygon, proto2)
    {
        DataProtoPolygon::SlideSegmentationData oldSlideData;
        if (oldSlideData.ParseFromString(fileData) && oldSlideData.tiles_size() > 0) {
            std::cout << "Detected old protobuf format (DataProtoPolygon)" << std::endl;
            return LoadOldFormatWithTissue(oldSlideData, outPolygons, outClassColors,
                                           outClassNames, outTissueData);
        }
    }

    // Try new format (histotyper_v2, proto3)
    {
        histotyper_v2::SlideSegmentationData newSlideData;
        if (newSlideData.ParseFromString(fileData) && newSlideData.tiles_size() > 0) {
            std::cout << "Detected new protobuf format (histotyper_v2)" << std::endl;
            return LoadNewFormatWithTissue(newSlideData, outPolygons, outClassColors,
                                           outClassNames, outTissueData);
        }
    }

    std::cerr << "Failed to parse protobuf file with either old or new format" << std::endl;
    return false;
}

bool ProtobufPolygonLoader::LoadOldFormatWithTissue(
        const DataProtoPolygon::SlideSegmentationData& slideData,
        std::vector<Polygon>& outPolygons,
        std::map<int, SDL_Color>& outClassColors,
        std::map<int, std::string>& outClassNames,
        TissueMapData& outTissueData) {

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

bool ProtobufPolygonLoader::LoadNewFormatWithTissue(
        const histotyper_v2::SlideSegmentationData& slideData,
        std::vector<Polygon>& outPolygons,
        std::map<int, SDL_Color>& outClassColors,
        std::map<int, std::string>& outClassNames,
        TissueMapData& outTissueData) {

    std::cout << "Slide ID: " << slideData.slide_id() << std::endl;
    std::cout << "Tiles: " << slideData.tiles_size() << std::endl;

    // Clear output containers
    outPolygons.clear();
    outClassColors.clear();
    outClassNames.clear();
    outTissueData.tiles.clear();
    outTissueData.classMapping.clear();

    int maxDeepZoomLevel = static_cast<int>(slideData.max_level());
    int level = static_cast<int>(slideData.level());
    int tileSize = static_cast<int>(slideData.tile_size());
    outTissueData.maxLevel = maxDeepZoomLevel;

    double scaleFactor = std::pow(2, maxDeepZoomLevel - level);

    // Build cell class mapping from cell_class_names (index = class_id)
    std::set<std::string> uniqueCellTypes;
    std::map<std::string, int> classMapping;
    for (int i = 0; i < slideData.cell_class_names_size(); ++i) {
        const std::string& name = slideData.cell_class_names(i);
        classMapping[name] = i;
        uniqueCellTypes.insert(name);
    }

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

    // Build tissue class mapping from tissue_class_names (index = class_id)
    std::cout << "Extracting tissue class mapping..." << std::endl;
    for (int i = 0; i < slideData.tissue_class_names_size(); ++i) {
        const std::string& name = slideData.tissue_class_names(i);
        outTissueData.classMapping[i] = name;
        std::cout << "  Tissue class " << i << " -> " << name << std::endl;
    }

    outTissueData.tiles.reserve(slideData.tiles_size());
    int tissueMapCount = 0;
    int totalCells = 0;

    for (int i = 0; i < slideData.tiles_size(); ++i) {
        const auto& tile = slideData.tiles(i);

        // Decode cells_blob: zstd decompress -> parse binary format
        if (!tile.cells_blob().empty()) {
            std::vector<uint8_t> cellsData;
            if (!DecompressZstd(tile.cells_blob(), cellsData)) {
                std::cerr << "Failed to decompress cells blob for tile at ("
                          << tile.x() << ", " << tile.y() << ")" << std::endl;
            } else if (cellsData.size() >= 2) {
                // Binary format: uint16 n_cells, then per cell:
                //   uint8 class_id, uint8 confidence, int16 centroid_x, int16 centroid_y,
                //   uint8 n_vertices, then n_vertices pairs of int16 (x, y)
                const uint8_t* ptr = cellsData.data();
                const uint8_t* end = ptr + cellsData.size();

                uint16_t nCells;
                std::memcpy(&nCells, ptr, sizeof(uint16_t));
                ptr += sizeof(uint16_t);

                for (uint16_t c = 0; c < nCells && ptr < end; ++c) {
                    if (ptr + 7 > end) break;  // minimum: class_id(1) + confidence(1) + cx(2) + cy(2) + n_verts(1)

                    uint8_t classId = *ptr++;
                    ptr++;  // skip confidence
                    ptr += 4;  // skip centroid_x(2) + centroid_y(2)

                    uint8_t nVertices = *ptr++;

                    size_t vertexBytes = static_cast<size_t>(nVertices) * 2 * sizeof(int16_t);
                    if (ptr + vertexBytes > end) break;

                    if (nVertices < 3) {
                        ptr += vertexBytes;
                        continue;
                    }

                    Polygon polygon;
                    polygon.classId = classId;
                    polygon.vertices.reserve(nVertices);

                    double tileOriginX = static_cast<double>(tile.x()) * tileSize;
                    double tileOriginY = static_cast<double>(tile.y()) * tileSize;

                    for (uint8_t v = 0; v < nVertices; ++v) {
                        int16_t vx, vy;
                        std::memcpy(&vx, ptr, sizeof(int16_t));
                        ptr += sizeof(int16_t);
                        std::memcpy(&vy, ptr, sizeof(int16_t));
                        ptr += sizeof(int16_t);

                        polygon.vertices.emplace_back(
                            (static_cast<double>(vx) + tileOriginX) * scaleFactor,
                            (static_cast<double>(vy) + tileOriginY) * scaleFactor
                        );
                    }

                    polygon.ComputeBoundingBox();
                    outPolygons.push_back(std::move(polygon));
                    totalCells++;
                }
            }
        }

        // Decode tissue_blob: zstd decompress -> parse header + class data
        if (!tile.tissue_blob().empty()) {
            std::vector<uint8_t> tissueData;
            if (!DecompressZstd(tile.tissue_blob(), tissueData)) {
                std::cerr << "Failed to decompress tissue blob for tile at ("
                          << tile.x() << ", " << tile.y() << ")" << std::endl;
            } else if (tissueData.size() >= 4) {
                // Binary format: uint16 width, uint16 height, then width*height uint8 class values
                const uint8_t* ptr = tissueData.data();

                uint16_t width, height;
                std::memcpy(&width, ptr, sizeof(uint16_t));
                ptr += sizeof(uint16_t);
                std::memcpy(&height, ptr, sizeof(uint16_t));
                ptr += sizeof(uint16_t);

                size_t expectedPixels = static_cast<size_t>(width) * height;
                size_t remaining = tissueData.size() - 4;

                if (remaining >= expectedPixels) {
                    TissueTile tissueTile;
                    tissueTile.level = level;
                    tissueTile.tileX = static_cast<int>(tile.x());
                    tissueTile.tileY = static_cast<int>(tile.y());
                    tissueTile.width = width;
                    tissueTile.height = height;
                    tissueTile.classData.assign(ptr, ptr + expectedPixels);
                    tissueTile.texture = nullptr;
                    tissueTile.textureValid = false;

                    outTissueData.tiles.push_back(std::move(tissueTile));
                    tissueMapCount++;
                } else {
                    std::cerr << "Tissue blob data too short for tile at ("
                              << tile.x() << ", " << tile.y() << "): expected "
                              << expectedPixels << " bytes, got " << remaining << std::endl;
                }
            }
        }

        // Progress update every 10 tiles
        if ((i + 1) % 10 == 0) {
            std::cout << "  Processed " << (i + 1) << " / " << slideData.tiles_size()
                      << " tiles..." << std::endl;
        }
    }

    std::cout << "Total polygons: " << totalCells << std::endl;
    std::cout << "Successfully loaded " << outPolygons.size() << " polygons" << std::endl;
    std::cout << "Successfully loaded " << tissueMapCount << " tissue map tiles" << std::endl;
    std::cout << "==================================\n" << std::endl;

    return true;
}
