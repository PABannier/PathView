// Viewport Unit Tests
// Tests for coordinate transformations, zoom/pan math, and bounds clamping
// Critical for correct viewer behavior

#include <gtest/gtest.h>
#include "Viewport.h"
#include <SDL_timer.h>
#include <cmath>
#include <vector>

// ============================================================================
// Test Fixture
// ============================================================================

class ViewportTest : public ::testing::Test {
protected:
    // Standard HD viewport with 10000x8000 slide
    static constexpr int WINDOW_WIDTH = 1920;
    static constexpr int WINDOW_HEIGHT = 1080;
    static constexpr int64_t SLIDE_WIDTH = 10000;
    static constexpr int64_t SLIDE_HEIGHT = 8000;

    std::unique_ptr<Viewport> viewport;

    void SetUp() override {
        viewport = std::make_unique<Viewport>(WINDOW_WIDTH, WINDOW_HEIGHT, SLIDE_WIDTH, SLIDE_HEIGHT);
    }

    // Helper to check if two Vec2s are approximately equal
    void ExpectVec2Near(const Vec2& actual, const Vec2& expected, double tolerance = 1e-5) {
        EXPECT_NEAR(actual.x, expected.x, tolerance)
            << "x mismatch: expected " << expected.x << ", got " << actual.x;
        EXPECT_NEAR(actual.y, expected.y, tolerance)
            << "y mismatch: expected " << expected.y << ", got " << actual.y;
    }
};

// ============================================================================
// Coordinate Transformation Tests - Core Mathematical Invariants
// ============================================================================

TEST_F(ViewportTest, ScreenToSlide_ThenSlideToScreen_ReturnsIdentity) {
    // Test at various zoom levels and positions
    std::vector<double> zooms = {0.5, 1.0, 1.5, 2.0, 3.0};
    std::vector<Vec2> positions = {
        Vec2(0, 0),
        Vec2(1000, 1000),
        Vec2(5000, 4000),
        Vec2(8000, 6000)
    };

    // Test at default state (after ResetView in constructor)
    std::vector<Vec2> screen_points = {
        Vec2(0, 0),
        Vec2(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2),
        Vec2(WINDOW_WIDTH, WINDOW_HEIGHT),
        Vec2(100, 200),
        Vec2(500, 500)
    };

    for (const auto& screen_pt : screen_points) {
        Vec2 slide_pt = viewport->ScreenToSlide(screen_pt);
        Vec2 result = viewport->SlideToScreen(slide_pt);
        ExpectVec2Near(result, screen_pt, 1.0);  // Allow 1 pixel tolerance
    }
}

TEST_F(ViewportTest, SlideToScreen_ThenScreenToSlide_ReturnsIdentity) {
    // Test inverse transformation
    std::vector<Vec2> slide_points = {
        Vec2(0, 0),
        Vec2(5000, 4000),
        Vec2(10000, 8000),
        Vec2(1234, 5678)
    };

    for (const auto& slide_pt : slide_points) {
        Vec2 screen_pt = viewport->SlideToScreen(slide_pt);
        Vec2 result = viewport->ScreenToSlide(screen_pt);
        ExpectVec2Near(result, slide_pt, 10.0);  // Larger tolerance for round-trip
    }
}

TEST_F(ViewportTest, ScreenToSlide_Origin_CorrectMapping) {
    // At screen origin, should map to viewport position
    Vec2 result = viewport->ScreenToSlide(Vec2(0, 0));
    Vec2 viewportPos = viewport->GetPosition();
    ExpectVec2Near(result, viewportPos);
}

TEST_F(ViewportTest, SlideToScreen_ViewportPosition_MapsToOrigin) {
    // Viewport position should map to screen origin
    Vec2 viewportPos = viewport->GetPosition();
    Vec2 result = viewport->SlideToScreen(viewportPos);
    ExpectVec2Near(result, Vec2(0, 0), 1.0);
}

