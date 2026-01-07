#pragma once

#include <iomanip>
#include <random>
#include <sstream>
#include <string>

namespace pathview {
namespace util {

inline std::string GenerateUUID() {
    thread_local std::mt19937_64 gen{std::random_device{}()};
    std::uniform_int_distribution<uint64_t> dis;

    uint64_t ab = dis(gen);
    uint64_t cd = dis(gen);

    // Set UUID version 4 and variant bits
    ab = (ab & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    cd = (cd & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

    std::ostringstream ss;
    ss << std::hex << std::setfill('0')
       << std::setw(8) << (ab >> 32) << "-"
       << std::setw(4) << ((ab >> 16) & 0xFFFF) << "-"
       << std::setw(4) << (ab & 0xFFFF) << "-"
       << std::setw(4) << (cd >> 48) << "-"
       << std::setw(12) << (cd & 0xFFFFFFFFFFFFULL);
    return ss.str();
}

}  // namespace util
}  // namespace pathview
