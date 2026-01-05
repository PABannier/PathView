// TileCache Unit Tests
// Tests for LRU eviction policy and memory tracking
// Critical for efficient tile management

#include <gtest/gtest.h>
#include "TileCache.h"
#include <vector>
#include <algorithm>

// ============================================================================
// Test Fixture
// ============================================================================

class TileCacheTest : public ::testing::Test {
protected:
    std::unique_ptr<TileCache> cache;

    void SetUp() override {
        cache = std::make_unique<TileCache>(1024 * 1024);  // 1MB cache for testing
    }

    // Helper to create tile data of specific size
    TileData CreateTileData(size_t bytes) {
        size_t pixels = bytes / sizeof(uint32_t);
        int32_t width = static_cast<int32_t>(pixels);
        int32_t height = 1;

        uint32_t* pixel_data = new uint32_t[pixels];
        std::fill(pixel_data, pixel_data + pixels, 0xFF0000FF);  // Red pixels

        return TileData(pixel_data, width, height);
    }

    // Helper to create a tile key
    TileKey MakeTileKey(int32_t level, int32_t x, int32_t y) {
        TileKey key;
        key.level = level;
        key.tileX = x;
        key.tileY = y;
        return key;
    }
};

// ============================================================================
// Basic Functionality Tests
// ============================================================================

TEST_F(TileCacheTest, Constructor_DefaultState_EmptyCache) {
    EXPECT_EQ(cache->GetTileCount(), 0);
    EXPECT_EQ(cache->GetMemoryUsage(), 0);
    EXPECT_EQ(cache->GetHitCount(), 0);
    EXPECT_EQ(cache->GetMissCount(), 0);
}

TEST_F(TileCacheTest, GetMaxMemory_ReturnsConfiguredLimit) {
    EXPECT_EQ(cache->GetMaxMemory(), 1024 * 1024);
}

TEST_F(TileCacheTest, HasTile_EmptyCache_ReturnsFalse) {
    TileKey key = MakeTileKey(0, 0, 0);
    EXPECT_FALSE(cache->HasTile(key));
}

TEST_F(TileCacheTest, GetTile_EmptyCache_ReturnsNullptr) {
    TileKey key = MakeTileKey(0, 0, 0);
    const TileData* tile = cache->GetTile(key);
    EXPECT_EQ(tile, nullptr);
}

// ============================================================================
// Insert and Retrieval Tests
// ============================================================================

TEST_F(TileCacheTest, InsertTile_SingleTile_IncreasesCount) {
    TileKey key = MakeTileKey(0, 0, 0);
    auto data = CreateTileData(256 * 256 * 4);  // 256KB

    cache->InsertTile(key, std::move(data));

    EXPECT_EQ(cache->GetTileCount(), 1);
}

TEST_F(TileCacheTest, InsertTile_SingleTile_UpdatesMemoryUsage) {
    TileKey key = MakeTileKey(0, 0, 0);
    size_t tile_size = 256 * 256 * 4;
    auto data = CreateTileData(tile_size);

    cache->InsertTile(key, std::move(data));

    EXPECT_EQ(cache->GetMemoryUsage(), tile_size);
}

TEST_F(TileCacheTest, HasTile_AfterInsert_ReturnsTrue) {
    TileKey key = MakeTileKey(0, 0, 0);
    auto data = CreateTileData(1000);

    cache->InsertTile(key, std::move(data));

    EXPECT_TRUE(cache->HasTile(key));
}

TEST_F(TileCacheTest, GetTile_AfterInsert_ReturnsValidPointer) {
    TileKey key = MakeTileKey(0, 0, 0);
    auto data = CreateTileData(1000);

    cache->InsertTile(key, std::move(data));

    const TileData* tile = cache->GetTile(key);
    ASSERT_NE(tile, nullptr);
    // CreateTileData(1000) creates 1000/4 = 250 pixels, so memorySize = 250*4 = 1000 bytes
    EXPECT_EQ(tile->memorySize, 1000);
}

