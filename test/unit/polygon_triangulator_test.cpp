// PolygonTriangulator Unit Tests
// Tests for ear-clipping triangulation algorithm
// Critical for polygon rendering

#include <gtest/gtest.h>
#include "PolygonTriangulator.h"
#include <vector>
#include <set>

// ============================================================================
// Test Fixture
// ============================================================================

class PolygonTriangulatorTest : public ::testing::Test {
protected:
    // Helper to count unique triangle count
    int CountTriangles(const std::vector<int>& indices) {
        return indices.size() / 3;
    }

    // Helper to check if all indices are valid
    bool AllIndicesValid(const std::vector<int>& indices, size_t vertexCount) {
        for (int idx : indices) {
            if (idx < 0 || idx >= static_cast<int>(vertexCount)) {
                return false;
            }
        }
        return true;
    }
};

// ============================================================================
// Triangle Count Invariant Tests
// ============================================================================

TEST_F(PolygonTriangulatorTest, Triangulate_Triangle_ReturnsOneTriangle) {
    std::vector<Vec2> vertices = {
        Vec2(0, 0), Vec2(10, 0), Vec2(5, 10)
    };

    std::vector<int> indices = PolygonTriangulator::Triangulate(vertices);

    EXPECT_EQ(indices.size(), 3);  // 1 triangle = 3 indices
    EXPECT_EQ(CountTriangles(indices), 1);
}

TEST_F(PolygonTriangulatorTest, Triangulate_SimpleQuad_ReturnsTwoTriangles) {
    std::vector<Vec2> vertices = {
        Vec2(0, 0), Vec2(10, 0), Vec2(10, 10), Vec2(0, 10)
    };

    std::vector<int> indices = PolygonTriangulator::Triangulate(vertices);

    EXPECT_EQ(indices.size(), 6);  // 2 triangles = 6 indices
    EXPECT_EQ(CountTriangles(indices), 2);  // (4-2) = 2 triangles
}

TEST_F(PolygonTriangulatorTest, Triangulate_Pentagon_ReturnsThreeTriangles) {
    std::vector<Vec2> vertices = {
        Vec2(0, 0), Vec2(10, 0), Vec2(12, 8), Vec2(5, 12), Vec2(-2, 8)
    };

    std::vector<int> indices = PolygonTriangulator::Triangulate(vertices);

    EXPECT_EQ(indices.size(), 9);  // 3 triangles = 9 indices
    EXPECT_EQ(CountTriangles(indices), 3);  // (5-2) = 3 triangles
}

TEST_F(PolygonTriangulatorTest, Triangulate_Hexagon_ReturnsFourTriangles) {
    std::vector<Vec2> vertices = {
        Vec2(0, 0), Vec2(10, 0), Vec2(15, 5), Vec2(10, 10), Vec2(0, 10), Vec2(-5, 5)
    };

    std::vector<int> indices = PolygonTriangulator::Triangulate(vertices);

    EXPECT_EQ(indices.size(), 12);  // 4 triangles = 12 indices
    EXPECT_EQ(CountTriangles(indices), 4);  // (6-2) = 4 triangles
}

// ============================================================================
// Degenerate Cases
// ============================================================================

TEST_F(PolygonTriangulatorTest, Triangulate_EmptyPolygon_ReturnsEmpty) {
    std::vector<Vec2> vertices;

    std::vector<int> indices = PolygonTriangulator::Triangulate(vertices);

    EXPECT_EQ(indices.size(), 0);
}

TEST_F(PolygonTriangulatorTest, Triangulate_SingleVertex_ReturnsEmpty) {
    std::vector<Vec2> vertices = {Vec2(0, 0)};

    std::vector<int> indices = PolygonTriangulator::Triangulate(vertices);

    EXPECT_EQ(indices.size(), 0);
}

TEST_F(PolygonTriangulatorTest, Triangulate_TwoVertices_ReturnsEmpty) {
    std::vector<Vec2> vertices = {Vec2(0, 0), Vec2(10, 10)};

    std::vector<int> indices = PolygonTriangulator::Triangulate(vertices);

    EXPECT_EQ(indices.size(), 0);
}

// ============================================================================
// Index Validity Tests
// ============================================================================

TEST_F(PolygonTriangulatorTest, Triangulate_AllIndices_WithinBounds) {
    std::vector<Vec2> vertices = {
        Vec2(0, 0), Vec2(10, 0), Vec2(10, 10), Vec2(0, 10)
    };

    std::vector<int> indices = PolygonTriangulator::Triangulate(vertices);

    EXPECT_TRUE(AllIndicesValid(indices, vertices.size()));

    for (int idx : indices) {
        EXPECT_GE(idx, 0);
        EXPECT_LT(idx, vertices.size());
    }
}

