#pragma once

#include "PolygonOverlay.h"
#include <vector>

/**
 * Spatial index for efficient polygon queries
 * Uses a simple grid-based structure for O(k) lookups
 * where k is the average number of polygons per cell
 */
class PolygonIndex {
public:
    /**
     * Constructor
     * @param gridWidth Number of grid cells horizontally
     * @param gridHeight Number of grid cells vertically
     * @param slideWidth Width of the slide in pixels (level 0)
     * @param slideHeight Height of the slide in pixels (level 0)
     */
    PolygonIndex(int gridWidth, int gridHeight, double slideWidth, double slideHeight);

    /**
     * Build the spatial index from a vector of polygons
     * @param polygons Vector of polygons to index
     */
    void Build(std::vector<Polygon>& polygons);

    /**
     * Query polygons that intersect a given region
     * @param region The bounding rectangle to query
     * @return Vector of pointers to polygons intersecting the region
     */
    std::vector<Polygon*> QueryRegion(const Rect& region);

    /**
     * Clear the index
     */
    void Clear();

private:
    struct GridCell {
        std::vector<Polygon*> polygons;
    };

    std::vector<std::vector<GridCell>> grid_;
    int gridWidth_;
    int gridHeight_;
    double cellWidth_;
    double cellHeight_;
    double slideWidth_;
    double slideHeight_;

    /**
     * Convert slide coordinates to grid cell indices
     * @param x Slide X coordinate
     * @param y Slide Y coordinate
     * @param outCellX Output grid cell X index
     * @param outCellY Output grid cell Y index
     */
    void SlideToGridCell(double x, double y, int& outCellX, int& outCellY) const;

    /**
     * Get all grid cells that intersect with a given bounding box
     * @param bbox Bounding box in slide coordinates
     * @param outCells Output vector of (cellX, cellY) pairs
     */
    void GetIntersectingCells(const Rect& bbox,
                              std::vector<std::pair<int, int>>& outCells) const;
};
