#pragma once

#include "PolygonOverlay.h"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <SDL2/SDL.h>

/**
 * Protocol Buffer Polygon File Loader
 *
 * Loads polygon data from protobuf-serialized SlideSegmentationData files.
 * The file format uses the histowmics.SlideSegmentationData message type.
 *
 * Cell types (strings) are automatically mapped to integer class IDs.
 */
class PolygonLoader {
public:
    /**
     * Load polygons from protobuf file
     * @param filepath Path to .pb or .protobuf file
     * @param outPolygons Output vector of polygons
     * @param outClassColors Output map of class ID to color
     * @return true if successful, false otherwise
     */
    static bool Load(const std::string& filepath,
                     std::vector<Polygon>& outPolygons,
                     std::map<int, SDL_Color>& outClassColors);

private:
    /**
     * Build class name to ID mapping from unique cell types
     * @param cellTypes Set of unique cell type strings
     * @param outMapping Output map of class name to ID
     */
    static void BuildClassMapping(const std::set<std::string>& cellTypes,
                                  std::map<std::string, int>& outMapping);

    /**
     * Generate default colors for classes
     * @param numClasses Number of classes
     * @param outColors Output map of class ID to color
     */
    static void GenerateDefaultColors(int numClasses,
                                      std::map<int, SDL_Color>& outColors);
};
