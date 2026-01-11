#pragma once

#include "Viewport.h"
#include <SDL2/SDL.h>
#include <vector>
#include <map>
#include <string>
#include <cstdint>
#include <array>
#include <memory>

// Tissue tile data structure
struct TissueTile {
    int level;                      // Pyramid level
    int tileX, tileY;               // Tile grid position
    int width, height;              // Tile dimensions in pixels
    std::vector<uint8_t> classData; // Class IDs per pixel (row-major)
    SDL_Texture* texture;           // Cached rendered texture
    bool textureValid;              // Cache invalidation flag

    // Pre-computed for performance (avoid std::pow per tile per frame)
    double scaleFactor;             // pow(2, maxLevel - level)
    Rect bounds;                    // Bounds in slide coordinates (level 0)

    TissueTile()
        : level(0), tileX(0), tileY(0), width(0), height(0),
          texture(nullptr), textureValid(false), scaleFactor(1.0) {}

    ~TissueTile() {
        // Note: texture cleanup handled by TissueMapOverlay
    }

    // Move constructor
    TissueTile(TissueTile&& other) noexcept
        : level(other.level), tileX(other.tileX), tileY(other.tileY),
          width(other.width), height(other.height),
          classData(std::move(other.classData)),
          texture(other.texture), textureValid(other.textureValid),
          scaleFactor(other.scaleFactor), bounds(other.bounds) {
        other.texture = nullptr;
        other.textureValid = false;
    }

    // Move assignment
    TissueTile& operator=(TissueTile&& other) noexcept {
        if (this != &other) {
            level = other.level;
            tileX = other.tileX;
            tileY = other.tileY;
            width = other.width;
            height = other.height;
            classData = std::move(other.classData);
            texture = other.texture;
            textureValid = other.textureValid;
            scaleFactor = other.scaleFactor;
            bounds = other.bounds;
            other.texture = nullptr;
            other.textureValid = false;
        }
        return *this;
    }

    // Delete copy operations
    TissueTile(const TissueTile&) = delete;
    TissueTile& operator=(const TissueTile&) = delete;
};

// Tissue class metadata
struct TissueClass {
    int classId;
    std::string name;
    SDL_Color color;
    bool visible;

    TissueClass() : classId(0), color({128, 128, 128, 255}), visible(true) {}
    TissueClass(int id, const std::string& n, SDL_Color c)
        : classId(id), name(n), color(c), visible(true) {}
};

// Spatial index for efficient tissue tile queries (O(k) instead of O(N))
class TissueTileIndex {
public:
    TissueTileIndex(int gridWidth, int gridHeight, double slideWidth, double slideHeight);
    void Build(std::vector<TissueTile>& tiles);
    std::vector<TissueTile*> QueryRegion(const Rect& region) const;
    void Clear();

private:
    std::vector<std::vector<std::vector<TissueTile*>>> grid_;  // 2D grid of tile pointers
    int gridWidth_, gridHeight_;
    double cellWidth_, cellHeight_;

    void SlideToGridCell(double x, double y, int& outCellX, int& outCellY) const;
};

// Main tissue map overlay class
class TissueMapOverlay {
public:
    explicit TissueMapOverlay(SDL_Renderer* renderer);
    ~TissueMapOverlay();

    // Delete copy constructor and assignment
    TissueMapOverlay(const TissueMapOverlay&) = delete;
    TissueMapOverlay& operator=(const TissueMapOverlay&) = delete;

    // Data loading
    void SetTissueData(std::vector<TissueTile>&& tiles,
                       const std::map<int, std::string>& classMapping,
                       int maxLevel);
    void Clear();

    // Rendering
    void Render(const Viewport& viewport);

    // Global visibility control
    void SetVisible(bool visible) { visible_ = visible; }
    bool IsVisible() const { return visible_; }

    // Opacity control (0.0 - 1.0)
    void SetOpacity(float opacity);
    float GetOpacity() const { return opacity_; }

    // Per-class visibility
    void SetClassVisible(int classId, bool visible);
    bool IsClassVisible(int classId) const;
    void SetAllClassesVisible(bool visible);

    // Color management
    void SetClassColor(int classId, SDL_Color color);
    SDL_Color GetClassColor(int classId) const;

    // Class information for UI
    const std::vector<TissueClass>& GetClasses() const { return classes_; }
    std::vector<int> GetClassIds() const;
    std::string GetClassName(int classId) const;
    int GetTileCount() const { return static_cast<int>(tiles_.size()); }

    // Slide dimensions for coordinate transforms
    void SetSlideDimensions(double width, double height);

private:
    SDL_Renderer* renderer_;
    std::vector<TissueTile> tiles_;
    std::vector<TissueClass> classes_;
    std::map<int, size_t> classIdToIndex_;  // Fast lookup: classId -> index in classes_

    bool visible_;
    float opacity_;
    double slideWidth_;
    double slideHeight_;
    int maxLevel_;

    // Spatial index for O(k) tile queries instead of O(N)
    std::unique_ptr<TissueTileIndex> spatialIndex_;
    static constexpr int DEFAULT_GRID_SIZE = 64;

    // Color lookup table for O(1) pixel color lookup (classId is uint8_t: 0-255)
    std::array<SDL_Color, 256> colorLUT_;

    // Texture management
    void RenderTileTexture(TissueTile& tile);
    void InvalidateAllTextures();
    void DestroyAllTextures();

    // Color LUT management
    void RebuildColorLUT();

    // Spatial index management
    void BuildSpatialIndex();

    // Default tissue class color palette
    static SDL_Color GetDefaultTissueColor(int classId);
};