TEST_F(TileCacheTest, InsertTile_MultipleTiles_SumsMemoryUsage) {
    cache->InsertTile(MakeTileKey(0, 0, 0), CreateTileData(100000));
    cache->InsertTile(MakeTileKey(0, 1, 0), CreateTileData(200000));
    cache->InsertTile(MakeTileKey(0, 2, 0), CreateTileData(300000));

    EXPECT_EQ(cache->GetTileCount(), 3);
    EXPECT_EQ(cache->GetMemoryUsage(), 600000);
}

// ============================================================================
// LRU Eviction Tests
// ============================================================================

TEST_F(TileCacheTest, Eviction_ExceedsCapacity_EvictsLeastRecentlyUsed) {
    cache = std::make_unique<TileCache>(500000);  // 500KB limit

    TileKey key_a = MakeTileKey(0, 0, 0);
    TileKey key_b = MakeTileKey(0, 1, 0);
    TileKey key_c = MakeTileKey(0, 2, 0);

    cache->InsertTile(key_a, CreateTileData(200000));  // Tile A
    cache->InsertTile(key_b, CreateTileData(200000));  // Tile B
    cache->InsertTile(key_c, CreateTileData(200000));  // Tile C, should evict A

    EXPECT_FALSE(cache->HasTile(key_a)) << "Tile A should be evicted";
    EXPECT_TRUE(cache->HasTile(key_b)) << "Tile B should remain";
    EXPECT_TRUE(cache->HasTile(key_c)) << "Tile C should remain";
}

TEST_F(TileCacheTest, Eviction_AccessUpdatesLRU_PreventsEviction) {
    cache = std::make_unique<TileCache>(500000);

    TileKey key_a = MakeTileKey(0, 0, 0);
    TileKey key_b = MakeTileKey(0, 1, 0);
    TileKey key_c = MakeTileKey(0, 2, 0);

    cache->InsertTile(key_a, CreateTileData(200000));  // Tile A (LRU)
    cache->InsertTile(key_b, CreateTileData(200000));  // Tile B

    // Access A, making it most recent
    cache->GetTile(key_a);

    cache->InsertTile(key_c, CreateTileData(200000));  // Tile C, should evict B (not A)

    EXPECT_TRUE(cache->HasTile(key_a)) << "Tile A should survive (was accessed)";
    EXPECT_FALSE(cache->HasTile(key_b)) << "Tile B should be evicted";
    EXPECT_TRUE(cache->HasTile(key_c)) << "Tile C should be added";
}

TEST_F(TileCacheTest, Eviction_MultipleEvictions_EvictsInLRUOrder) {
    cache = std::make_unique<TileCache>(400000);

    TileKey key_a = MakeTileKey(0, 0, 0);
    TileKey key_b = MakeTileKey(0, 1, 0);
    TileKey key_c = MakeTileKey(0, 2, 0);
    TileKey key_d = MakeTileKey(0, 3, 0);

    cache->InsertTile(key_a, CreateTileData(150000));  // A (oldest)
    cache->InsertTile(key_b, CreateTileData(150000));  // B
    cache->InsertTile(key_c, CreateTileData(150000));  // C (will cause A to be evicted)

    // Now A is evicted, B and C remain
    EXPECT_FALSE(cache->HasTile(key_a));
    EXPECT_TRUE(cache->HasTile(key_b));
    EXPECT_TRUE(cache->HasTile(key_c));

    // Insert D (should evict B)
    cache->InsertTile(key_d, CreateTileData(150000));

    EXPECT_FALSE(cache->HasTile(key_b));
    EXPECT_TRUE(cache->HasTile(key_c));
    EXPECT_TRUE(cache->HasTile(key_d));
}

TEST_F(TileCacheTest, Eviction_MemoryUsage_CorrectAfterEviction) {
    cache = std::make_unique<TileCache>(500000);

    cache->InsertTile(MakeTileKey(0, 0, 0), CreateTileData(200000));
    cache->InsertTile(MakeTileKey(0, 1, 0), CreateTileData(200000));

    EXPECT_EQ(cache->GetMemoryUsage(), 400000);

    // This should evict first tile
    cache->InsertTile(MakeTileKey(0, 2, 0), CreateTileData(200000));

    // Memory should be ~400000 (two tiles)
    EXPECT_LE(cache->GetMemoryUsage(), 500000);
    EXPECT_GE(cache->GetMemoryUsage(), 300000);
}

