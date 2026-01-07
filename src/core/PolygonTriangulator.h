#pragma once

#include "PolygonOverlay.h"
#include <vector>

/**
 * Polygon triangulation using ear-clipping algorithm
 * Works for simple (non-self-intersecting) polygons
 */
class PolygonTriangulator {
public:
    /**
     * Triangulate a polygon into a list of triangle indices
     * @param vertices Polygon vertices in order (CCW or CW)
     * @return Vector of indices forming triangles (each 3 indices = 1 triangle)
     */
    static std::vector<int> Triangulate(const std::vector<Vec2>& vertices);

private:
    /**
     * Check if a triangle formed by three consecutive vertices is an "ear"
     * (i.e., can be safely removed without creating intersections)
     * @param vertices All polygon vertices
     * @param indices Current active vertex indices
     * @param i Index in the indices array to check
     * @return true if the triangle is an ear
     */
    static bool IsEar(const std::vector<Vec2>& vertices,
                     const std::vector<int>& indices,
                     int i,
                     bool isCCW);

    /**
     * Check if three points form a convex angle (turn right)
     * @param a First point
     * @param b Middle point (vertex of the angle)
     * @param c Third point
     * @return true if angle is convex (< 180 degrees)
     */
    static bool IsConvex(const Vec2& a, const Vec2& b, const Vec2& c, bool isCCW);

    /**
     * Check if a point is inside a triangle
     * @param p Point to check
     * @param a Triangle vertex 1
     * @param b Triangle vertex 2
     * @param c Triangle vertex 3
     * @return true if point is inside the triangle
     */
    static bool PointInTriangle(const Vec2& p, const Vec2& a, const Vec2& b, const Vec2& c);

    /**
     * Calculate signed area of a triangle (half of cross product)
     * @param a First vertex
     * @param b Second vertex
     * @param c Third vertex
     * @return Signed area (positive if CCW, negative if CW)
     */
    static double SignedArea(const Vec2& a, const Vec2& b, const Vec2& c);
};
