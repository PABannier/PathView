#include "PolygonIndex.h"
#include <iostream>
#include <algorithm>
#include <set>

PolygonIndex::PolygonIndex(int gridWidth, int gridHeight,
                           double slideWidth, double slideHeight)
    : gridWidth_(gridWidth)
    , gridHeight_(gridHeight) {

    // Calculate cell dimensions
    cellWidth_ = slideWidth / gridWidth;
    cellHeight_ = slideHeight / gridHeight;

    // Initialize grid
    grid_.resize(gridHeight_);
    for (auto& row : grid_) {
        row.resize(gridWidth_);
    }

    std::cout << "PolygonIndex created: " << gridWidth_ << "x" << gridHeight_
              << " grid, cell size: " << cellWidth_ << "x" << cellHeight_ << std::endl;
}

void PolygonIndex::Build(std::vector<Polygon>& polygons) {
    // Clear existing index
    Clear();

    // Insert each polygon into overlapping grid cells
    for (auto& polygon : polygons) {
        std::vector<std::pair<int, int>> cells;
        GetIntersectingCells(polygon.boundingBox, cells);

        for (const auto& cellCoord : cells) {
            int cellX = cellCoord.first;
            int cellY = cellCoord.second;
            grid_[cellY][cellX].polygons.push_back(&polygon);
        }
    }

    // Print statistics
    size_t totalEntries = 0;
    size_t maxPerCell = 0;
    size_t nonEmptyCells = 0;

    for (const auto& row : grid_) {
        for (const auto& cell : row) {
            size_t count = cell.polygons.size();
            if (count > 0) {
                ++nonEmptyCells;
                totalEntries += count;
                maxPerCell = std::max(maxPerCell, count);
            }
        }
    }

    double avgPerCell = nonEmptyCells > 0 ?
                        static_cast<double>(totalEntries) / nonEmptyCells : 0.0;

    std::cout << "Spatial index built: "
              << nonEmptyCells << " / " << (gridWidth_ * gridHeight_) << " cells occupied"
              << ", avg " << avgPerCell << " polygons/cell"
              << ", max " << maxPerCell << " polygons/cell" << std::endl;
}

std::vector<Polygon*> PolygonIndex::QueryRegion(const Rect& region) {
    // Get all cells intersecting the query region
    std::vector<std::pair<int, int>> cells;
    GetIntersectingCells(region, cells);

    // Phase 1: Use vector + sort + unique instead of set (faster for large N)
    std::vector<Polygon*> candidatePolygons;
    candidatePolygons.reserve(cells.size() * 50);  // Estimate: 50 polygons per cell

    // Collect all polygons from intersecting cells
    for (const auto& cellCoord : cells) {
        int cellX = cellCoord.first;
        int cellY = cellCoord.second;
        const GridCell& cell = grid_[cellY][cellX];

        for (Polygon* polygon : cell.polygons) {
            candidatePolygons.push_back(polygon);
        }
    }

    // Deduplicate: sort + unique is faster than set for large vectors
    std::sort(candidatePolygons.begin(), candidatePolygons.end());
    candidatePolygons.erase(
        std::unique(candidatePolygons.begin(), candidatePolygons.end()),
        candidatePolygons.end()
    );

    // Filter by actual intersection (eliminate false positives from grid quantization)
    std::vector<Polygon*> result;
    result.reserve(candidatePolygons.size());

    for (Polygon* polygon : candidatePolygons) {
        if (polygon->boundingBox.Intersects(region)) {
            result.push_back(polygon);
        }
    }

    return result;
}

void PolygonIndex::Clear() {
    for (auto& row : grid_) {
        for (auto& cell : row) {
            cell.polygons.clear();
        }
    }
}

void PolygonIndex::SlideToGridCell(double x, double y, int& outCellX, int& outCellY) const {
    outCellX = static_cast<int>(x / cellWidth_);
    outCellY = static_cast<int>(y / cellHeight_);

    // Clamp to grid bounds
    outCellX = std::max(0, std::min(outCellX, gridWidth_ - 1));
    outCellY = std::max(0, std::min(outCellY, gridHeight_ - 1));
}

void PolygonIndex::GetIntersectingCells(const Rect& bbox,
                                        std::vector<std::pair<int, int>>& outCells) const {
    outCells.clear();

    // Get grid cell range that intersects the bounding box
    int minCellX, minCellY, maxCellX, maxCellY;

    SlideToGridCell(bbox.x, bbox.y, minCellX, minCellY);
    SlideToGridCell(bbox.x + bbox.width, bbox.y + bbox.height, maxCellX, maxCellY);

    // Collect all cells in the range
    for (int cellY = minCellY; cellY <= maxCellY; ++cellY) {
        for (int cellX = minCellX; cellX <= maxCellX; ++cellX) {
            outCells.push_back({cellX, cellY});
        }
    }
}
