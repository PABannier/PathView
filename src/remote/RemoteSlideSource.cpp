#include "RemoteSlideSource.h"
#include "../core/SlideLoader.h"  // For LevelDimensions
#include <SDL2/SDL_image.h>
#include <iostream>
#include <cstring>
#include <algorithm>

namespace pathview {
namespace remote {

RemoteSlideSource::RemoteSlideSource(std::shared_ptr<WsiStreamClient> client,
                                       const std::string& slideId)
    : client_(std::move(client))
    , slideId_(slideId)
{
    if (!client_ || !client_->IsConnected()) {
        errorMessage_ = "Client not connected";
        return;
    }

    // Fetch slide metadata
    auto slideInfo = client_->FetchSlideInfo(slideId_);
    if (!slideInfo) {
        errorMessage_ = client_->GetLastError();
        return;
    }

    info_ = std::move(*slideInfo);
    valid_ = true;

    std::cout << "RemoteSlideSource: Loaded " << slideId_
              << " (" << info_.width << "x" << info_.height
              << ", " << info_.levelCount << " levels, tile size " << info_.tileSize << ")"
              << std::endl;
}

RemoteSlideSource::~RemoteSlideSource() = default;

bool RemoteSlideSource::IsValid() const {
    return valid_;
}

const std::string& RemoteSlideSource::GetError() const {
    return errorMessage_;
}

int32_t RemoteSlideSource::GetLevelCount() const {
    return info_.levelCount;
}

LevelDimensions RemoteSlideSource::GetLevelDimensions(int32_t level) const {
    if (level < 0 || level >= info_.levelCount) {
        return {0, 0};
    }

    double downsample = GetLevelDownsample(level);
    return {
        static_cast<int64_t>(info_.width / downsample),
        static_cast<int64_t>(info_.height / downsample)
    };
}

double RemoteSlideSource::GetLevelDownsample(int32_t level) const {
    if (level < 0 || level >= static_cast<int32_t>(info_.downsamples.size())) {
        return 1.0;
    }
    return info_.downsamples[level];
}

int64_t RemoteSlideSource::GetWidth() const {
    return info_.width;
}

int64_t RemoteSlideSource::GetHeight() const {
    return info_.height;
}

std::string RemoteSlideSource::GetIdentifier() const {
    return client_->GetServerUrl() + "/" + slideId_;
}

uint32_t* RemoteSlideSource::ReadRegion(int32_t level, int64_t x, int64_t y,
                                         int64_t width, int64_t height) {
    // The TileLoadThreadPool calls ReadRegion with requests in level-0 coordinates.
    // PathView uses 512px internal tiles, but the server may use different tile sizes (e.g., 256px).
    // We need to fetch all server tiles that cover the requested region and composite them.

    if (!valid_ || !client_ || !client_->IsConnected()) {
        return nullptr;
    }

    double downsample = GetLevelDownsample(level);
    int32_t serverTileSize = info_.tileSize;

    // Convert request from level-0 to level-N coordinates
    int64_t levelX = static_cast<int64_t>(x / downsample);
    int64_t levelY = static_cast<int64_t>(y / downsample);
    int64_t levelW = width;   // Width/height are already in level-N space
    int64_t levelH = height;

    // Calculate which server tiles we need
    int32_t startTileX = static_cast<int32_t>(levelX / serverTileSize);
    int32_t startTileY = static_cast<int32_t>(levelY / serverTileSize);
    int32_t endTileX = static_cast<int32_t>((levelX + levelW - 1) / serverTileSize);
    int32_t endTileY = static_cast<int32_t>((levelY + levelH - 1) / serverTileSize);

    // Allocate output buffer
    uint32_t* output = new uint32_t[width * height];
    std::memset(output, 0, width * height * sizeof(uint32_t));

    // Fetch and composite each tile
    for (int32_t ty = startTileY; ty <= endTileY; ++ty) {
        for (int32_t tx = startTileX; tx <= endTileX; ++tx) {
            int actualTileW = 0, actualTileH = 0;
            uint32_t* tilePixels = FetchAndDecodeTile(level, tx, ty, actualTileW, actualTileH);
            if (!tilePixels || actualTileW <= 0 || actualTileH <= 0) {
                continue;  // Skip failed tiles (they'll appear as black)
            }

            // Calculate where this tile's pixels go in our output
            int64_t tileOriginX = tx * serverTileSize;
            int64_t tileOriginY = ty * serverTileSize;

            // Source region within the tile (respecting actual tile dimensions for edge tiles)
            int64_t srcX = std::max(int64_t(0), levelX - tileOriginX);
            int64_t srcY = std::max(int64_t(0), levelY - tileOriginY);
            int64_t srcW = std::min(static_cast<int64_t>(actualTileW) - srcX,
                                    levelX + levelW - tileOriginX - srcX);
            int64_t srcH = std::min(static_cast<int64_t>(actualTileH) - srcY,
                                    levelY + levelH - tileOriginY - srcY);

            // Clamp to valid ranges
            srcW = std::min(srcW, static_cast<int64_t>(actualTileW) - srcX);
            srcH = std::min(srcH, static_cast<int64_t>(actualTileH) - srcY);

            if (srcW <= 0 || srcH <= 0) {
                delete[] tilePixels;
                continue;
            }

            // Destination position in output buffer
            int64_t dstX = std::max(int64_t(0), tileOriginX - levelX);
            int64_t dstY = std::max(int64_t(0), tileOriginY - levelY);

            // Copy pixels (using actual tile width for source stride)
            for (int64_t row = 0; row < srcH; ++row) {
                int64_t srcOffset = (srcY + row) * actualTileW + srcX;
                int64_t dstOffset = (dstY + row) * width + dstX;
                std::memcpy(&output[dstOffset], &tilePixels[srcOffset], srcW * sizeof(uint32_t));
            }

            delete[] tilePixels;
        }
    }

    return output;
}

uint32_t* RemoteSlideSource::FetchAndDecodeTile(int32_t level,
                                                  int32_t tileX, int32_t tileY,
                                                  int& outWidth, int& outHeight) {
    std::lock_guard<std::mutex> lock(fetchMutex_);

    outWidth = 0;
    outHeight = 0;

    // Retry up to 3 times on failure
    constexpr int MAX_RETRIES = 3;
    for (int attempt = 0; attempt < MAX_RETRIES; ++attempt) {
        auto result = client_->FetchTile(slideId_, level, tileX, tileY);
        if (result.success) {
            uint32_t* pixels = DecodeJpeg(result.jpegData, outWidth, outHeight);
            if (pixels) {
                return pixels;
            }
            // JPEG decode failed - don't retry this
            std::cerr << "RemoteSlideSource: JPEG decode failed for tile ("
                      << level << "," << tileX << "," << tileY << ")" << std::endl;
            return nullptr;
        }

        // Log retry attempts
        if (attempt < MAX_RETRIES - 1) {
            std::cerr << "RemoteSlideSource: Retry " << (attempt + 1) << "/" << (MAX_RETRIES - 1)
                      << " for tile (" << level << "," << tileX << "," << tileY << "): "
                      << result.errorMessage << std::endl;
        } else {
            std::cerr << "RemoteSlideSource: Failed to fetch tile after " << MAX_RETRIES
                      << " attempts (" << level << "," << tileX << "," << tileY << "): "
                      << result.errorMessage << std::endl;
        }
    }

    return nullptr;
}

uint32_t* RemoteSlideSource::DecodeJpeg(const std::vector<uint8_t>& jpegData,
                                          int& outWidth, int& outHeight) {
    // Use SDL_image to decode JPEG
    SDL_RWops* rw = SDL_RWFromConstMem(jpegData.data(), static_cast<int>(jpegData.size()));
    if (!rw) {
        std::cerr << "RemoteSlideSource: Failed to create RWops: " << SDL_GetError() << std::endl;
        return nullptr;
    }

    SDL_Surface* surface = IMG_Load_RW(rw, 1);  // 1 = free RWops after load
    if (!surface) {
        std::cerr << "RemoteSlideSource: Failed to decode JPEG: " << IMG_GetError() << std::endl;
        return nullptr;
    }

    outWidth = surface->w;
    outHeight = surface->h;

    // Convert to RGBA format if needed
    SDL_Surface* rgbaSurface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surface);

    if (!rgbaSurface) {
        std::cerr << "RemoteSlideSource: Failed to convert to RGBA: " << SDL_GetError() << std::endl;
        return nullptr;
    }

    // Copy pixels (caller owns the buffer)
    size_t pixelCount = static_cast<size_t>(rgbaSurface->w) * rgbaSurface->h;
    uint32_t* pixels = new uint32_t[pixelCount];
    std::memcpy(pixels, rgbaSurface->pixels, pixelCount * sizeof(uint32_t));

    SDL_FreeSurface(rgbaSurface);

    return pixels;
}

}  // namespace remote
}  // namespace pathview