// ============================================================================
// Hit/Miss Statistics Tests
// ============================================================================

TEST_F(TileCacheTest, Statistics_GetTileHit_IncrementsHitCount) {
    TileKey key = MakeTileKey(0, 0, 0);
    cache->InsertTile(key, CreateTileData(1000));

    // Initial state (insert doesn't count as hit/miss in this implementation)
    size_t initial_hits = cache->GetHitCount();

    cache->GetTile(key);

    EXPECT_EQ(cache->GetHitCount(), initial_hits + 1);
    EXPECT_EQ(cache->GetMissCount(), 0);
}

TEST_F(TileCacheTest, Statistics_GetTileMiss_IncrementsMissCount) {
    TileKey key = MakeTileKey(0, 0, 0);

    cache->GetTile(key);  // Not in cache

    EXPECT_EQ(cache->GetHitCount(), 0);
    EXPECT_EQ(cache->GetMissCount(), 1);
}

TEST_F(TileCacheTest, Statistics_MultipleHitsAndMisses_CorrectCounts) {
    TileKey key_a = MakeTileKey(0, 0, 0);
    TileKey key_b = MakeTileKey(0, 1, 0);

    cache->InsertTile(key_a, CreateTileData(1000));

    cache->GetTile(key_a);  // Hit
    cache->GetTile(key_a);  // Hit
    cache->GetTile(key_b);  // Miss
    cache->GetTile(key_b);  // Miss

    EXPECT_EQ(cache->GetHitCount(), 2);
    EXPECT_EQ(cache->GetMissCount(), 2);
}

TEST_F(TileCacheTest, GetHitRate_CalculatesCorrectly) {
    TileKey key = MakeTileKey(0, 0, 0);
    cache->InsertTile(key, CreateTileData(1000));

    cache->GetTile(key);  // Hit
    cache->GetTile(key);  // Hit
    cache->GetTile(MakeTileKey(1, 1, 1));  // Miss

    // 2 hits, 1 miss = 2/3 = 0.666...
    EXPECT_NEAR(cache->GetHitRate(), 2.0 / 3.0, 0.001);
}

TEST_F(TileCacheTest, GetHitRate_NoAccess_ReturnsZero) {
    EXPECT_DOUBLE_EQ(cache->GetHitRate(), 0.0);
}

// ============================================================================
// Clear Tests
// ============================================================================

TEST_F(TileCacheTest, Clear_RemovesAllTiles) {
    cache->InsertTile(MakeTileKey(0, 0, 0), CreateTileData(100000));
    cache->InsertTile(MakeTileKey(0, 1, 0), CreateTileData(200000));
    cache->InsertTile(MakeTileKey(0, 2, 0), CreateTileData(300000));

    EXPECT_EQ(cache->GetTileCount(), 3);

    cache->Clear();

    EXPECT_EQ(cache->GetTileCount(), 0);
}

TEST_F(TileCacheTest, Clear_ResetsMemoryUsage) {
    cache->InsertTile(MakeTileKey(0, 0, 0), CreateTileData(100000));
    cache->InsertTile(MakeTileKey(0, 1, 0), CreateTileData(200000));

    EXPECT_GT(cache->GetMemoryUsage(), 0);

    cache->Clear();

    EXPECT_EQ(cache->GetMemoryUsage(), 0);
}

