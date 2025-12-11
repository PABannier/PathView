#include "TextureManager.h"
#include <iostream>
#include <sstream>

std::string TileKey::ToString() const {
    std::ostringstream oss;
    oss << "L" << level << "_X" << tileX << "_Y" << tileY;
    return oss.str();
}

TextureManager::TextureManager(SDL_Renderer* renderer)
    : renderer_(renderer)
{
    if (!renderer_) {
        std::cerr << "TextureManager: renderer is null" << std::endl;
    }
}

TextureManager::~TextureManager() {
    ClearCache();
}

SDL_Texture* TextureManager::CreateTexture(const uint32_t* pixels, int32_t width, int32_t height) {
    if (!renderer_) {
        std::cerr << "TextureManager: renderer is null" << std::endl;
        return nullptr;
    }

    if (!pixels) {
        std::cerr << "TextureManager: pixel data is null" << std::endl;
        return nullptr;
    }

    // Create SDL texture
    SDL_Texture* texture = SDL_CreateTexture(
        renderer_,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STATIC,
        width,
        height
    );

    if (!texture) {
        std::cerr << "Failed to create texture: " << SDL_GetError() << std::endl;
        return nullptr;
    }

    // Upload pixel data
    int pitch = width * sizeof(uint32_t);
    if (SDL_UpdateTexture(texture, nullptr, pixels, pitch) != 0) {
        std::cerr << "Failed to update texture: " << SDL_GetError() << std::endl;
        SDL_DestroyTexture(texture);
        return nullptr;
    }

    return texture;
}

SDL_Texture* TextureManager::GetOrCreateTexture(const TileKey& key, const uint32_t* pixels, int32_t width, int32_t height) {
    // Check if texture already exists in cache
    auto it = textureCache_.find(key);
    if (it != textureCache_.end()) {
        return it->second;
    }

    // Create new texture
    SDL_Texture* texture = CreateTexture(pixels, width, height);
    if (!texture) {
        return nullptr;
    }

    // Add to cache
    textureCache_[key] = texture;

    return texture;
}

void TextureManager::DestroyTexture(SDL_Texture* texture) {
    if (!texture) {
        return;
    }

    // Remove from cache if present
    for (auto it = textureCache_.begin(); it != textureCache_.end(); ++it) {
        if (it->second == texture) {
            textureCache_.erase(it);
            break;
        }
    }

    SDL_DestroyTexture(texture);
}

void TextureManager::ClearCache() {
    for (auto& pair : textureCache_) {
        if (pair.second) {
            SDL_DestroyTexture(pair.second);
        }
    }
    textureCache_.clear();
}
