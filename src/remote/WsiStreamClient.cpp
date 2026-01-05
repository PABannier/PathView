#include "WsiStreamClient.h"

// Disable deprecated OpenSSL warnings before including httplib
#ifdef __APPLE__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "../../external/cpp-mcp/common/httplib.h"

#ifdef __APPLE__
#pragma clang diagnostic pop
#endif

#include <simdjson.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <cmath>

namespace pathview {
namespace remote {

WsiStreamClient::WsiStreamClient() = default;
WsiStreamClient::~WsiStreamClient() = default;

ConnectionResult WsiStreamClient::Connect(const std::string& serverUrl,
                                           const std::string& authSecret) {
    ConnectionResult result;

    // Normalize URL (remove trailing slash)
    serverUrl_ = serverUrl;
    while (!serverUrl_.empty() && serverUrl_.back() == '/') {
        serverUrl_.pop_back();
    }

    // Create URL signer if auth secret provided
    if (!authSecret.empty()) {
        signer_ = std::make_unique<UrlSigner>(authSecret);
    } else {
        signer_.reset();
    }

    // Test connection with health check
    try {
        httplib::Client client(serverUrl_);
        client.set_connection_timeout(5);
        client.set_read_timeout(10);

        auto res = client.Get("/health");
        if (!res) {
            result.errorMessage = "Connection failed: " + httplib::to_string(res.error());
            connected_ = false;
            return result;
        }

        if (res->status != 200) {
            result.errorMessage = "Health check failed with status " + std::to_string(res->status);
            connected_ = false;
            return result;
        }

        // Parse health response
        simdjson::dom::parser parser;
        auto docResult = parser.parse(res->body);
        if (docResult.error() != simdjson::SUCCESS) {
            result.errorMessage = "Invalid health response JSON";
            connected_ = false;
            return result;
        }

        simdjson::dom::element doc = docResult.value();
        auto statusResult = doc["status"].get_string();
        if (statusResult.error() != simdjson::SUCCESS) {
            result.errorMessage = "Health response missing 'status'";
            connected_ = false;
            return result;
        }

        std::string_view status = statusResult.value();
        if (status != "healthy") {
            result.errorMessage = "Server reports unhealthy status: " + std::string(status);
            connected_ = false;
            return result;
        }

        auto version = doc["version"].get_string();
        if (version.error() == simdjson::SUCCESS) {
            result.serverVersion = std::string(version.value());
        }

        connected_ = true;
        result.success = true;
        std::cout << "WsiStreamClient: Connected to " << serverUrl_
                  << " (version: " << result.serverVersion << ")" << std::endl;

    } catch (const std::exception& e) {
        result.errorMessage = std::string("Connection error: ") + e.what();
        connected_ = false;
    }

    return result;
}

void WsiStreamClient::Disconnect() {
    connected_ = false;
    serverUrl_.clear();
    signer_.reset();
    std::cout << "WsiStreamClient: Disconnected" << std::endl;
}

bool WsiStreamClient::CheckHealth() {
    if (!connected_) {
        return false;
    }

    try {
        httplib::Client client(serverUrl_);
        client.set_connection_timeout(5);
        client.set_read_timeout(5);

        auto res = client.Get("/health");
        return res && res->status == 200;
    } catch (...) {
        return false;
    }
}

std::optional<std::vector<SlideEntry>> WsiStreamClient::FetchSlideList(int limit) {
    if (!connected_) {
        lastError_ = "Not connected";
        return std::nullopt;
    }

    try {
        httplib::Client client(serverUrl_);
        client.set_connection_timeout(5);
        client.set_read_timeout(30);

        std::string path = "/slides";
        std::map<std::string, std::string> params = {
            {"limit", std::to_string(limit)}
        };

        std::string url;
        if (signer_ && signer_->IsEnabled()) {
            url = signer_->BuildSignedUrl(path, params);
        } else {
            url = path + "?limit=" + std::to_string(limit);
        }

        auto res = client.Get(url);
        if (!res) {
            lastError_ = "Request failed: " + httplib::to_string(res.error());
            return std::nullopt;
        }

        if (res->status == 401) {
            lastError_ = "Authentication failed";
            return std::nullopt;
        }

        if (res->status != 200) {
            lastError_ = "Request failed with status " + std::to_string(res->status);
            return std::nullopt;
        }

        // Parse JSON response
        simdjson::dom::parser parser;
        auto docResult = parser.parse(res->body);
        if (docResult.error() != simdjson::SUCCESS) {
            lastError_ = "Invalid slide list JSON";
            return std::nullopt;
        }
        simdjson::dom::element doc = docResult.value();

        std::vector<SlideEntry> slides;

        // Handle both array format and object with "slides" key
        simdjson::dom::array arr;
        if (doc.is_array()) {
            arr = doc.get_array().value();
        } else if (doc.is_object()) {
            auto slidesArr = doc["slides"].get_array();
            if (slidesArr.error() == simdjson::SUCCESS) {
                arr = slidesArr.value();
            }
        }

        for (auto item : arr) {
            SlideEntry entry;

            // Handle both string format and object format
            if (item.is_string()) {
                entry.id = std::string(item.get_string().value());
                entry.name = entry.id;
            } else if (item.is_object()) {
                auto id = item["id"].get_string();
                if (id.error() == simdjson::SUCCESS) {
                    entry.id = std::string(id.value());
                }
                auto name = item["name"].get_string();
                if (name.error() == simdjson::SUCCESS) {
                    entry.name = std::string(name.value());
                } else {
                    entry.name = entry.id;
                }
                auto size = item["size"].get_int64();
                if (size.error() == simdjson::SUCCESS) {
                    entry.size = size.value();
                }
            }

            if (!entry.id.empty()) {
                slides.push_back(std::move(entry));
            }
        }

        std::cout << "WsiStreamClient: Fetched " << slides.size() << " slides" << std::endl;
        return slides;

    } catch (const std::exception& e) {
        lastError_ = std::string("Error fetching slide list: ") + e.what();
        return std::nullopt;
    }
}

std::optional<SlideInfo> WsiStreamClient::FetchSlideInfo(const std::string& slideId) {
    if (!connected_) {
        lastError_ = "Not connected";
        return std::nullopt;
    }

    try {
        httplib::Client client(serverUrl_);
        client.set_connection_timeout(5);
        client.set_read_timeout(30);

        std::string path = "/slides/" + UrlEncodeSlideId(slideId);

        std::string url;
        if (signer_ && signer_->IsEnabled()) {
            url = signer_->BuildSignedUrl(path);
        } else {
            url = path;
        }

        auto res = client.Get(url);
        if (!res) {
            lastError_ = "Request failed: " + httplib::to_string(res.error());
            return std::nullopt;
        }

        if (res->status == 401) {
            lastError_ = "Authentication failed";
            return std::nullopt;
        }

        if (res->status == 404) {
            lastError_ = "Slide not found: " + slideId;
            return std::nullopt;
        }

        if (res->status != 200) {
            lastError_ = "Request failed with status " + std::to_string(res->status);
            return std::nullopt;
        }

        // Parse JSON response
        // API returns: { slide_id, format, width, height, level_count, levels: [...] }
        // Each level: { level, width, height, tile_width, tile_height, tiles_x, tiles_y, downsample }
        simdjson::dom::parser parser;
        auto docResult = parser.parse(res->body);
        if (docResult.error() != simdjson::SUCCESS) {
            lastError_ = "Invalid slide info JSON";
            return std::nullopt;
        }
        simdjson::dom::element doc = docResult.value();

        SlideInfo info;
        info.id = slideId;

        auto width = doc["width"].get_int64();
        if (width.error() == simdjson::SUCCESS) {
            info.width = width.value();
        }

        auto height = doc["height"].get_int64();
        if (height.error() == simdjson::SUCCESS) {
            info.height = height.value();
        }

        auto levelCount = doc["level_count"].get_int64();
        if (levelCount.error() == simdjson::SUCCESS) {
            info.levelCount = static_cast<int32_t>(levelCount.value());
        }

        // Parse levels array to extract tile_size and downsamples
        auto levelsArray = doc["levels"].get_array();
        if (levelsArray.error() == simdjson::SUCCESS) {
            bool gotTileSize = false;
            for (auto levelObj : levelsArray.value()) {
                // Get tile_width from first level (level 0)
                if (!gotTileSize) {
                    auto tileWidth = levelObj["tile_width"].get_int64();
                    if (tileWidth.error() == simdjson::SUCCESS) {
                        info.tileSize = static_cast<int32_t>(tileWidth.value());
                        gotTileSize = true;
                    }
                }

                // Get downsample for each level
                auto downsample = levelObj["downsample"].get_double();
                if (downsample.error() == simdjson::SUCCESS) {
                    info.downsamples.push_back(downsample.value());
                }
            }
        }

        // Fallback: if downsamples not extracted, generate standard pyramid (2x per level)
        if (info.downsamples.empty() && info.levelCount > 0) {
            for (int32_t i = 0; i < info.levelCount; ++i) {
                info.downsamples.push_back(std::pow(2.0, i));
            }
        }

        // Fallback: default tile size if not found
        if (info.tileSize == 0) {
            info.tileSize = 256;  // Common default
        }

        std::cout << "WsiStreamClient: Fetched slide info for " << slideId
                  << " (" << info.width << "x" << info.height
                  << ", " << info.levelCount << " levels)" << std::endl;

        return info;

    } catch (const std::exception& e) {
        lastError_ = std::string("Error fetching slide info: ") + e.what();
        return std::nullopt;
    }
}

TileFetchResult WsiStreamClient::FetchTile(const std::string& slideId,
                                            int32_t level, int32_t x, int32_t y,
                                            int quality) {
    TileFetchResult result;

    if (!connected_) {
        result.errorMessage = "Not connected";
        return result;
    }

    try {
        httplib::Client client(serverUrl_);
        client.set_connection_timeout(5);
        client.set_read_timeout(30);

        std::ostringstream pathStream;
        pathStream << "/tiles/" << UrlEncodeSlideId(slideId)
                   << "/" << level << "/" << x << "/" << y << ".jpg";
        std::string path = pathStream.str();

        std::map<std::string, std::string> params = {
            {"quality", std::to_string(quality)}
        };

        std::string url;
        if (signer_ && signer_->IsEnabled()) {
            url = signer_->BuildSignedUrl(path, params);
        } else {
            url = path + "?quality=" + std::to_string(quality);
        }

        auto res = client.Get(url);
        if (!res) {
            result.errorMessage = "Request failed: " + httplib::to_string(res.error());
            return result;
        }

        result.httpStatus = res->status;

        if (res->status == 401) {
            result.errorMessage = "Authentication failed";
            return result;
        }

        if (res->status == 404) {
            result.errorMessage = "Tile not found";
            return result;
        }

        if (res->status != 200) {
            result.errorMessage = "Request failed with status " + std::to_string(res->status);
            return result;
        }

        // Copy JPEG data
        result.jpegData.assign(res->body.begin(), res->body.end());
        result.success = true;

        return result;

    } catch (const std::exception& e) {
        result.errorMessage = std::string("Error fetching tile: ") + e.what();
        return result;
    }
}

std::string WsiStreamClient::UrlEncodeSlideId(const std::string& slideId) {
    std::ostringstream oss;
    oss << std::hex << std::uppercase << std::setfill('0');

    for (char c : slideId) {
        if (std::isalnum(static_cast<unsigned char>(c)) ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            oss << c;
        } else {
            oss << '%' << std::setw(2)
                << static_cast<int>(static_cast<unsigned char>(c));
        }
    }
    return oss.str();
}

}  // namespace remote
}  // namespace pathview
