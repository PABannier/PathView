#pragma once

#include "../core/ISlideSource.h"
#include "WsiStreamClient.h"
#include <memory>
#include <string>
#include <vector>
#include <mutex>

namespace pathview {
namespace remote {

class RemoteSlideSource : public ISlideSource {
public:
    // Create a remote slide source from an existing connected client
    RemoteSlideSource(std::shared_ptr<WsiStreamClient> client,
                      const std::string& slideId);
    ~RemoteSlideSource() override;

    // ISlideSource interface
    bool IsValid() const override;
    const std::string& GetError() const override;
    int32_t GetLevelCount() const override;
    LevelDimensions GetLevelDimensions(int32_t level) const override;
    double GetLevelDownsample(int32_t level) const override;
    int64_t GetWidth() const override;
    int64_t GetHeight() const override;
    uint32_t* ReadRegion(int32_t level, int64_t x, int64_t y,
                         int64_t width, int64_t height) override;
    std::string GetIdentifier() const override;
    bool IsRemote() const override { return true; }

    // Additional accessors
    const std::string& GetSlideId() const { return slideId_; }
    int32_t GetTileSize() const { return info_.tileSize; }

private:
    // Fetch and decode a single tile, returns actual decoded dimensions
    uint32_t* FetchAndDecodeTile(int32_t level, int32_t tileX, int32_t tileY,
                                  int& outWidth, int& outHeight);

    // Decode JPEG to RGBA pixels
    uint32_t* DecodeJpeg(const std::vector<uint8_t>& jpegData,
                          int& outWidth, int& outHeight);

    std::shared_ptr<WsiStreamClient> client_;
    std::string slideId_;
    SlideInfo info_;
    std::string errorMessage_;
    bool valid_ = false;

    // Thread safety for HTTP requests
    mutable std::mutex fetchMutex_;
};

}  // namespace remote
}  // namespace pathview
