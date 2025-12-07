#include "SlideRenderer.h"
#include "SlideLoader.h"
#include "Viewport.h"
#include "TextureManager.h"
#include "TileCache.h"
#include <iostream>
#include <cmath>
#include <algorithm>

SlideRenderer::SlideRenderer(SlideLoader* loader, SDL_Renderer* renderer, TextureManager* textureManager)
    : loader_(loader)
    , renderer_(renderer)
    , textureManager_(textureManager)
    , tileCache_(std::make_unique<TileCache>())
{
}

SlideRenderer::~SlideRenderer() {
}

void SlideRenderer::Render(const Viewport& viewport) {
    if (!loader_ || !loader_->IsValid()) {
        return;
    }

    // Select appropriate level based on zoom
    int32_t level = SelectLevel(viewport.GetZoom());

    // Render using tiles
    RenderTiled(viewport, level);
}

size_t SlideRenderer::GetCacheTileCount() const {
    return tileCache_ ? tileCache_->GetTileCount() : 0;
}

size_t SlideRenderer::GetCacheMemoryUsage() const {
    return tileCache_ ? tileCache_->GetMemoryUsage() : 0;
}

double SlideRenderer::GetCacheHitRate() const {
    return tileCache_ ? tileCache_->GetHitRate() : 0.0;
}

int32_t SlideRenderer::SelectLevel(double zoom) const {
    // Goal: Select level where downsample ≈ 1/zoom
    // At 100% zoom (1.0), we want level 0 (downsample 1)
    // At 50% zoom (0.5), we want a level with downsample ~2
    // At 25% zoom (0.25), we want a level with downsample ~4

    double targetDownsample = 1.0 / zoom;
    int32_t levelCount = loader_->GetLevelCount();

    int32_t bestLevel = 0;
    double bestDiff = std::abs(loader_->GetLevelDownsample(0) - targetDownsample);

    for (int32_t i = 1; i < levelCount; ++i) {
        double downsample = loader_->GetLevelDownsample(i);
        double diff = std::abs(downsample - targetDownsample);

        // Prefer higher resolution when between two levels to avoid pixelation
        if (diff < bestDiff || (diff == bestDiff && downsample < loader_->GetLevelDownsample(bestLevel))) {
            bestDiff = diff;
            bestLevel = i;
        }
    }

    return bestLevel;
}

void SlideRenderer::RenderTiled(const Viewport& viewport, int32_t level) {
    // Enumerate visible tiles
    std::vector<TileKey> visibleTiles = EnumerateVisibleTiles(viewport, level);

    // Load and render each visible tile
    for (const auto& tileKey : visibleTiles) {
        LoadAndRenderTile(tileKey, viewport, level);
    }
}

std::vector<TileKey> SlideRenderer::EnumerateVisibleTiles(const Viewport& viewport, int32_t level) const {
    std::vector<TileKey> tiles;

    // Get visible region in slide coordinates (level 0)
    Rect visibleRegion = viewport.GetVisibleRegion();

    // Get downsample factor for this level
    double downsample = loader_->GetLevelDownsample(level);

    // Convert visible region to level coordinates
    int64_t levelLeft = static_cast<int64_t>(visibleRegion.x / downsample);
    int64_t levelTop = static_cast<int64_t>(visibleRegion.y / downsample);
    int64_t levelRight = static_cast<int64_t>((visibleRegion.x + visibleRegion.width) / downsample);
    int64_t levelBottom = static_cast<int64_t>((visibleRegion.y + visibleRegion.height) / downsample);

    // Get level dimensions
    auto levelDims = loader_->GetLevelDimensions(level);

    // Clamp to level bounds
    levelLeft = std::max(0LL, levelLeft);
    levelTop = std::max(0LL, levelTop);
    levelRight = std::min(levelDims.width, levelRight);
    levelBottom = std::min(levelDims.height, levelBottom);

    // Calculate tile indices
    int32_t startTileX = static_cast<int32_t>(levelLeft / TILE_SIZE);
    int32_t startTileY = static_cast<int32_t>(levelTop / TILE_SIZE);
    int32_t endTileX = static_cast<int32_t>(levelRight / TILE_SIZE);
    int32_t endTileY = static_cast<int32_t>(levelBottom / TILE_SIZE);

    // Enumerate all visible tiles
    for (int32_t ty = startTileY; ty <= endTileY; ++ty) {
        for (int32_t tx = startTileX; tx <= endTileX; ++tx) {
            tiles.push_back({level, tx, ty});
        }
    }

    return tiles;
}

void SlideRenderer::LoadAndRenderTile(const TileKey& key, const Viewport& viewport, int32_t level) {
    // Try to get tile from cache
    const TileData* cachedTile = tileCache_->GetTile(key);

    if (!cachedTile) {
        // Cache miss - load tile from slide
        double downsample = loader_->GetLevelDownsample(level);

        // Calculate tile position in level 0 coordinates
        int64_t x0 = static_cast<int64_t>(key.tileX * TILE_SIZE * downsample);
        int64_t y0 = static_cast<int64_t>(key.tileY * TILE_SIZE * downsample);

        // Calculate tile dimensions at this level
        auto levelDims = loader_->GetLevelDimensions(level);
        int64_t levelX = key.tileX * TILE_SIZE;
        int64_t levelY = key.tileY * TILE_SIZE;

        int64_t tileWidth = std::min(static_cast<int64_t>(TILE_SIZE), levelDims.width - levelX);
        int64_t tileHeight = std::min(static_cast<int64_t>(TILE_SIZE), levelDims.height - levelY);

        if (tileWidth <= 0 || tileHeight <= 0) {
            return;
        }

        // Read tile from slide
        uint32_t* pixels = loader_->ReadRegion(level, x0, y0, tileWidth, tileHeight);
        if (!pixels) {
            return;
        }

        // Store in cache
        TileData tileData(pixels, tileWidth, tileHeight);
        tileCache_->InsertTile(key, std::move(tileData));

        // Get the cached tile
        cachedTile = tileCache_->GetTile(key);
        if (!cachedTile) {
            return;
        }
    }

    // Create or get SDL texture
    SDL_Texture* texture = textureManager_->GetOrCreateTexture(
        key,
        cachedTile->pixels,
        cachedTile->width,
        cachedTile->height
    );

    if (!texture) {
        return;
    }

    // Calculate tile position in slide coordinates (level 0)
    double downsample = loader_->GetLevelDownsample(level);
    double tileX0 = key.tileX * TILE_SIZE * downsample;
    double tileY0 = key.tileY * TILE_SIZE * downsample;
    double tileX1 = tileX0 + cachedTile->width * downsample;
    double tileY1 = tileY0 + cachedTile->height * downsample;

    // Convert to screen coordinates
    Vec2 topLeft = viewport.SlideToScreen(Vec2(tileX0, tileY0));
    Vec2 bottomRight = viewport.SlideToScreen(Vec2(tileX1, tileY1));

    SDL_Rect dstRect = {
        static_cast<int>(topLeft.x),
        static_cast<int>(topLeft.y),
        static_cast<int>(bottomRight.x - topLeft.x),
        static_cast<int>(bottomRight.y - topLeft.y)
    };

    // Render tile
    SDL_RenderCopy(renderer_, texture, nullptr, &dstRect);
}
