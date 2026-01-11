#pragma once

#include "PolygonLoader.h"
#include "PolygonOverlay.h"
#include "TissueMapOverlay.h"
#include <string>
#include <vector>
#include <map>
#include <SDL2/SDL.h>

/**
 * Container for loaded tissue segmentation map data
 */
struct TissueMapData {
    std::vector<TissueTile> tiles;
    std::map<int, std::string> classMapping;
    int maxLevel;

    TissueMapData() : maxLevel(0) {}
};

/**
 * Protocol Buffer Polygon File Loader
 *
 * Loads polygon data from protobuf-serialized SlideSegmentationData files.
 * The file format uses the histowmics.SlideSegmentationData message type.
 *
 * Cell types (strings) are automatically mapped to integer class IDs.
 */
class ProtobufPolygonLoader: public PolygonLoader {
public:
    /**
     * Load polygons from protobuf file
     * @param filepath Path to .pb or .protobuf file
     * @param outPolygons Output vector of polygons
     * @param outClassColors Output map of class ID to color
     * @param outClassNames Output map of class ID to class name
     * @return true if successful, false otherwise
     */
    bool Load(const std::string& filepath,
                    std::vector<Polygon>& outPolygons,
                    std::map<int, SDL_Color>& outClassColors,
                    std::map<int, std::string>& outClassNames) override;

    /**
     * Load both polygons and tissue segmentation map from protobuf file
     * @param filepath Path to .pb or .protobuf file
     * @param outPolygons Output vector of polygons
     * @param outClassColors Output map of class ID to color
     * @param outClassNames Output map of class ID to class name
     * @param outTissueData Output tissue segmentation map data
     * @return true if successful, false otherwise
     */
    bool LoadWithTissue(const std::string& filepath,
                        std::vector<Polygon>& outPolygons,
                        std::map<int, SDL_Color>& outClassColors,
                        std::map<int, std::string>& outClassNames,
                        TissueMapData& outTissueData);
};
