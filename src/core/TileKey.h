#pragma once

#include <cstdint>
#include <functional>
#include <string>

struct TileKey {
    int32_t level;
    int32_t tileX;
    int32_t tileY;

    bool operator==(const TileKey& other) const {
        return level == other.level && tileX == other.tileX && tileY == other.tileY;
    }

    bool operator<(const TileKey& other) const {
        if (level != other.level) return level < other.level;
        if (tileX != other.tileX) return tileX < other.tileX;
        return tileY < other.tileY;
    }

    std::string ToString() const {
        return "L" + std::to_string(level)
            + "_X" + std::to_string(tileX)
            + "_Y" + std::to_string(tileY);
    }
};

// Hash function for TileKey
struct TileKeyHash {
    size_t operator()(const TileKey& k) const noexcept {
        size_t seed = 0;
        size_t h1 = std::hash<int32_t>{}(k.level);
        size_t h2 = std::hash<int32_t>{}(k.tileX);
        size_t h3 = std::hash<int32_t>{}(k.tileY);

        constexpr size_t kHashMix = sizeof(size_t) >= 8
            ? static_cast<size_t>(0x9e3779b97f4a7c15ULL)
            : static_cast<size_t>(0x9e3779b9UL);

        seed ^= h1 + kHashMix + (seed << 6) + (seed >> 2);
        seed ^= h2 + kHashMix + (seed << 6) + (seed >> 2);
        seed ^= h3 + kHashMix + (seed << 6) + (seed >> 2);
        return seed;
    }
};
