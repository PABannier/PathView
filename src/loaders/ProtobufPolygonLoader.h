#pragma once

#include "PolygonLoader.h"
#include "PolygonOverlay.h"
#include "TissueMapOverlay.h"
#include "cell_polygons.pb.h"
#include "new_cell_masks.pb.h"
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
 * Supports both the old format (DataProtoPolygon, proto2) and the new format
 * (histotyper_v2, proto3). Format detection is automatic with fallback.
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

private:
    /** Load old format (DataProtoPolygon) from pre-parsed data */
    bool LoadOldFormatWithTissue(const DataProtoPolygon::SlideSegmentationData& slideData,
                                 std::vector<Polygon>& outPolygons,
                                 std::map<int, SDL_Color>& outClassColors,
                                 std::map<int, std::string>& outClassNames,
                                 TissueMapData& outTissueData);

    /** Load new format (histotyper_v2) from pre-parsed data */
    bool LoadNewFormatWithTissue(const histotyper_v2::SlideSegmentationData& slideData,
                                 std::vector<Polygon>& outPolygons,
                                 std::map<int, SDL_Color>& outClassColors,
                                 std::map<int, std::string>& outClassNames,
                                 TissueMapData& outTissueData);
};
