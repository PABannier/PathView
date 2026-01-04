#pragma once

#include "UrlSigner.h"
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <cstdint>

namespace pathview {
namespace remote {

// Slide metadata from /slides/{id}
struct SlideInfo {
    std::string id;
    int64_t width = 0;
    int64_t height = 0;
    int32_t levelCount = 0;
    int32_t tileSize = 512;
    std::vector<double> downsamples;  // Per-level downsample factors
};

// Slide entry from /slides list
struct SlideEntry {
    std::string id;
    std::string name;
    int64_t size = 0;  // File size in bytes (if available)
};

// Connection result
struct ConnectionResult {
    bool success = false;
    std::string serverVersion;
    std::string errorMessage;
};

// HTTP fetch result for tiles
struct TileFetchResult {
    bool success = false;
    std::vector<uint8_t> jpegData;
    std::string errorMessage;
    int httpStatus = 0;
};

class WsiStreamClient {
public:
    WsiStreamClient();
    ~WsiStreamClient();

    // Connection management
    ConnectionResult Connect(const std::string& serverUrl,
                             const std::string& authSecret = "");
    void Disconnect();
    bool IsConnected() const { return connected_; }
    const std::string& GetServerUrl() const { return serverUrl_; }

    // API methods
    std::optional<std::vector<SlideEntry>> FetchSlideList(int limit = 100);
    std::optional<SlideInfo> FetchSlideInfo(const std::string& slideId);
    TileFetchResult FetchTile(const std::string& slideId,
                               int32_t level, int32_t x, int32_t y,
                               int quality = 80);

    // Health check
    bool CheckHealth();

    // Error info
    const std::string& GetLastError() const { return lastError_; }

private:
    // URL encode slide ID (handles / in paths)
    static std::string UrlEncodeSlideId(const std::string& slideId);

    std::string serverUrl_;
    std::unique_ptr<UrlSigner> signer_;
    bool connected_ = false;
    std::string lastError_;
};

}  // namespace remote
}  // namespace pathview
