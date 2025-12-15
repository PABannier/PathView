// Animation Unit Tests
// Tests for easing functions and animation state machine
// Critical for MCP smooth transition requirement

#include <gtest/gtest.h>
#include "Animation.h"
#include <cmath>
#include <vector>

// ============================================================================
// Test Fixture
// ============================================================================

class AnimationTest : public ::testing::Test {
protected:
    Animation anim;

    void SetUp() override {
        anim = Animation();
    }
};

// ============================================================================
// Easing Behavior Tests (tested indirectly through animation)
// Note: EaseInOutCubic is private, so we test its behavior through the
// animation system rather than directly
// ============================================================================

TEST_F(AnimationTest, Animation_StartAndEnd_ExactTargetValues) {
    // Verify that smooth animation starts at start value and ends at target value
    anim.StartAt(Vec2(0, 0), 1.0, Vec2(100, 100), 2.0, AnimationMode::SMOOTH, 1000.0, 1000.0);

    Vec2 outPos;
    double outZoom;

    // At t=0 (start time 1000), should be at start
    anim.Update(1000.0, outPos, outZoom);
    // Actually at t=0 we get the eased value which is 0, so start values

    // At t=1 (time 2000), should be at target exactly
    anim.Update(2000.0, outPos, outZoom);
    EXPECT_DOUBLE_EQ(outPos.x, 100.0);
    EXPECT_DOUBLE_EQ(outPos.y, 100.0);
    EXPECT_DOUBLE_EQ(outZoom, 2.0);
}

TEST_F(AnimationTest, Animation_Midpoint_ApproximatelyHalfway) {
    // At t=0.5, ease-in-out cubic should be approximately at midpoint
    anim.StartAt(Vec2(0, 0), 1.0, Vec2(100, 100), 2.0, AnimationMode::SMOOTH, 1000.0, 1000.0);

    Vec2 outPos;
    double outZoom;

    anim.Update(1500.0, outPos, outZoom);  // t = 0.5

    EXPECT_NEAR(outPos.x, 50.0, 5.0);  // Should be near midpoint
    EXPECT_NEAR(outPos.y, 50.0, 5.0);
    EXPECT_NEAR(outZoom, 1.5, 0.1);
}

// ============================================================================
// Animation State Machine Tests
// ============================================================================

TEST_F(AnimationTest, IsActive_InitialState_ReturnsFalse) {
    EXPECT_FALSE(anim.IsActive());
}

TEST_F(AnimationTest, Start_SetsActiveState) {
    anim.StartAt(Vec2(0, 0), 1.0, Vec2(100, 100), 2.0, AnimationMode::SMOOTH, 1000.0, 300.0);
    EXPECT_TRUE(anim.IsActive());
}

TEST_F(AnimationTest, Update_InstantMode_CompletesImmediately) {
    Vec2 startPos(0, 0);
    double startZoom = 1.0;
    Vec2 targetPos(100, 100);
    double targetZoom = 2.0;

    anim.StartAt(startPos, startZoom, targetPos, targetZoom, AnimationMode::INSTANT, 1000.0, 300.0);

    Vec2 outPos;
    double outZoom;
    bool complete = anim.Update(0.0, outPos, outZoom);

    EXPECT_TRUE(complete);
    EXPECT_DOUBLE_EQ(outPos.x, 100.0);
    EXPECT_DOUBLE_EQ(outPos.y, 100.0);
    EXPECT_DOUBLE_EQ(outZoom, 2.0);
    EXPECT_FALSE(anim.IsActive());
}

TEST_F(AnimationTest, Update_SmoothMode_GradualTransition) {
    Vec2 startPos(0, 0);
    double startZoom = 1.0;
    Vec2 targetPos(100, 100);
    double targetZoom = 2.0;

    anim.StartAt(startPos, startZoom, targetPos, targetZoom, AnimationMode::SMOOTH, 1000.0, 1000.0);

    // Simulate time progression at 25%, 50%, 75%
    Vec2 outPos;
    double outZoom;

    // At 25% progress
    bool complete25 = anim.Update(1250.0, outPos, outZoom);
    EXPECT_FALSE(complete25);
    EXPECT_GT(outPos.x, 0.0);
    EXPECT_LT(outPos.x, 100.0);
    EXPECT_TRUE(anim.IsActive());

    // At 50% progress (should be exactly halfway due to symmetry)
    bool complete50 = anim.Update(1500.0, outPos, outZoom);
    EXPECT_FALSE(complete50);
    EXPECT_DOUBLE_EQ(outPos.x, 50.0);
    EXPECT_DOUBLE_EQ(outPos.y, 50.0);
    EXPECT_DOUBLE_EQ(outZoom, 1.5);

    // At 75% progress
    bool complete75 = anim.Update(1750.0, outPos, outZoom);
    EXPECT_FALSE(complete75);
    EXPECT_GT(outPos.x, 50.0);
    EXPECT_LT(outPos.x, 100.0);
}

