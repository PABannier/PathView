#include "UrlSigner.h"
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

namespace pathview {
namespace remote {

UrlSigner::UrlSigner(const std::string& secret)
    : secret_(secret)
{
}

std::string UrlSigner::Sign(const std::string& path,
                            const std::map<std::string, std::string>& params,
                            int64_t validitySeconds) const {
    if (secret_.empty()) {
        // No signing needed, just return existing params as query string
        return BuildCanonicalQuery(params);
    }

    // Create a mutable copy of params
    std::map<std::string, std::string> signedParams = params;

    // Add expiration timestamp
    auto now = std::chrono::system_clock::now();
    auto expTime = now + std::chrono::seconds(validitySeconds);
    int64_t exp = std::chrono::duration_cast<std::chrono::seconds>(
        expTime.time_since_epoch()).count();
    signedParams["exp"] = std::to_string(exp);

    // Build canonical query string (without sig)
    std::string canonicalQuery = BuildCanonicalQuery(signedParams);

    // Build signature base string: {path}?{canonical_query}
    std::string signatureBase = path + "?" + canonicalQuery;

    // Compute HMAC-SHA256 signature
    std::string sig = ComputeHmacSha256(signatureBase);

    // Add signature to query string
    if (!canonicalQuery.empty()) {
        canonicalQuery += "&";
    }
    canonicalQuery += "sig=" + sig;

    return canonicalQuery;
}

std::string UrlSigner::BuildSignedUrl(const std::string& path,
                                      const std::map<std::string, std::string>& params,
                                      int64_t validitySeconds) const {
    std::string query = Sign(path, params, validitySeconds);
    if (query.empty()) {
        return path;
    }
    return path + "?" + query;
}

std::string UrlSigner::BuildCanonicalQuery(
    const std::map<std::string, std::string>& params) const {
    // std::map is already sorted by key
    std::ostringstream oss;
    bool first = true;
    for (const auto& [key, value] : params) {
        if (!first) {
            oss << "&";
        }
        oss << UrlEncode(key) << "=" << UrlEncode(value);
        first = false;
    }
    return oss.str();
}

std::string UrlSigner::ComputeHmacSha256(const std::string& message) const {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digestLen = 0;

    HMAC(EVP_sha256(),
         secret_.data(), static_cast<int>(secret_.size()),
         reinterpret_cast<const unsigned char*>(message.data()),
         message.size(),
         digest, &digestLen);

    // Convert to hex string
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < digestLen; ++i) {
        oss << std::setw(2) << static_cast<int>(digest[i]);
    }
    return oss.str();
}

std::string UrlSigner::UrlEncode(const std::string& str) {
    std::ostringstream oss;
    oss << std::hex << std::uppercase << std::setfill('0');

    for (char c : str) {
        // Unreserved characters (RFC 3986)
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
