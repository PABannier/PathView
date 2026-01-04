#pragma once

#include <string>
#include <map>
#include <cstdint>

namespace pathview {
namespace remote {

class UrlSigner {
public:
    explicit UrlSigner(const std::string& secret);

    // Sign a URL path with query parameters
    // Returns the full query string including exp and sig parameters
    std::string Sign(const std::string& path,
                     const std::map<std::string, std::string>& params = {},
                     int64_t validitySeconds = 300) const;

    // Build a signed URL (path + signed query string)
    std::string BuildSignedUrl(const std::string& path,
                                const std::map<std::string, std::string>& params = {},
                                int64_t validitySeconds = 300) const;

    // Check if signing is enabled (secret is not empty)
    bool IsEnabled() const { return !secret_.empty(); }

private:
    // Build canonical query string (sorted by key)
    std::string BuildCanonicalQuery(const std::map<std::string, std::string>& params) const;

    // Compute HMAC-SHA256 and return hex string
    std::string ComputeHmacSha256(const std::string& message) const;

    // URL-encode a string
    static std::string UrlEncode(const std::string& str);

    std::string secret_;
};

}  // namespace remote
}  // namespace pathview
