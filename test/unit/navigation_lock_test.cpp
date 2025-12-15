#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include "../../src/core/NavigationLock.h"

// Test NavigationLock class functionality
class NavigationLockTest : public ::testing::Test {
protected:
    NavigationLock lock;
};

TEST_F(NavigationLockTest, InitiallyUnlocked) {
    EXPECT_FALSE(lock.IsLocked());
    EXPECT_EQ(lock.GetOwnerUUID(), "");
    EXPECT_EQ(lock.GetClientFd(), -1);
    EXPECT_FALSE(lock.IsExpired());
}

TEST_F(NavigationLockTest, LockGrant) {
    // Simulate lock grant
    lock.SetLocked(true);
    lock.SetOwnerUUID("test-uuid-123");
    lock.SetGrantedTime(std::chrono::steady_clock::now());
    lock.SetTTL(std::chrono::milliseconds(5000));
    lock.SetClientFd(42);

    EXPECT_TRUE(lock.IsLocked());
    EXPECT_EQ(lock.GetOwnerUUID(), "test-uuid-123");
    EXPECT_EQ(lock.GetClientFd(), 42);
    EXPECT_FALSE(lock.IsExpired());
}

TEST_F(NavigationLockTest, LockExpiry) {
    // Grant lock with short TTL
    lock.SetLocked(true);
    lock.SetOwnerUUID("test-uuid-123");
    lock.SetGrantedTime(std::chrono::steady_clock::now());
    lock.SetTTL(std::chrono::milliseconds(100));

    EXPECT_FALSE(lock.IsExpired());

    // Wait for expiry
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    EXPECT_TRUE(lock.IsExpired());
}

TEST_F(NavigationLockTest, OwnershipCheck) {
    lock.SetLocked(true);
    lock.SetOwnerUUID("agent-abc-123");
    lock.SetGrantedTime(std::chrono::steady_clock::now());
    lock.SetTTL(std::chrono::milliseconds(5000));

    EXPECT_TRUE(lock.IsOwnedBy("agent-abc-123"));
    EXPECT_FALSE(lock.IsOwnedBy("agent-xyz-456"));
    EXPECT_FALSE(lock.IsOwnedBy(""));
}

TEST_F(NavigationLockTest, NotExpiredWhenUnlocked) {
    // Even with expired TTL, unlocked state should not be "expired"
    lock.SetLocked(false);
    lock.SetGrantedTime(std::chrono::steady_clock::now() - std::chrono::seconds(10));
    lock.SetTTL(std::chrono::milliseconds(1000));

    EXPECT_FALSE(lock.IsExpired());
}

TEST_F(NavigationLockTest, OwnershipRequiresLocked) {
    // Ownership check should fail when not locked
    lock.SetLocked(false);
    lock.SetOwnerUUID("test-uuid");

    EXPECT_FALSE(lock.IsOwnedBy("test-uuid"));
}

TEST_F(NavigationLockTest, LockRenewal) {
    // Initial lock
    lock.SetLocked(true);
    lock.SetOwnerUUID("agent-123");
    auto firstGrant = std::chrono::steady_clock::now();
    lock.SetGrantedTime(firstGrant);
    lock.SetTTL(std::chrono::milliseconds(1000));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Renew lock (same owner)
    auto secondGrant = std::chrono::steady_clock::now();
    lock.SetGrantedTime(secondGrant);
    lock.SetTTL(std::chrono::milliseconds(2000));

    EXPECT_GT(lock.GetGrantedTime(), firstGrant);
    EXPECT_FALSE(lock.IsExpired());
}

TEST_F(NavigationLockTest, UnlockReset) {
    // Lock
    lock.SetLocked(true);
    lock.SetOwnerUUID("agent-123");
    lock.SetGrantedTime(std::chrono::steady_clock::now());
    lock.SetTTL(std::chrono::milliseconds(5000));
    lock.SetClientFd(42);

    EXPECT_TRUE(lock.IsLocked());

    // Unlock by resetting
    lock.Reset();

    EXPECT_FALSE(lock.IsLocked());
    EXPECT_EQ(lock.GetOwnerUUID(), "");
    EXPECT_EQ(lock.GetClientFd(), -1);
    EXPECT_FALSE(lock.IsExpired());
}