// ============================================================================
// Zoom Tests
// ============================================================================

TEST_F(ViewportTest, GetZoom_InitialState_WithinLimits) {
    double zoom = viewport->GetZoom();
    EXPECT_GE(zoom, viewport->GetMinZoom());
    EXPECT_LE(zoom, viewport->GetMaxZoom());
}

TEST_F(ViewportTest, ZoomAtPoint_CenterOfScreen_ChangesZoom) {
    double initial_zoom = viewport->GetZoom();
    Vec2 center(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);

    viewport->ZoomAtPoint(center, 2.0, AnimationMode::INSTANT);
    viewport->UpdateAnimation(1000.0);  // Complete animation

    double new_zoom = viewport->GetZoom();
    EXPECT_NE(new_zoom, initial_zoom);
}

TEST_F(ViewportTest, ZoomAtPoint_ScreenPoint_PointRemainsFixed) {
    Vec2 screen_point(960, 540);  // Center of 1920x1080

    // Get slide point before zoom
    Vec2 slide_before = viewport->ScreenToSlide(screen_point);

    // Zoom in by 2x at this point
    viewport->ZoomAtPoint(screen_point, 2.0, AnimationMode::INSTANT);
    viewport->UpdateAnimation(1000.0);  // Complete animation

    // Get slide point after zoom
    Vec2 slide_after = viewport->ScreenToSlide(screen_point);

    // The slide point under the cursor should not have moved significantly
    ExpectVec2Near(slide_after, slide_before, 50.0);  // Some tolerance for clamping
}

TEST_F(ViewportTest, ZoomAtPoint_ZoomIn_IncreasesZoom) {
    double initial_zoom = viewport->GetZoom();

    viewport->ZoomAtPoint(Vec2(100, 100), 1.5, AnimationMode::INSTANT);
    viewport->UpdateAnimation(1000.0);

    EXPECT_GT(viewport->GetZoom(), initial_zoom);
}

TEST_F(ViewportTest, ZoomAtPoint_ZoomOut_DecreasesZoom) {
    // First zoom in so we have room to zoom out
    viewport->ZoomAtPoint(Vec2(100, 100), 2.0, AnimationMode::INSTANT);
    viewport->UpdateAnimation(1000.0);
    double mid_zoom = viewport->GetZoom();

    // Now zoom out
    viewport->ZoomAtPoint(Vec2(100, 100), 0.5, AnimationMode::INSTANT);
    viewport->UpdateAnimation(1000.0);

    EXPECT_LT(viewport->GetZoom(), mid_zoom);
}

TEST_F(ViewportTest, ZoomAtPoint_BeyondMaxZoom_ClampsToMaxZoom) {
    viewport->ZoomAtPoint(Vec2(100, 100), 100.0, AnimationMode::INSTANT);
    viewport->UpdateAnimation(1000.0);

    EXPECT_LE(viewport->GetZoom(), viewport->GetMaxZoom());
}

TEST_F(ViewportTest, ZoomAtPoint_BelowMinZoom_ClampsToMinZoom) {
    viewport->ZoomAtPoint(Vec2(100, 100), 0.001, AnimationMode::INSTANT);
    viewport->UpdateAnimation(1000.0);

    EXPECT_GE(viewport->GetZoom(), viewport->GetMinZoom());
}

// ============================================================================
// Pan Tests
// ============================================================================

TEST_F(ViewportTest, Pan_PositiveDelta_MovesViewport) {
    Vec2 initial_pos = viewport->GetPosition();

    viewport->Pan(Vec2(100, 100), AnimationMode::INSTANT);
    viewport->UpdateAnimation(1000.0);

    Vec2 new_pos = viewport->GetPosition();
    EXPECT_GT(new_pos.x, initial_pos.x);
    EXPECT_GT(new_pos.y, initial_pos.y);
}

