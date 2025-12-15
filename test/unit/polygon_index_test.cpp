// PolygonIndex Unit Tests
// Tests for spatial indexing and region queries
// Critical for efficient polygon rendering

#include <gtest/gtest.h>
#include "PolygonIndex.h"
#include "PolygonOverlay.h"
#include <vector>
#include <algorithm>

// ============================================================================
// Test Fixture
// ============================================================================

class PolygonIndexTest : public ::testing::Test {
protected:
    std::vector<Polygon> polygons;
    static constexpr double SLIDE_WIDTH = 10000.0;
    static constexpr double SLIDE_HEIGHT = 8000.0;
    static constexpr int GRID_SIZE = 100;  // 100x100 grid

    void SetUp() override {
        polygons.clear();
    }

    // Helper to create a rectangular polygon at specific position
    Polygon CreateRectPolygon(double x, double y, double width, double height, int classId = 0) {
        Polygon p;
        p.classId = classId;
        p.vertices = {
            Vec2(x, y),
            Vec2(x + width, y),
            Vec2(x + width, y + height),
            Vec2(x, y + height)
        };
        p.ComputeBoundingBox();
        return p;
    }

    // Helper to create a triangular polygon
    Polygon CreateTrianglePolygon(double x1, double y1, double x2, double y2, double x3, double y3) {
        Polygon p;
        p.classId = 0;
        p.vertices = {Vec2(x1, y1), Vec2(x2, y2), Vec2(x3, y3)};
        p.ComputeBoundingBox();
        return p;
    }
};

// ============================================================================
// Constructor and Initialization Tests
// ============================================================================

TEST_F(PolygonIndexTest, Constructor_ValidParameters_Succeeds) {
    EXPECT_NO_THROW({
        PolygonIndex index(GRID_SIZE, GRID_SIZE, SLIDE_WIDTH, SLIDE_HEIGHT);
    });
}

TEST_F(PolygonIndexTest, Build_EmptyPolygonList_Succeeds) {
    PolygonIndex index(GRID_SIZE, GRID_SIZE, SLIDE_WIDTH, SLIDE_HEIGHT);

    EXPECT_NO_THROW({
        index.Build(polygons);
    });
}

// ============================================================================
// Basic Query Tests
// ============================================================================

TEST_F(PolygonIndexTest, QueryRegion_EmptyIndex_ReturnsEmpty) {
    PolygonIndex index(GRID_SIZE, GRID_SIZE, SLIDE_WIDTH, SLIDE_HEIGHT);
    index.Build(polygons);

    Rect query_region(0, 0, 1000, 1000);
    std::vector<Polygon*> results = index.QueryRegion(query_region);

    EXPECT_EQ(results.size(), 0);
}

TEST_F(PolygonIndexTest, QueryRegion_ContainsPolygon_ReturnsPolygon) {
    polygons.push_back(CreateRectPolygon(100, 100, 50, 50));  // 100-150, 100-150

    PolygonIndex index(GRID_SIZE, GRID_SIZE, SLIDE_WIDTH, SLIDE_HEIGHT);
    index.Build(polygons);

    // Query region that contains the polygon
    Rect query_region(90, 90, 70, 70);  // 90-160, 90-160
    std::vector<Polygon*> results = index.QueryRegion(query_region);

    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0]->classId, 0);
}

TEST_F(PolygonIndexTest, QueryRegion_NoOverlap_ReturnsEmpty) {
    polygons.push_back(CreateRectPolygon(100, 100, 50, 50));

    PolygonIndex index(GRID_SIZE, GRID_SIZE, SLIDE_WIDTH, SLIDE_HEIGHT);
    index.Build(polygons);

    // Query region that doesn't overlap
    Rect query_region(200, 200, 100, 100);  // 200-300, 200-300
    std::vector<Polygon*> results = index.QueryRegion(query_region);

    EXPECT_EQ(results.size(), 0);
}

TEST_F(PolygonIndexTest, QueryRegion_PartialOverlap_ReturnsPolygon) {
    polygons.push_back(CreateRectPolygon(100, 100, 50, 50));  // 100-150, 100-150

    PolygonIndex index(GRID_SIZE, GRID_SIZE, SLIDE_WIDTH, SLIDE_HEIGHT);
    index.Build(polygons);

    // Query region overlaps only corner of polygon
    Rect query_region(140, 140, 50, 50);  // 140-190, 140-190
    std::vector<Polygon*> results = index.QueryRegion(query_region);

    // Should return the polygon (partial overlap)
    EXPECT_EQ(results.size(), 1);
}

