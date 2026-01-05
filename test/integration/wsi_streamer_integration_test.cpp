// ==============================================================================
// WSI Streamer Integration Tests
// ==============================================================================
//
// These tests verify that PathView's WsiStreamClient can correctly communicate
// with a running WSIStreamer server. The server is expected to be running
// (via docker-compose) with a test slide uploaded to MinIO.
//
// Required environment variables:
//   WSI_STREAMER_URL  - Server URL (default: http://localhost:3000)
//   WSI_TEST_SLIDE_ID - Name of the uploaded test slide
//
// Run via: ./test/integration/wsi_streamer_e2e.sh <slide.svs>
//
// ==============================================================================

#include <gtest/gtest.h>
#include "WsiStreamClient.h"
#include <cstdlib>
#include <vector>
#include <iostream>
#include <memory>

namespace pathview {
namespace remote {
namespace test {

// ==============================================================================
// Test Fixture
// ==============================================================================

class WsiStreamerIntegrationTest : public ::testing::Test {
protected:
    static std::string serverUrl_;
    static std::string slideId_;
    std::unique_ptr<WsiStreamClient> client_;

    static void SetUpTestSuite() {
        // Read from environment variables set by bash script
        const char* url = std::getenv("WSI_STREAMER_URL");
        const char* slide = std::getenv("WSI_TEST_SLIDE_ID");

        serverUrl_ = url ? url : "http://localhost:3000";
        slideId_ = slide ? slide : "";

        std::cout << "Test Configuration:" << std::endl;
        std::cout << "  Server URL: " << serverUrl_ << std::endl;
        std::cout << "  Slide ID: " << (slideId_.empty() ? "(not set)" : slideId_) << std::endl;

        if (slideId_.empty()) {
            GTEST_SKIP() << "WSI_TEST_SLIDE_ID not set - skipping integration tests";
        }
    }

    void SetUp() override {
        if (slideId_.empty()) {
            GTEST_SKIP() << "WSI_TEST_SLIDE_ID not set";
        }
        client_ = std::make_unique<WsiStreamClient>();
    }

    void TearDown() override {
        if (client_ && client_->IsConnected()) {
            client_->Disconnect();
        }
    }

    // Helper: Verify JPEG magic bytes (FF D8 FF)
    static bool IsValidJpeg(const std::vector<uint8_t>& data) {
        return data.size() >= 3 &&
               data[0] == 0xFF &&
               data[1] == 0xD8 &&
               data[2] == 0xFF;
    }

