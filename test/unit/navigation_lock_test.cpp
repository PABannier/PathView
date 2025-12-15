#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include "../../src/core/Application.h"

// Test NavigationLock struct functionality
class NavigationLockTest : public ::testing::Test {
protected:
    NavigationLock lock;
};

TEST_F(NavigationLockTest, InitiallyUnlocked) {
    EXPECT_FALSE(lock.isLocked);
    EXPECT_EQ(lock.ownerUUID, "");
    EXPECT_EQ(lock.clientFd, -1);
    EXPECT_FALSE(lock.IsExpired());
}

TEST_F(NavigationLockTest, LockGrant) {
    // Simulate lock grant
    lock.isLocked = true;
    lock.ownerUUID = "test-uuid-123";
    lock.grantedTime = std::chrono::steady_clock::now();
    lock.ttlMs = std::chrono::milliseconds(5000);
    lock.clientFd = 42;

    EXPECT_TRUE(lock.isLocked);
    EXPECT_EQ(lock.ownerUUID, "test-uuid-123");
    EXPECT_EQ(lock.clientFd, 42);
    EXPECT_FALSE(lock.IsExpired());
}

TEST_F(NavigationLockTest, LockExpiry) {
    // Grant lock with short TTL
    lock.isLocked = true;
    lock.ownerUUID = "test-uuid-123";
    lock.grantedTime = std::chrono::steady_clock::now();
    lock.ttlMs = std::chrono::milliseconds(100);

    EXPECT_FALSE(lock.IsExpired());

    // Wait for expiry
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    EXPECT_TRUE(lock.IsExpired());
}

TEST_F(NavigationLockTest, OwnershipCheck) {
    lock.isLocked = true;
    lock.ownerUUID = "agent-abc-123";
    lock.grantedTime = std::chrono::steady_clock::now();
    lock.ttlMs = std::chrono::milliseconds(5000);

    EXPECT_TRUE(lock.IsOwnedBy("agent-abc-123"));
    EXPECT_FALSE(lock.IsOwnedBy("agent-xyz-456"));
    EXPECT_FALSE(lock.IsOwnedBy(""));
}

TEST_F(NavigationLockTest, NotExpiredWhenUnlocked) {
    // Even with expired TTL, unlocked state should not be "expired"
    lock.isLocked = false;
    lock.grantedTime = std::chrono::steady_clock::now() - std::chrono::seconds(10);
    lock.ttlMs = std::chrono::milliseconds(1000);

    EXPECT_FALSE(lock.IsExpired());
}

TEST_F(NavigationLockTest, OwnershipRequiresLocked) {
    // Ownership check should fail when not locked
    lock.isLocked = false;
    lock.ownerUUID = "test-uuid";

    EXPECT_FALSE(lock.IsOwnedBy("test-uuid"));
}

TEST_F(NavigationLockTest, LockRenewal) {
    // Initial lock
    lock.isLocked = true;
    lock.ownerUUID = "agent-123";
    auto firstGrant = std::chrono::steady_clock::now();
    lock.grantedTime = firstGrant;
    lock.ttlMs = std::chrono::milliseconds(1000);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Renew lock (same owner)
    auto secondGrant = std::chrono::steady_clock::now();
    lock.grantedTime = secondGrant;
    lock.ttlMs = std::chrono::milliseconds(2000);

    EXPECT_GT(lock.grantedTime, firstGrant);
    EXPECT_FALSE(lock.IsExpired());
}

TEST_F(NavigationLockTest, UnlockReset) {
    // Lock
    lock.isLocked = true;
    lock.ownerUUID = "agent-123";
    lock.grantedTime = std::chrono::steady_clock::now();
    lock.ttlMs = std::chrono::milliseconds(5000);
    lock.clientFd = 42;

    EXPECT_TRUE(lock.isLocked);

    // Unlock by resetting to default
    lock = NavigationLock();

    EXPECT_FALSE(lock.isLocked);
    EXPECT_EQ(lock.ownerUUID, "");
    EXPECT_EQ(lock.clientFd, -1);
    EXPECT_FALSE(lock.IsExpired());
}