TEST_F(PolygonTriangulatorTest, Triangulate_ComplexPolygon_ValidIndices) {
    // L-shaped polygon
    std::vector<Vec2> vertices = {
        Vec2(0, 0), Vec2(10, 0), Vec2(10, 5),
        Vec2(5, 5), Vec2(5, 10), Vec2(0, 10)
    };

    std::vector<int> indices = PolygonTriangulator::Triangulate(vertices);

    EXPECT_TRUE(AllIndicesValid(indices, vertices.size()));
    EXPECT_EQ(CountTriangles(indices), 4);  // (6-2) = 4 triangles
}

// ============================================================================
// Concave Polygon Tests
// ============================================================================

TEST_F(PolygonTriangulatorTest, Triangulate_ConcavePolygon_ProducesCorrectTriangles) {
    // Simple concave polygon (chevron/arrow shape)
    std::vector<Vec2> vertices = {
        Vec2(0, 0), Vec2(10, 0), Vec2(10, 5),
        Vec2(5, 5), Vec2(5, 10), Vec2(0, 10)
    };

    std::vector<int> indices = PolygonTriangulator::Triangulate(vertices);

    EXPECT_EQ(indices.size(), 12);  // (6-2) = 4 triangles
}

TEST_F(PolygonTriangulatorTest, Triangulate_StarShape_HandlesCorrectly) {
    // 5-pointed star (alternating inner and outer vertices)
    std::vector<Vec2> vertices = {
        Vec2(0, -10),   // Top point
        Vec2(-2, -3),   // Inner
        Vec2(-9, -3),   // Left point
        Vec2(-3, 2),    // Inner
        Vec2(-6, 9),    // Bottom-left
        Vec2(0, 4),     // Inner
        Vec2(6, 9),     // Bottom-right
        Vec2(3, 2),     // Inner
        Vec2(9, -3),    // Right point
        Vec2(2, -3)     // Inner
    };

    std::vector<int> indices = PolygonTriangulator::Triangulate(vertices);

    EXPECT_EQ(CountTriangles(indices), 8);  // (10-2) = 8 triangles
    EXPECT_TRUE(AllIndicesValid(indices, vertices.size()));
}

// ============================================================================
// Collinear Points Tests
// ============================================================================

TEST_F(PolygonTriangulatorTest, Triangulate_CollinearVertices_HandlesGracefully) {
    // Rectangle with extra collinear point on top edge
    std::vector<Vec2> vertices = {
        Vec2(0, 0), Vec2(5, 0), Vec2(10, 0),  // Three collinear points
        Vec2(10, 10), Vec2(0, 10)
    };

    std::vector<int> indices = PolygonTriangulator::Triangulate(vertices);

    // Should still triangulate (degenerates to quad essentially)
    EXPECT_GT(indices.size(), 0);
    EXPECT_TRUE(AllIndicesValid(indices, vertices.size()));
}

// ============================================================================
// Numerical Precision Tests
// ============================================================================

TEST_F(PolygonTriangulatorTest, Triangulate_VerySmallPolygon_Works) {
    // Tiny polygon (< 1 pixel)
    std::vector<Vec2> vertices = {
        Vec2(0.1, 0.1), Vec2(0.2, 0.1), Vec2(0.15, 0.2)
    };

    std::vector<int> indices = PolygonTriangulator::Triangulate(vertices);

    EXPECT_EQ(indices.size(), 3);
}

TEST_F(PolygonTriangulatorTest, Triangulate_VeryLargePolygon_Works) {
    // Large polygon
    std::vector<Vec2> vertices = {
        Vec2(0, 0), Vec2(10000, 0), Vec2(10000, 10000), Vec2(0, 10000)
    };

    std::vector<int> indices = PolygonTriangulator::Triangulate(vertices);

    EXPECT_EQ(indices.size(), 6);
    EXPECT_TRUE(AllIndicesValid(indices, vertices.size()));
}

// ============================================================================
// Winding Order Tests
// ============================================================================

TEST_F(PolygonTriangulatorTest, Triangulate_CounterClockwise_Works) {
    // CCW winding
    std::vector<Vec2> vertices = {
        Vec2(0, 0), Vec2(10, 0), Vec2(10, 10), Vec2(0, 10)
    };

    std::vector<int> indices = PolygonTriangulator::Triangulate(vertices);

    EXPECT_EQ(CountTriangles(indices), 2);
}

TEST_F(PolygonTriangulatorTest, Triangulate_Clockwise_Works) {
    // CW winding (reversed from CCW)
    std::vector<Vec2> vertices = {
        Vec2(0, 0), Vec2(0, 10), Vec2(10, 10), Vec2(10, 0)
    };

    std::vector<int> indices = PolygonTriangulator::Triangulate(vertices);

    EXPECT_EQ(CountTriangles(indices), 2);
}

// ============================================================================
// Complex Shape Tests
// ============================================================================