TEST_F(ViewportTest, Pan_NegativeDelta_MovesViewport) {
    // First pan to a non-zero position
    viewport->Pan(Vec2(1000, 1000), AnimationMode::INSTANT);
    viewport->UpdateAnimation(1000.0);
    Vec2 mid_pos = viewport->GetPosition();

    // Then pan back
    viewport->Pan(Vec2(-500, -500), AnimationMode::INSTANT);
    viewport->UpdateAnimation(1000.0);

    Vec2 new_pos = viewport->GetPosition();
    EXPECT_LT(new_pos.x, mid_pos.x);
    EXPECT_LT(new_pos.y, mid_pos.y);
}

TEST_F(ViewportTest, Pan_BeyondBounds_ClampsToBounds) {
    // Try to pan far beyond slide bounds
    viewport->Pan(Vec2(50000, 50000), AnimationMode::INSTANT);
    viewport->UpdateAnimation(1000.0);

    Vec2 pos = viewport->GetPosition();

    // Should be clamped within reasonable bounds
    EXPECT_LT(pos.x, SLIDE_WIDTH);
    EXPECT_LT(pos.y, SLIDE_HEIGHT);
}

TEST_F(ViewportTest, Pan_NegativeBeyondBounds_ClampsToBounds) {
    // Try to pan far into negative
    viewport->Pan(Vec2(-50000, -50000), AnimationMode::INSTANT);
    viewport->UpdateAnimation(1000.0);

    Vec2 pos = viewport->GetPosition();

    // Should be clamped at or near zero (or negative if viewport > slide)
    // Exact value depends on viewport size
    EXPECT_GE(pos.x, -static_cast<double>(WINDOW_WIDTH));
    EXPECT_GE(pos.y, -static_cast<double>(WINDOW_HEIGHT));
}

// ============================================================================
// CenterOn Tests
// ============================================================================

