#include "SnapshotManager.h"
#include "UUID.h"
#include <algorithm>

namespace pathview {
namespace http {

SnapshotManager::SnapshotManager(size_t maxSnapshots, std::chrono::milliseconds cleanupInterval)
    : maxSnapshots_(maxSnapshots)
    , cleanupInterval_(cleanupInterval)
{
    // Start cleanup thread
    if (cleanupInterval_.count() > 0) {
        cleanupThread_ = std::thread([this]() {
            std::unique_lock<std::mutex> lock(cleanupCvMutex_);
            while (running_) {
                // Sleep up to cleanupInterval_, but wake immediately on shutdown to avoid destructor stalls
                cleanupCv_.wait_for(lock, cleanupInterval_, [this]() {
                    return !running_;
                });

                if (!running_) break;

                lock.unlock();
                Cleanup();
                lock.lock();
            }
        });
    }
}

SnapshotManager::~SnapshotManager() {
    running_ = false;
    cleanupCv_.notify_all();
    if (cleanupThread_.joinable()) {
        cleanupThread_.join();
    }
}

std::string SnapshotManager::AddSnapshot(const std::vector<uint8_t>& pngData, int width, int height) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Generate UUID
    std::string id = pathview::util::GenerateUUID();

    // Evict oldest if cache is full
    while (cache_.size() >= maxSnapshots_) {
        EvictOldest();
    }

    // Create snapshot
    Snapshot snapshot{
        id,
        pngData,
        width,
        height,
        std::chrono::steady_clock::now()
    };

    // Add to cache and LRU list
    cache_[id] = std::move(snapshot);
    lruList_.push_front(id);

    return id;
}

std::optional<SnapshotManager::Snapshot> SnapshotManager::GetSnapshot(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_.find(id);
    if (it == cache_.end()) {
        return std::nullopt;
    }

    // Update last access time
    it->second.lastAccess = std::chrono::steady_clock::now();

    // Move to front of LRU list
    lruList_.remove(id);
    lruList_.push_front(id);

    return it->second;
}

void SnapshotManager::Cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::steady_clock::now();

    // Remove expired snapshots
    auto it = cache_.begin();
    while (it != cache_.end()) {
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - it->second.lastAccess);
        if (age >= TTL) {
            lruList_.remove(it->first);
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
}

size_t SnapshotManager::GetCacheSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.size();
}

void SnapshotManager::EvictOldest() {
    if (lruList_.empty()) {
        return;
    }

    // Remove oldest (back of list)
    std::string oldestId = lruList_.back();
    lruList_.pop_back();
    cache_.erase(oldestId);
}

void SnapshotManager::AddStreamFrame(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);

    streamFrameIds_.push_back(id);

    // Evict oldest if buffer full
    while (streamFrameIds_.size() > maxStreamFrames_) {
        streamFrameIds_.pop_front();
    }
}

std::string SnapshotManager::GetLatestStreamFrame() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return streamFrameIds_.empty() ? "" : streamFrameIds_.back();
}

} // namespace http
} // namespace pathview