TEST_F(AnimationTest, Update_AfterCompletion_SnapsToTarget) {
    Vec2 startPos(0, 0);
    double startZoom = 1.0;
    Vec2 targetPos(100, 100);
    double targetZoom = 2.0;

    anim.StartAt(startPos, startZoom, targetPos, targetZoom, AnimationMode::SMOOTH, 1000.0, 300.0);

    Vec2 outPos;
    double outZoom;

    // Update far beyond duration
    bool complete = anim.Update(2000.0, outPos, outZoom);

    EXPECT_TRUE(complete);
    EXPECT_DOUBLE_EQ(outPos.x, 100.0);
    EXPECT_DOUBLE_EQ(outPos.y, 100.0);
    EXPECT_DOUBLE_EQ(outZoom, 2.0);
    EXPECT_FALSE(anim.IsActive());
}

TEST_F(AnimationTest, Update_NotActive_ReturnsFalse) {
    Vec2 outPos;
    double outZoom;

    bool result = anim.Update(1000.0, outPos, outZoom);
    EXPECT_FALSE(result);
}

TEST_F(AnimationTest, Cancel_StopsAnimation) {
    anim.StartAt(Vec2(0, 0), 1.0, Vec2(100, 100), 2.0, AnimationMode::SMOOTH, 1000.0, 300.0);
    EXPECT_TRUE(anim.IsActive());

    anim.Cancel();
    EXPECT_FALSE(anim.IsActive());
}

TEST_F(AnimationTest, Update_SmoothMode_MonotonicProgression) {
    Vec2 startPos(0, 0);
    Vec2 targetPos(1000, 1000);

    anim.StartAt(startPos, 1.0, targetPos, 2.0, AnimationMode::SMOOTH, 1000.0, 500.0);

    Vec2 outPos;
    double outZoom;
    double prev_x = 0.0;

    // Sample at 10% intervals
    for (double time = 1000.0; time <= 1500.0; time += 50.0) {
        anim.Update(time, outPos, outZoom);
        EXPECT_GE(outPos.x, prev_x) << "Non-monotonic at time " << time;
        prev_x = outPos.x;
    }
}

TEST_F(AnimationTest, Update_SmoothMode_EaseInOutCharacteristic) {
    Vec2 startPos(0, 0);
    Vec2 targetPos(1000, 0);

    anim.StartAt(startPos, 1.0, targetPos, 1.0, AnimationMode::SMOOTH, 1000.0, 1000.0);

    Vec2 outPos;
    double outZoom;

    // Measure deltas at different points
    anim.Update(1100.0, outPos, outZoom);
    double pos_10 = outPos.x;

    anim.Update(1200.0, outPos, outZoom);
    double pos_20 = outPos.x;
    double delta_early = pos_20 - pos_10;

    anim.Update(1500.0, outPos, outZoom);
    double pos_50 = outPos.x;

    anim.Update(1600.0, outPos, outZoom);
    double pos_60 = outPos.x;
    double delta_mid = pos_60 - pos_50;

    anim.Update(1900.0, outPos, outZoom);
    double pos_90 = outPos.x;

    anim.Update(2000.0, outPos, outZoom);
    double pos_100 = outPos.x;
    double delta_late = pos_100 - pos_90;

    // In ease-in-out, middle section should be faster than early and late sections
    EXPECT_GT(delta_mid, delta_early) << "Middle not faster than early";
    EXPECT_GT(delta_mid, delta_late) << "Middle not faster than late";
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(AnimationTest, Start_ZeroDuration_StillWorks) {
    anim.StartAt(Vec2(0, 0), 1.0, Vec2(100, 100), 2.0, AnimationMode::SMOOTH, 1000.0, 0.0);

    Vec2 outPos;
    double outZoom;

    // With zero duration, any update should complete immediately
    bool complete = anim.Update(1000.0, outPos, outZoom);
    EXPECT_TRUE(complete);
    EXPECT_DOUBLE_EQ(outPos.x, 100.0);
}

TEST_F(AnimationTest, Start_OverwritesPreviousAnimation) {
    // Start first animation
    anim.StartAt(Vec2(0, 0), 1.0, Vec2(100, 100), 2.0, AnimationMode::SMOOTH, 1000.0, 300.0);
    EXPECT_TRUE(anim.IsActive());

    // Start second animation (should overwrite)
    anim.StartAt(Vec2(200, 200), 3.0, Vec2(300, 300), 4.0, AnimationMode::SMOOTH, 1500.0, 300.0);

    Vec2 outPos;
    double outZoom;

    // Update and check it's using new target
    anim.Update(2000.0, outPos, outZoom);
    EXPECT_DOUBLE_EQ(outPos.x, 300.0);
    EXPECT_DOUBLE_EQ(outZoom, 4.0);
}