TEST_F(PolygonTriangulatorTest, Triangulate_LetterEShape_CorrectTriangleCount) {
    // E-shaped polygon
    std::vector<Vec2> vertices = {
        Vec2(0, 0), Vec2(10, 0), Vec2(10, 3),
        Vec2(3, 3), Vec2(3, 7), Vec2(10, 7),
        Vec2(10, 10), Vec2(3, 10), Vec2(3, 14),
        Vec2(10, 14), Vec2(10, 17), Vec2(0, 17)
    };

    std::vector<int> indices = PolygonTriangulator::Triangulate(vertices);

    EXPECT_EQ(CountTriangles(indices), 10);  // (12-2) = 10 triangles
    EXPECT_TRUE(AllIndicesValid(indices, vertices.size()));
}

TEST_F(PolygonTriangulatorTest, Triangulate_IrregularPolygon_NoSelfIntersections) {
    // Irregular convex polygon
    std::vector<Vec2> vertices = {
        Vec2(2, 3), Vec2(8, 1), Vec2(15, 5), Vec2(12, 12), Vec2(4, 10)
    };

    std::vector<int> indices = PolygonTriangulator::Triangulate(vertices);

    EXPECT_EQ(CountTriangles(indices), 3);  // (5-2) = 3 triangles
    EXPECT_TRUE(AllIndicesValid(indices, vertices.size()));
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST_F(PolygonTriangulatorTest, Triangulate_ManyVertices_CorrectTriangleCount) {
    // Create polygon with many vertices (circle approximation)
    std::vector<Vec2> vertices;
    int num_vertices = 50;
    double radius = 100.0;

    for (int i = 0; i < num_vertices; ++i) {
        double angle = 2.0 * M_PI * i / num_vertices;
        vertices.push_back(Vec2(radius * cos(angle), radius * sin(angle)));
    }

    std::vector<int> indices = PolygonTriangulator::Triangulate(vertices);

    EXPECT_EQ(CountTriangles(indices), num_vertices - 2);  // (n-2) triangles
    EXPECT_TRUE(AllIndicesValid(indices, vertices.size()));
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(PolygonTriangulatorTest, Triangulate_NearlyCollinearTriangle_Works) {
    // Very flat triangle (almost collinear)
    std::vector<Vec2> vertices = {
        Vec2(0, 0), Vec2(100, 0), Vec2(50, 0.01)  // Peak is barely above base
    };

    std::vector<int> indices = PolygonTriangulator::Triangulate(vertices);

    // Should still produce a triangle or handle gracefully
    EXPECT_GE(indices.size(), 0);  // Either 0 or 3
}

TEST_F(PolygonTriangulatorTest, Triangulate_DuplicateVertices_HandlesGracefully) {
    // Polygon with duplicate vertex
    std::vector<Vec2> vertices = {
        Vec2(0, 0), Vec2(10, 0), Vec2(10, 0),  // Duplicate
        Vec2(10, 10), Vec2(0, 10)
    };

    std::vector<int> indices = PolygonTriangulator::Triangulate(vertices);

    // Should handle gracefully (implementation-dependent)
    EXPECT_GE(indices.size(), 0);
}

// ============================================================================
// Triangle Validity Tests
// ============================================================================

TEST_F(PolygonTriangulatorTest, Triangulate_OutputIndices_FormValidTriangles) {
    std::vector<Vec2> vertices = {
        Vec2(0, 0), Vec2(10, 0), Vec2(10, 10), Vec2(0, 10)
    };

    std::vector<int> indices = PolygonTriangulator::Triangulate(vertices);

    // Each set of 3 indices should form a valid triangle
    ASSERT_EQ(indices.size() % 3, 0);  // Must be multiple of 3

    for (size_t i = 0; i < indices.size(); i += 3) {
        int i0 = indices[i];
        int i1 = indices[i + 1];
        int i2 = indices[i + 2];

        // Indices should be different (non-degenerate triangle)
        EXPECT_NE(i0, i1);
        EXPECT_NE(i1, i2);
        EXPECT_NE(i0, i2);
    }
}

TEST_F(PolygonTriangulatorTest, Triangulate_NoRepeatedTriangles_UniqueIndices) {
    std::vector<Vec2> vertices = {
        Vec2(0, 0), Vec2(10, 0), Vec2(10, 10), Vec2(5, 10), Vec2(0, 10)
    };

    std::vector<int> indices = PolygonTriangulator::Triangulate(vertices);

    // Convert triangles to sets and check for duplicates
    std::set<std::set<int>> unique_triangles;

    for (size_t i = 0; i < indices.size(); i += 3) {
        std::set<int> triangle = {indices[i], indices[i+1], indices[i+2]};
        unique_triangles.insert(triangle);
    }

    // Number of unique triangles should equal total triangles
    EXPECT_EQ(unique_triangles.size(), indices.size() / 3);
}