// ============================================================================
// Multiple Polygon Tests
// ============================================================================

TEST_F(PolygonIndexTest, QueryRegion_MultiplePolygons_ReturnsAll) {
    polygons.push_back(CreateRectPolygon(100, 100, 50, 50));
    polygons.push_back(CreateRectPolygon(120, 120, 50, 50));
    polygons.push_back(CreateRectPolygon(500, 500, 50, 50));  // Far away

    PolygonIndex index(GRID_SIZE, GRID_SIZE, SLIDE_WIDTH, SLIDE_HEIGHT);
    index.Build(polygons);

    // Query region that overlaps first two polygons
    Rect query_region(90, 90, 90, 90);  // 90-180, 90-180
    std::vector<Polygon*> results = index.QueryRegion(query_region);

    EXPECT_EQ(results.size(), 2);
}

TEST_F(PolygonIndexTest, QueryRegion_EntireSlide_ReturnsAllPolygons) {
    // Add polygons at various positions
    for (int i = 0; i < 20; ++i) {
        polygons.push_back(CreateRectPolygon(i * 100, i * 100, 50, 50));
    }

    PolygonIndex index(GRID_SIZE, GRID_SIZE, SLIDE_WIDTH, SLIDE_HEIGHT);
    index.Build(polygons);

    // Query entire slide
    Rect query_region(0, 0, SLIDE_WIDTH, SLIDE_HEIGHT);
    std::vector<Polygon*> results = index.QueryRegion(query_region);

    EXPECT_EQ(results.size(), 20);
}

TEST_F(PolygonIndexTest, QueryRegion_NoDuplicates_EachPolygonOnce) {
    // Create a large polygon that spans multiple grid cells
    polygons.push_back(CreateRectPolygon(50, 50, 500, 500));

    PolygonIndex index(GRID_SIZE, GRID_SIZE, SLIDE_WIDTH, SLIDE_HEIGHT);
    index.Build(polygons);

    // Query a region that overlaps the polygon
    Rect query_region(100, 100, 400, 400);
    std::vector<Polygon*> results = index.QueryRegion(query_region);

    // Should return polygon exactly once, not multiple times
    EXPECT_EQ(results.size(), 1);
}

// ============================================================================
// Grid Boundary Tests
// ============================================================================

TEST_F(PolygonIndexTest, QueryRegion_AtGridBoundary_HandlesCorrectly) {
    // Grid cell size: 10000 / 100 = 100 pixels per cell
    // Place polygon exactly at grid boundary
    polygons.push_back(CreateRectPolygon(99, 99, 2, 2));  // Straddles boundary at 100

    PolygonIndex index(GRID_SIZE, GRID_SIZE, SLIDE_WIDTH, SLIDE_HEIGHT);
    index.Build(polygons);

    // Query region that includes the boundary
    Rect query_region(95, 95, 10, 10);  // 95-105, 95-105
    std::vector<Polygon*> results = index.QueryRegion(query_region);

    EXPECT_EQ(results.size(), 1);
}