    // Helper: Verify JPEG end marker (FF D9)
    static bool HasJpegEndMarker(const std::vector<uint8_t>& data) {
        return data.size() >= 2 &&
               data[data.size() - 2] == 0xFF &&
               data[data.size() - 1] == 0xD9;
    }
};

// Static member initialization
std::string WsiStreamerIntegrationTest::serverUrl_;
std::string WsiStreamerIntegrationTest::slideId_;

// ==============================================================================
// Connection Tests
// ==============================================================================

TEST_F(WsiStreamerIntegrationTest, Connect_HealthCheck_ReturnsHealthy) {
    // Act
    auto result = client_->Connect(serverUrl_);

    // Assert
    EXPECT_TRUE(result.success) << "Connection failed: " << result.errorMessage;
    EXPECT_TRUE(client_->IsConnected());
    EXPECT_FALSE(result.serverVersion.empty()) << "Server version should be present";
    EXPECT_TRUE(client_->CheckHealth());

    std::cout << "Server version: " << result.serverVersion << std::endl;
}

TEST_F(WsiStreamerIntegrationTest, Reconnect_AfterDisconnect_Succeeds) {
    // Arrange - connect first
    auto result1 = client_->Connect(serverUrl_);
    ASSERT_TRUE(result1.success) << result1.errorMessage;
    ASSERT_TRUE(client_->IsConnected());

    // Act - disconnect and reconnect
    client_->Disconnect();
    EXPECT_FALSE(client_->IsConnected());

    auto result2 = client_->Connect(serverUrl_);

    // Assert
    EXPECT_TRUE(result2.success) << result2.errorMessage;
    EXPECT_TRUE(client_->IsConnected());
    EXPECT_TRUE(client_->CheckHealth());
}

// ==============================================================================
// Slide List Tests
// ==============================================================================

TEST_F(WsiStreamerIntegrationTest, FetchSlideList_ReturnsUploadedSlide) {
    // Arrange
    auto connectResult = client_->Connect(serverUrl_);
    ASSERT_TRUE(connectResult.success) << connectResult.errorMessage;

    // Act
    auto slides = client_->FetchSlideList();

    // Assert
    ASSERT_TRUE(slides.has_value()) << "Failed to fetch slide list: " << client_->GetLastError();
    EXPECT_GE(slides->size(), 1u) << "Expected at least one slide";

    // Verify our test slide is in the list
    bool foundSlide = false;
    for (const auto& entry : *slides) {
        std::cout << "Found slide: " << entry.id << " (name: " << entry.name << ")" << std::endl;
        if (entry.id == slideId_ || entry.name == slideId_) {
            foundSlide = true;
        }
    }
    EXPECT_TRUE(foundSlide) << "Test slide '" << slideId_ << "' not found in slide list";
}

// ==============================================================================
// Slide Metadata Tests
// ==============================================================================

TEST_F(WsiStreamerIntegrationTest, FetchSlideInfo_ReturnsValidMetadata) {
    // Arrange
    auto connectResult = client_->Connect(serverUrl_);
    ASSERT_TRUE(connectResult.success) << connectResult.errorMessage;

    // Act
    auto info = client_->FetchSlideInfo(slideId_);

    // Assert
    ASSERT_TRUE(info.has_value()) << "Failed to fetch slide info: " << client_->GetLastError();

    EXPECT_EQ(info->id, slideId_);
    EXPECT_GT(info->width, 0) << "Width should be positive";
    EXPECT_GT(info->height, 0) << "Height should be positive";
    EXPECT_GT(info->levelCount, 0) << "Level count should be positive";
    EXPECT_GT(info->tileSize, 0) << "Tile size should be positive";

    // Verify downsamples array is populated
    EXPECT_EQ(info->downsamples.size(), static_cast<size_t>(info->levelCount));
    if (!info->downsamples.empty()) {
        EXPECT_DOUBLE_EQ(info->downsamples[0], 1.0) << "Level 0 should have downsample 1.0";
    }

    // Log metadata for debugging
    std::cout << "Slide Metadata:" << std::endl;
    std::cout << "  ID: " << info->id << std::endl;
    std::cout << "  Dimensions: " << info->width << "x" << info->height << std::endl;
    std::cout << "  Levels: " << info->levelCount << std::endl;
    std::cout << "  Tile size: " << info->tileSize << std::endl;
    std::cout << "  Downsamples: ";
    for (double ds : info->downsamples) {
        std::cout << ds << " ";
    }
    std::cout << std::endl;
}

TEST_F(WsiStreamerIntegrationTest, FetchSlideInfo_NonexistentSlide_ReturnsError) {
    // Arrange
    auto connectResult = client_->Connect(serverUrl_);
    ASSERT_TRUE(connectResult.success) << connectResult.errorMessage;

    // Act
    auto info = client_->FetchSlideInfo("nonexistent_slide_12345.svs");

    // Assert
    EXPECT_FALSE(info.has_value());
    EXPECT_FALSE(client_->GetLastError().empty());
    std::cout << "Expected error: " << client_->GetLastError() << std::endl;
}

// ==============================================================================
// Tile Fetch Tests
// ==============================================================================

TEST_F(WsiStreamerIntegrationTest, FetchTile_Level0_ReturnsValidJpeg) {
    // Arrange
    auto connectResult = client_->Connect(serverUrl_);
    ASSERT_TRUE(connectResult.success) << connectResult.errorMessage;

    // Act - Fetch tile at level 0, position (0, 0)
    auto result = client_->FetchTile(slideId_, 0, 0, 0);

    // Assert
    EXPECT_TRUE(result.success) << "Tile fetch failed: " << result.errorMessage;
    EXPECT_EQ(result.httpStatus, 200);
    EXPECT_FALSE(result.jpegData.empty()) << "JPEG data should not be empty";

    // Verify JPEG structure
    EXPECT_TRUE(IsValidJpeg(result.jpegData))
        << "Data does not start with JPEG magic bytes (FF D8 FF)";
    EXPECT_TRUE(HasJpegEndMarker(result.jpegData))
        << "Data does not end with JPEG end marker (FF D9)";

    // Reasonable size check (tiles are typically 10KB - 500KB)
    EXPECT_GT(result.jpegData.size(), 1000u) << "JPEG too small, likely invalid";
    EXPECT_LT(result.jpegData.size(), 5u * 1024 * 1024) << "JPEG too large (>5MB)";

    std::cout << "Tile (0,0) at level 0: " << result.jpegData.size() << " bytes" << std::endl;
}

TEST_F(WsiStreamerIntegrationTest, FetchTile_MultipleLevels_AllReturnValidJpeg) {
    // Arrange
    auto connectResult = client_->Connect(serverUrl_);
    ASSERT_TRUE(connectResult.success) << connectResult.errorMessage;

    auto info = client_->FetchSlideInfo(slideId_);
    ASSERT_TRUE(info.has_value()) << client_->GetLastError();

    std::cout << "Testing tiles across " << info->levelCount << " levels:" << std::endl;

    // Act & Assert - Test tile from each level
    for (int32_t level = 0; level < info->levelCount; ++level) {
        auto result = client_->FetchTile(slideId_, level, 0, 0);

        EXPECT_TRUE(result.success)
            << "Level " << level << " fetch failed: " << result.errorMessage;
        EXPECT_TRUE(IsValidJpeg(result.jpegData))
            << "Level " << level << " tile is not valid JPEG";

        std::cout << "  Level " << level << ": " << result.jpegData.size() << " bytes" << std::endl;
    }
}

TEST_F(WsiStreamerIntegrationTest, FetchTile_DifferentQualities_SizeVaries) {
    // Arrange
    auto connectResult = client_->Connect(serverUrl_);
    ASSERT_TRUE(connectResult.success) << connectResult.errorMessage;

    // Act - Fetch same tile at different quality levels
    auto lowQuality = client_->FetchTile(slideId_, 0, 0, 0, 20);
    auto highQuality = client_->FetchTile(slideId_, 0, 0, 0, 95);

    // Assert
    ASSERT_TRUE(lowQuality.success) << lowQuality.errorMessage;
    ASSERT_TRUE(highQuality.success) << highQuality.errorMessage;

    EXPECT_TRUE(IsValidJpeg(lowQuality.jpegData));
    EXPECT_TRUE(IsValidJpeg(highQuality.jpegData));

    // Higher quality should generally produce larger files
    EXPECT_GT(highQuality.jpegData.size(), lowQuality.jpegData.size())
        << "High quality JPEG should be larger than low quality";

    std::cout << "Quality comparison:" << std::endl;
    std::cout << "  Quality 20: " << lowQuality.jpegData.size() << " bytes" << std::endl;
    std::cout << "  Quality 95: " << highQuality.jpegData.size() << " bytes" << std::endl;
}

TEST_F(WsiStreamerIntegrationTest, FetchTile_MultiplePositions_AllReturnValidJpeg) {
    // Arrange
    auto connectResult = client_->Connect(serverUrl_);
    ASSERT_TRUE(connectResult.success) << connectResult.errorMessage;

    // Test a few different tile positions at level 0
    const std::vector<std::pair<int, int>> positions = {
        {0, 0}, {1, 0}, {0, 1}, {1, 1}
    };

    std::cout << "Testing multiple tile positions:" << std::endl;

    // Act & Assert
    for (const auto& [x, y] : positions) {
        auto result = client_->FetchTile(slideId_, 0, x, y);

        EXPECT_TRUE(result.success)
            << "Tile (" << x << "," << y << ") fetch failed: " << result.errorMessage;

        if (result.success) {
            EXPECT_TRUE(IsValidJpeg(result.jpegData))
                << "Tile (" << x << "," << y << ") is not valid JPEG";
            std::cout << "  Tile (" << x << "," << y << "): "
                      << result.jpegData.size() << " bytes" << std::endl;
        }
    }
}

// ==============================================================================
// Error Handling Tests
// ==============================================================================

TEST_F(WsiStreamerIntegrationTest, FetchTile_OutOfBounds_ReturnsError) {
    // Arrange
    auto connectResult = client_->Connect(serverUrl_);
    ASSERT_TRUE(connectResult.success) << connectResult.errorMessage;

    // Act - Request tile with very large coordinates
    auto result = client_->FetchTile(slideId_, 0, 999999, 999999);

    // Assert
    EXPECT_FALSE(result.success);
    EXPECT_NE(result.httpStatus, 200);
    // Server returns 400 for out-of-bounds tiles
    EXPECT_TRUE(result.httpStatus == 400 || result.httpStatus == 404)
        << "Expected 400 or 404, got " << result.httpStatus;

    std::cout << "Out-of-bounds response: HTTP " << result.httpStatus << std::endl;
}

TEST_F(WsiStreamerIntegrationTest, FetchTile_InvalidLevel_ReturnsError) {
    // Arrange
    auto connectResult = client_->Connect(serverUrl_);
    ASSERT_TRUE(connectResult.success) << connectResult.errorMessage;

    auto info = client_->FetchSlideInfo(slideId_);
    ASSERT_TRUE(info.has_value());

    // Act - Request tile at invalid level (beyond available levels)
    auto result = client_->FetchTile(slideId_, info->levelCount + 10, 0, 0);

    // Assert
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.httpStatus, 400) << "Expected 400 for invalid level";

    std::cout << "Invalid level response: HTTP " << result.httpStatus << std::endl;
}

TEST_F(WsiStreamerIntegrationTest, FetchTile_NonexistentSlide_ReturnsError) {
    // Arrange
    auto connectResult = client_->Connect(serverUrl_);
    ASSERT_TRUE(connectResult.success) << connectResult.errorMessage;

    // Act
    auto result = client_->FetchTile("nonexistent_slide_xyz.svs", 0, 0, 0);

    // Assert
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.httpStatus == 404 || result.httpStatus == 400)
        << "Expected 404 or 400, got " << result.httpStatus;

    std::cout << "Non-existent slide response: HTTP " << result.httpStatus << std::endl;
}

}  // namespace test
}  // namespace remote
}  // namespace pathview