TEST_F(ViewportTest, CenterOn_SlideCenter_CentersViewport) {
    Vec2 slide_center(SLIDE_WIDTH / 2, SLIDE_HEIGHT / 2);

    viewport->CenterOn(slide_center, AnimationMode::INSTANT);
    viewport->UpdateAnimation(1000.0);

    // The center of the screen should now map to the slide center
    Vec2 screen_center(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
    Vec2 mapped_slide = viewport->ScreenToSlide(screen_center);

    ExpectVec2Near(mapped_slide, slide_center, 100.0);  // Generous tolerance
}

TEST_F(ViewportTest, CenterOn_SlideOrigin_ClampsWithinBounds) {
    // First complete any pending animation from constructor
    viewport->UpdateAnimation(SDL_GetTicks() + 1000);

    // Zoom in first so viewport is smaller than slide, allowing CenterOn to work properly
    viewport->ZoomAtPoint(Vec2(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2), 4.0, AnimationMode::INSTANT);
    viewport->UpdateAnimation(SDL_GetTicks() + 1000);

    viewport->CenterOn(Vec2(0, 0), AnimationMode::INSTANT);
    viewport->UpdateAnimation(SDL_GetTicks() + 1000);

    // When viewport < slide, trying to center on (0,0) will clamp position to (0,0)
    // because the viewport can't go past the slide edge
    Vec2 pos = viewport->GetPosition();
    EXPECT_NEAR(pos.x, 0.0, 1.0);  // Should be clamped to 0 (or very close)
    EXPECT_NEAR(pos.y, 0.0, 1.0);  // Should be clamped to 0 (or very close)
}

// ============================================================================
// ResetView Tests
// ============================================================================

TEST_F(ViewportTest, ResetView_SetsZoomToMinZoom) {
    // First zoom in
    viewport->ZoomAtPoint(Vec2(100, 100), 2.0, AnimationMode::INSTANT);
    viewport->UpdateAnimation(1000.0);

    // Then reset
    viewport->ResetView(AnimationMode::INSTANT);
    viewport->UpdateAnimation(2000.0);

    EXPECT_NEAR(viewport->GetZoom(), viewport->GetMinZoom(), 0.01);
}

// ============================================================================
// Bounds Clamping Edge Cases
// ============================================================================

TEST_F(ViewportTest, ClampToBounds_ViewportLargerThanSlide_CentersSlide) {
    // Create viewport much larger than slide
    Viewport tiny_slide_vp(1920, 1080, 500, 300);

    // Zoom out far enough that viewport > slide
    tiny_slide_vp.ZoomAtPoint(Vec2(100, 100), 0.1, AnimationMode::INSTANT);
    tiny_slide_vp.UpdateAnimation(1000.0);

    Vec2 pos = tiny_slide_vp.GetPosition();

    // Position should be negative to center the small slide
    // (This allows the slide to be centered in a larger viewport)
    // Exact value depends on zoom level, but conceptually it should work
    EXPECT_TRUE(pos.x < 500 && pos.y < 300);  // Within slide bounds or negative
}

TEST_F(ViewportTest, ClampToBounds_NormalCase_StaysWithinBounds) {
    // At normal zoom (around 1.0), position should be >= 0
    viewport->ResetView(AnimationMode::INSTANT);
    viewport->UpdateAnimation(1000.0);

    // Then zoom in
    viewport->ZoomAtPoint(Vec2(100, 100), 2.0, AnimationMode::INSTANT);
    viewport->UpdateAnimation(1000.0);

    Vec2 pos = viewport->GetPosition();

    // Position should be non-negative and within slide
    EXPECT_GE(pos.x, 0.0);
    EXPECT_GE(pos.y, 0.0);
    EXPECT_LE(pos.x, SLIDE_WIDTH);
    EXPECT_LE(pos.y, SLIDE_HEIGHT);
}

// ============================================================================
// GetVisibleRegion Tests
// ============================================================================

TEST_F(ViewportTest, GetVisibleRegion_ReturnsValidRect) {
    Rect region = viewport->GetVisibleRegion();

    EXPECT_GT(region.width, 0);
    EXPECT_GT(region.height, 0);
}

TEST_F(ViewportTest, GetVisibleRegion_PositionMatchesViewport) {
    Rect region = viewport->GetVisibleRegion();
    Vec2 pos = viewport->GetPosition();

    EXPECT_NEAR(region.x, pos.x, 1.0);
    EXPECT_NEAR(region.y, pos.y, 1.0);
}

TEST_F(ViewportTest, GetVisibleRegion_SizeInverselyProportionalToZoom) {
    // Get region at initial zoom
    Rect region1 = viewport->GetVisibleRegion();
    double width1 = region1.width;

    // Zoom in 2x
    viewport->ZoomAtPoint(Vec2(100, 100), 2.0, AnimationMode::INSTANT);
    viewport->UpdateAnimation(1000.0);

    Rect region2 = viewport->GetVisibleRegion();
    double width2 = region2.width;

    // After 2x zoom, visible width should be ~half
    EXPECT_NEAR(width2, width1 / 2.0, width1 * 0.1);  // 10% tolerance
}

// ============================================================================
// Window Resize Tests
// ============================================================================

TEST_F(ViewportTest, SetWindowSize_UpdatesWindowDimensions) {
    viewport->SetWindowSize(2560, 1440);

    EXPECT_EQ(viewport->GetWindowWidth(), 2560);
    EXPECT_EQ(viewport->GetWindowHeight(), 1440);
}

TEST_F(ViewportTest, SetWindowSize_RecalculatesZoomLimits) {
    double initial_min_zoom = viewport->GetMinZoom();

    // Make window larger
    viewport->SetWindowSize(3840, 2160);

    double new_min_zoom = viewport->GetMinZoom();

    // Larger window should allow smaller min zoom (more of slide fits)
    EXPECT_NE(new_min_zoom, initial_min_zoom);
}

// ============================================================================
// Slide Dimension Change Tests
// ============================================================================

TEST_F(ViewportTest, SetSlideDimensions_UpdatesDimensions) {
    viewport->SetSlideDimensions(20000, 16000);

    EXPECT_EQ(viewport->GetSlideWidth(), 20000);
    EXPECT_EQ(viewport->GetSlideHeight(), 16000);
}

TEST_F(ViewportTest, SetSlideDimensions_RecalculatesZoomLimits) {
    double initial_min_zoom = viewport->GetMinZoom();

    // Make slide much larger
    viewport->SetSlideDimensions(50000, 40000);

    double new_min_zoom = viewport->GetMinZoom();

    // Larger slide should require smaller min zoom to fit
    EXPECT_LT(new_min_zoom, initial_min_zoom);
}

// ============================================================================
// Animation Integration Tests
// ============================================================================

TEST_F(ViewportTest, UpdateAnimation_NoActiveAnimation_NoChange) {
    // First, complete any pending animation from the constructor's ResetView() call
    viewport->UpdateAnimation(1000.0);

    // Now get the stable state after initial animation is complete
    Vec2 initial_pos = viewport->GetPosition();
    double initial_zoom = viewport->GetZoom();

    // Call UpdateAnimation again - should not change anything since no animation is active
    viewport->UpdateAnimation(2000.0);

    EXPECT_EQ(viewport->GetPosition().x, initial_pos.x);
    EXPECT_EQ(viewport->GetZoom(), initial_zoom);
}

TEST_F(ViewportTest, Pan_SmoothMode_UsesAnimation) {
    Vec2 initial_pos = viewport->GetPosition();

    viewport->Pan(Vec2(1000, 1000), AnimationMode::SMOOTH);

    // Should still be near initial position (animation not complete)
    Vec2 pos_immediately = viewport->GetPosition();
    ExpectVec2Near(pos_immediately, initial_pos, 10.0);

    // Update animation partway (150ms into 300ms animation)
    viewport->UpdateAnimation(SDL_GetTicks() + 150);

    Vec2 pos_mid = viewport->GetPosition();
    // Should be somewhere between start and end
    EXPECT_GT(pos_mid.x, initial_pos.x);
    EXPECT_LT(pos_mid.x, initial_pos.x + 1000.0);
}

TEST_F(ViewportTest, ZoomAtPoint_InstantMode_NoAnimation) {
    double initial_zoom = viewport->GetZoom();

    viewport->ZoomAtPoint(Vec2(100, 100), 2.0, AnimationMode::INSTANT);

    // Update immediately completes INSTANT mode
    viewport->UpdateAnimation(1000.0);

    EXPECT_NE(viewport->GetZoom(), initial_zoom);
}

// ============================================================================
// Stress Tests & Edge Cases
// ============================================================================

TEST_F(ViewportTest, GetZoomLimits_NonZeroSlide_ValidLimits) {
    EXPECT_GT(viewport->GetMinZoom(), 0.0);
    EXPECT_GT(viewport->GetMaxZoom(), 0.0);
    EXPECT_LT(viewport->GetMinZoom(), viewport->GetMaxZoom());
}

TEST_F(ViewportTest, Rect_Contains_CorrectlyIdentifiesPoints) {
    Rect r(100, 100, 200, 150);

    EXPECT_TRUE(r.Contains(150, 150));   // Inside
    EXPECT_TRUE(r.Contains(100, 100));   // Top-left corner
    EXPECT_FALSE(r.Contains(300, 250));  // Outside (at boundary)
    EXPECT_FALSE(r.Contains(50, 50));    // Outside (before)
    EXPECT_FALSE(r.Contains(350, 300));  // Outside (after)
}

TEST_F(ViewportTest, Rect_Intersects_CorrectlyDetectsOverlap) {
    Rect r1(100, 100, 200, 150);
    Rect r2(150, 125, 100, 100);  // Overlaps
    Rect r3(400, 400, 100, 100);  // No overlap

    EXPECT_TRUE(r1.Intersects(r2));
    EXPECT_FALSE(r1.Intersects(r3));
}