TEST_F(PolygonIndexTest, QueryRegion_PolygonSpansMultipleCells_FoundInAllCells) {
    // Create polygon that definitely spans multiple grid cells
    // Grid cell = 100x100, so 500x500 polygon spans 25 cells
    polygons.push_back(CreateRectPolygon(50, 50, 500, 500));

    PolygonIndex index(GRID_SIZE, GRID_SIZE, SLIDE_WIDTH, SLIDE_HEIGHT);
    index.Build(polygons);

    // Query different regions within the polygon
    Rect query1(60, 60, 50, 50);    // Top-left
    Rect query2(500, 500, 50, 50);  // Bottom-right
    Rect query3(250, 250, 50, 50);  // Center

    EXPECT_EQ(index.QueryRegion(query1).size(), 1);
    EXPECT_EQ(index.QueryRegion(query2).size(), 1);
    EXPECT_EQ(index.QueryRegion(query3).size(), 1);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(PolygonIndexTest, QueryRegion_TinyPolygon_StillFound) {
    // 1x1 pixel polygon
    polygons.push_back(CreateRectPolygon(100, 100, 1, 1));

    PolygonIndex index(GRID_SIZE, GRID_SIZE, SLIDE_WIDTH, SLIDE_HEIGHT);
    index.Build(polygons);

    Rect query_region(99, 99, 3, 3);  // 99-102, 99-102
    std::vector<Polygon*> results = index.QueryRegion(query_region);

    EXPECT_EQ(results.size(), 1);
}

TEST_F(PolygonIndexTest, QueryRegion_PolygonAtEdge_FoundCorrectly) {
    // Polygon at slide edge
    polygons.push_back(CreateRectPolygon(9950, 7950, 50, 50));  // Near bottom-right corner

    PolygonIndex index(GRID_SIZE, GRID_SIZE, SLIDE_WIDTH, SLIDE_HEIGHT);
    index.Build(polygons);

    Rect query_region(9900, 7900, 100, 100);
    std::vector<Polygon*> results = index.QueryRegion(query_region);

    EXPECT_EQ(results.size(), 1);
}

TEST_F(PolygonIndexTest, QueryRegion_PolygonAtOrigin_FoundCorrectly) {
    polygons.push_back(CreateRectPolygon(0, 0, 50, 50));

    PolygonIndex index(GRID_SIZE, GRID_SIZE, SLIDE_WIDTH, SLIDE_HEIGHT);
    index.Build(polygons);

    Rect query_region(0, 0, 100, 100);
    std::vector<Polygon*> results = index.QueryRegion(query_region);

    EXPECT_EQ(results.size(), 1);
}

TEST_F(PolygonIndexTest, Build_PolygonWithNoVertices_HandlesGracefully) {
    Polygon empty_polygon;
    empty_polygon.ComputeBoundingBox();
    polygons.push_back(empty_polygon);

    PolygonIndex index(GRID_SIZE, GRID_SIZE, SLIDE_WIDTH, SLIDE_HEIGHT);

    EXPECT_NO_THROW({
        index.Build(polygons);
    });
}

TEST_F(PolygonIndexTest, QueryRegion_ExactBoundaryMatch_ReturnsPolygon) {
    // Polygon and query region have exact same bounds
    polygons.push_back(CreateRectPolygon(100, 100, 100, 100));

    PolygonIndex index(GRID_SIZE, GRID_SIZE, SLIDE_WIDTH, SLIDE_HEIGHT);
    index.Build(polygons);

    Rect query_region(100, 100, 100, 100);
    std::vector<Polygon*> results = index.QueryRegion(query_region);

    EXPECT_EQ(results.size(), 1);
}

// ============================================================================
// Clear Tests
// ============================================================================

TEST_F(PolygonIndexTest, Clear_RemovesAllPolygons) {
    polygons.push_back(CreateRectPolygon(100, 100, 50, 50));
    polygons.push_back(CreateRectPolygon(200, 200, 50, 50));

    PolygonIndex index(GRID_SIZE, GRID_SIZE, SLIDE_WIDTH, SLIDE_HEIGHT);
    index.Build(polygons);

    // Verify polygons are indexed
    Rect query_all(0, 0, SLIDE_WIDTH, SLIDE_HEIGHT);
    EXPECT_EQ(index.QueryRegion(query_all).size(), 2);

    // Clear and verify
    index.Clear();
    EXPECT_EQ(index.QueryRegion(query_all).size(), 0);
}

TEST_F(PolygonIndexTest, Clear_ThenRebuild_WorksCorrectly) {
    polygons.push_back(CreateRectPolygon(100, 100, 50, 50));

    PolygonIndex index(GRID_SIZE, GRID_SIZE, SLIDE_WIDTH, SLIDE_HEIGHT);
    index.Build(polygons);

    index.Clear();

    // Add different polygons and rebuild
    polygons.clear();
    polygons.push_back(CreateRectPolygon(500, 500, 50, 50));
    index.Build(polygons);

    Rect query_old(100, 100, 50, 50);
    Rect query_new(500, 500, 50, 50);

    EXPECT_EQ(index.QueryRegion(query_old).size(), 0);
    EXPECT_EQ(index.QueryRegion(query_new).size(), 1);
}

// ============================================================================
// Performance-Related Tests
// ============================================================================

TEST_F(PolygonIndexTest, QueryRegion_ManyPolygons_ReturnsOnlyRelevant) {
    // Add 100 polygons scattered across slide
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 10; ++j) {
            polygons.push_back(CreateRectPolygon(i * 1000, j * 800, 50, 50));
        }
    }

    PolygonIndex index(GRID_SIZE, GRID_SIZE, SLIDE_WIDTH, SLIDE_HEIGHT);
    index.Build(polygons);

    // Query small region that should only contain 1 polygon
    Rect small_query(2020, 1620, 100, 100);
    std::vector<Polygon*> results = index.QueryRegion(small_query);

    // Should return a small number, not all 100
    EXPECT_LT(results.size(), 10);
    EXPECT_GT(results.size(), 0);  // Should find at least the nearby one
}

