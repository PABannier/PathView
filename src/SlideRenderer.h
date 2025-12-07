#pragma once

#include <SDL2/SDL.h>
#include <cstdint>
#include <vector>
#include <memory>

class SlideLoader;
class Viewport;
class TextureManager;
class TileCache;
struct TileKey;

class SlideRenderer {
public:
    SlideRenderer(SlideLoader* loader, SDL_Renderer* renderer, TextureManager* textureManager);
    ~SlideRenderer();

    void Render(const Viewport& viewport);

    // Get cache statistics
    size_t GetCacheTileCount() const;
    size_t GetCacheMemoryUsage() const;
    double GetCacheHitRate() const;

private:
    int32_t SelectLevel(double zoom) const;
    void RenderTiled(const Viewport& viewport, int32_t level);
    std::vector<TileKey> EnumerateVisibleTiles(const Viewport& viewport, int32_t level) const;
    void LoadAndRenderTile(const TileKey& key, const Viewport& viewport, int32_t level);

    SlideLoader* loader_;
    SDL_Renderer* renderer_;
    TextureManager* textureManager_;
    std::unique_ptr<TileCache> tileCache_;

    // Tile size (512x512 is standard)
    static constexpr int32_t TILE_SIZE = 512;
};