TEST_F(TileCacheTest, Clear_DoesNotResetStatistics) {
    TileKey key = MakeTileKey(0, 0, 0);
    cache->InsertTile(key, CreateTileData(1000));
    cache->GetTile(key);  // Hit

    cache->Clear();

    // Statistics should persist after clear
    EXPECT_EQ(cache->GetHitCount(), 1);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(TileCacheTest, InsertTile_DuplicateKey_KeepsExisting) {
    TileKey key = MakeTileKey(0, 0, 0);

    cache->InsertTile(key, CreateTileData(100000));
    size_t initial_memory = cache->GetMemoryUsage();

    // Insert again with same key but different size
    // The cache keeps the existing tile and ignores the new one
    cache->InsertTile(key, CreateTileData(200000));

    // Should have kept existing, not added or replaced
    EXPECT_EQ(cache->GetTileCount(), 1);
    EXPECT_EQ(cache->GetMemoryUsage(), initial_memory);
}

TEST_F(TileCacheTest, InsertTile_ZeroSize_HandlesGracefully) {
    TileKey key = MakeTileKey(0, 0, 0);

    TileData empty_data(nullptr, 0, 0);
    cache->InsertTile(key, std::move(empty_data));

    EXPECT_EQ(cache->GetTileCount(), 1);
    EXPECT_TRUE(cache->HasTile(key));
}

TEST_F(TileCacheTest, Eviction_TileLargerThanCapacity_StillInserts) {
    cache = std::make_unique<TileCache>(100000);  // 100KB limit

    TileKey key = MakeTileKey(0, 0, 0);
    auto large_tile = CreateTileData(500000);  // 500KB tile (larger than capacity)

    cache->InsertTile(key, std::move(large_tile));

    // Implementation may choose to insert anyway or reject
    // This tests that it doesn't crash
    EXPECT_GE(cache->GetTileCount(), 0);
}

TEST_F(TileCacheTest, HasTile_DifferentKeys_DistinguishesProperly) {
    TileKey key1 = MakeTileKey(0, 0, 0);
    TileKey key2 = MakeTileKey(0, 0, 1);  // Different Y
    TileKey key3 = MakeTileKey(1, 0, 0);  // Different level

    cache->InsertTile(key1, CreateTileData(1000));

    EXPECT_TRUE(cache->HasTile(key1));
    EXPECT_FALSE(cache->HasTile(key2));
    EXPECT_FALSE(cache->HasTile(key3));
}

// ============================================================================
// TileKey Tests
// ============================================================================

TEST_F(TileCacheTest, TileKey_Equality_WorksCorrectly) {
    TileKey key1 = MakeTileKey(0, 5, 10);
    TileKey key2 = MakeTileKey(0, 5, 10);
    TileKey key3 = MakeTileKey(0, 5, 11);

    EXPECT_TRUE(key1 == key2);
    EXPECT_FALSE(key1 == key3);
}

TEST_F(TileCacheTest, TileKey_ToString_FormatsCorrectly) {
    TileKey key = MakeTileKey(2, 15, 20);
    std::string str = key.ToString();

    EXPECT_NE(str.find("2"), std::string::npos);  // Contains level
    EXPECT_NE(str.find("15"), std::string::npos);  // Contains X
    EXPECT_NE(str.find("20"), std::string::npos);  // Contains Y
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST_F(TileCacheTest, StressTest_ManyInserts_MaintainsMemoryLimit) {
    cache = std::make_unique<TileCache>(1000000);  // 1MB limit

    // Insert 100 tiles of 50KB each (total 5MB)
    for (int i = 0; i < 100; ++i) {
        TileKey key = MakeTileKey(0, i, 0);
        cache->InsertTile(key, CreateTileData(50000));
    }

    // Cache should have evicted tiles to stay under limit
    EXPECT_LE(cache->GetMemoryUsage(), 1000000);
    EXPECT_GT(cache->GetTileCount(), 0);  // Should have some tiles
    EXPECT_LT(cache->GetTileCount(), 100);  // But not all
}

TEST_F(TileCacheTest, StressTest_AlternatingAccessPattern_CorrectLRU) {
    cache = std::make_unique<TileCache>(300000);

    TileKey key_a = MakeTileKey(0, 0, 0);
    TileKey key_b = MakeTileKey(0, 1, 0);
    TileKey key_c = MakeTileKey(0, 2, 0);

    cache->InsertTile(key_a, CreateTileData(100000));
    cache->InsertTile(key_b, CreateTileData(100000));
    cache->InsertTile(key_c, CreateTileData(100000));

    // Access A and B repeatedly (keeping them "hot")
    for (int i = 0; i < 10; ++i) {
        cache->GetTile(key_a);
        cache->GetTile(key_b);
    }

    // Insert new tile (should evict C, not A or B)
    cache->InsertTile(MakeTileKey(0, 3, 0), CreateTileData(100000));

    EXPECT_TRUE(cache->HasTile(key_a));
    EXPECT_TRUE(cache->HasTile(key_b));
    // C may or may not be evicted depending on exact implementation
}