// ============================================================================
// Correctness Tests with Different Shapes
// ============================================================================

TEST_F(PolygonIndexTest, QueryRegion_TriangularPolygon_FoundByBoundingBox) {
    polygons.push_back(CreateTrianglePolygon(100, 100, 150, 100, 125, 150));

    PolygonIndex index(GRID_SIZE, GRID_SIZE, SLIDE_WIDTH, SLIDE_HEIGHT);
    index.Build(polygons);

    // Query the bounding box area
    Rect query_region(100, 100, 50, 50);
    std::vector<Polygon*> results = index.QueryRegion(query_region);

    EXPECT_EQ(results.size(), 1);
}

TEST_F(PolygonIndexTest, QueryRegion_ComplexPolygon_IndexedCorrectly) {
    // Create irregular polygon (L-shape)
    Polygon l_shape;
    l_shape.vertices = {
        Vec2(100, 100), Vec2(200, 100), Vec2(200, 150),
        Vec2(150, 150), Vec2(150, 200), Vec2(100, 200)
    };
    l_shape.ComputeBoundingBox();
    polygons.push_back(l_shape);

    PolygonIndex index(GRID_SIZE, GRID_SIZE, SLIDE_WIDTH, SLIDE_HEIGHT);
    index.Build(polygons);

    // Query overlapping region
    Rect query_region(120, 120, 50, 50);
    std::vector<Polygon*> results = index.QueryRegion(query_region);

    EXPECT_EQ(results.size(), 1);
}

// ============================================================================
// Grid Size Variation Tests
// ============================================================================

TEST_F(PolygonIndexTest, Constructor_SmallGrid_WorksCorrectly) {
    polygons.push_back(CreateRectPolygon(100, 100, 50, 50));

    // Very coarse grid (10x10)
    PolygonIndex index(10, 10, SLIDE_WIDTH, SLIDE_HEIGHT);
    index.Build(polygons);

    Rect query_region(90, 90, 70, 70);
    std::vector<Polygon*> results = index.QueryRegion(query_region);

    EXPECT_EQ(results.size(), 1);
}

TEST_F(PolygonIndexTest, Constructor_LargeGrid_WorksCorrectly) {
    polygons.push_back(CreateRectPolygon(100, 100, 50, 50));

    // Very fine grid (1000x1000)
    PolygonIndex index(1000, 1000, SLIDE_WIDTH, SLIDE_HEIGHT);
    index.Build(polygons);

    Rect query_region(90, 90, 70, 70);
    std::vector<Polygon*> results = index.QueryRegion(query_region);

    EXPECT_EQ(results.size(), 1);
}

// ============================================================================
// Polygon Classification Tests
// ============================================================================

TEST_F(PolygonIndexTest, QueryRegion_DifferentClasses_AllReturned) {
    polygons.push_back(CreateRectPolygon(100, 100, 50, 50, 1));  // Class 1
    polygons.push_back(CreateRectPolygon(120, 120, 50, 50, 2));  // Class 2
    polygons.push_back(CreateRectPolygon(140, 140, 50, 50, 3));  // Class 3

    PolygonIndex index(GRID_SIZE, GRID_SIZE, SLIDE_WIDTH, SLIDE_HEIGHT);
    index.Build(polygons);

    Rect query_region(90, 90, 110, 110);
    std::vector<Polygon*> results = index.QueryRegion(query_region);

    EXPECT_EQ(results.size(), 3);

    // Verify all classes are present
    std::vector<int> classes;
    for (const auto* poly : results) {
        classes.push_back(poly->classId);
    }
    std::sort(classes.begin(), classes.end());

    EXPECT_EQ(classes[0], 1);
    EXPECT_EQ(classes[1], 2);
    EXPECT_EQ(classes[2], 3);
}
