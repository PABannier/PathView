#pragma once

#include <SDL2/SDL.h>
#include <cstdint>
#include <vector>
#include <memory>

#include "TileConstants.h"
#include "TileKey.h"

class ISlideSource;
class Viewport;
class TextureManager;
class TileCache;
class TileLoadThreadPool;
struct TileData;

class SlideRenderer {
public:
    SlideRenderer(ISlideSource* source, SDL_Renderer* renderer, TextureManager* textureManager);
    ~SlideRenderer();

    // Lifecycle management for async loading
    void Initialize();
    void Shutdown();

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

    // Progressive rendering: find and render fallback from coarser level
    const TileData* FindBestFallback(const TileKey& key, TileKey* outFallbackKey);
    void RenderFallbackTile(const TileKey& targetKey, const TileKey& fallbackKey,
                            const TileData* fallbackTile, const Viewport& viewport, int32_t targetLevel);

    // Render a cached tile to screen
    void RenderTileToScreen(const TileKey& key, const TileData* tileData,
                            const Viewport& viewport, int32_t level);

    ISlideSource* source_;
    SDL_Renderer* renderer_;
    TextureManager* textureManager_;
    std::unique_ptr<TileCache> tileCache_;
    std::unique_ptr<TileLoadThreadPool> threadPool_;

    // Tile size (512x512 is standard)
    static constexpr int32_t TILE_SIZE = pathview::kTileSize;
};
