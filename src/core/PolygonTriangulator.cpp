#include "PolygonTriangulator.h"
#include <cmath>
#include <algorithm>

std::vector<int> PolygonTriangulator::Triangulate(const std::vector<Vec2>& vertices) {
    std::vector<int> triangles;

    if (vertices.size() < 3) {
        return triangles;  // Not enough vertices
    }

    if (vertices.size() == 3) {
        // Triangle - already triangulated
        triangles = {0, 1, 2};
        return triangles;
    }

    // Determine polygon winding (signed area > 0 means CCW)
    double signedArea = 0.0;
    for (size_t i = 0; i < vertices.size(); ++i) {
        size_t j = (i + 1) % vertices.size();
        signedArea += vertices[i].x * vertices[j].y - vertices[j].x * vertices[i].y;
    }
    bool isCCW = signedArea > 0.0;

    // Create list of active vertex indices
    std::vector<int> indices;
    indices.reserve(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        indices.push_back(static_cast<int>(i));
    }

    // Ear clipping algorithm
    int iterations = 0;
    const int maxIterations = static_cast<int>(vertices.size()) * 2;  // Safety limit

    while (indices.size() > 3 && iterations < maxIterations) {
        bool earFound = false;

        for (size_t i = 0; i < indices.size(); ++i) {
            if (IsEar(vertices, indices, static_cast<int>(i), isCCW)) {
                // Found an ear - add triangle and remove the ear vertex
                int prevIdx = (i == 0) ? static_cast<int>(indices.size() - 1) : static_cast<int>(i - 1);
                int nextIdx = (i == indices.size() - 1) ? 0 : static_cast<int>(i + 1);

                triangles.push_back(indices[prevIdx]);
                triangles.push_back(indices[i]);
                triangles.push_back(indices[nextIdx]);

                // Remove the ear vertex from active list
                indices.erase(indices.begin() + i);

                earFound = true;
                break;
            }
        }

        if (!earFound) {
            // No ear found - polygon might be degenerate or self-intersecting
            // Fall back to simple fan triangulation
            for (size_t i = 1; i + 1 < indices.size(); ++i) {
                triangles.push_back(indices[0]);
                triangles.push_back(indices[i]);
                triangles.push_back(indices[i + 1]);
            }
            break;
        }

        ++iterations;
    }

    // Add final triangle
    if (indices.size() == 3) {
        triangles.push_back(indices[0]);
        triangles.push_back(indices[1]);
        triangles.push_back(indices[2]);
    }

    return triangles;
}

bool PolygonTriangulator::IsEar(const std::vector<Vec2>& vertices,
                                const std::vector<int>& indices,
                                int i,
                                bool isCCW) {
    int n = static_cast<int>(indices.size());
    int prevIdx = (i == 0) ? n - 1 : i - 1;
    int nextIdx = (i == n - 1) ? 0 : i + 1;

    const Vec2& prev = vertices[indices[prevIdx]];
    const Vec2& curr = vertices[indices[i]];
    const Vec2& next = vertices[indices[nextIdx]];

    // Check if triangle is convex
    if (!IsConvex(prev, curr, next, isCCW)) {
        return false;
    }

    // Check if any other vertex is inside this triangle
    for (int j = 0; j < n; ++j) {
        if (j == prevIdx || j == i || j == nextIdx) {
            continue;
        }

        const Vec2& p = vertices[indices[j]];
        if (PointInTriangle(p, prev, curr, next)) {
            return false;
        }
    }

    return true;
}

bool PolygonTriangulator::IsConvex(const Vec2& a, const Vec2& b, const Vec2& c, bool isCCW) {
    // Compute cross product of vectors (b-a) and (c-b)
    double cross = (b.x - a.x) * (c.y - b.y) - (b.y - a.y) * (c.x - b.x);
    constexpr double kEpsilon = 1e-12;
    return isCCW ? (cross > kEpsilon) : (cross < -kEpsilon);
}

bool PolygonTriangulator::PointInTriangle(const Vec2& p,
                                         const Vec2& a,
                                         const Vec2& b,
                                         const Vec2& c) {
    // Use barycentric coordinates
    double d1 = SignedArea(p, a, b);
    double d2 = SignedArea(p, b, c);
    double d3 = SignedArea(p, c, a);

    bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);

    return !(hasNeg && hasPos);  // Point is inside if all same sign
}

double PolygonTriangulator::SignedArea(const Vec2& a, const Vec2& b, const Vec2& c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}
