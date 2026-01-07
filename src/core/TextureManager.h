#pragma once

#include <SDL2/SDL.h>
#include <cstdint>
#include <unordered_map>
#include "TileKey.h"

class TileCache;

class TextureManager {
public:
    explicit TextureManager(SDL_Renderer* renderer);
    ~TextureManager();

    // Delete copy, allow move
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    // Create texture from RGBA pixel data
    SDL_Texture* CreateTexture(const uint32_t* pixels, int32_t width, int32_t height);

    // Get or create texture for a tile
    SDL_Texture* GetOrCreateTexture(const TileKey& key, const uint32_t* pixels, int32_t width, int32_t height);

    // Destroy a specific texture
    void DestroyTexture(SDL_Texture* texture);

    // Clear all cached textures
    void ClearCache();

    // Remove textures for tiles that are no longer in the tile cache
    void PruneCache(const TileCache& tileCache);

    // Get cache statistics
    size_t GetCacheSize() const { return textureCache_.size(); }

private:
    SDL_Renderer* renderer_;
    std::unordered_map<TileKey, SDL_Texture*, TileKeyHash> textureCache_;
};
