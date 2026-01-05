#pragma once

#include "ISlideSource.h"
#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <openslide/openslide.h>

struct LevelDimensions {
    int64_t width;
    int64_t height;
};

class SlideLoader : public ISlideSource {
public:
    explicit SlideLoader(const std::string& path);
    ~SlideLoader() override;

    // Delete copy, allow move
    SlideLoader(const SlideLoader&) = delete;
    SlideLoader& operator=(const SlideLoader&) = delete;
    SlideLoader(SlideLoader&& other) noexcept;
    SlideLoader& operator=(SlideLoader&& other) noexcept;

    // ISlideSource interface implementation
    bool IsValid() const override;
    const std::string& GetError() const override;
    int32_t GetLevelCount() const override;
    LevelDimensions GetLevelDimensions(int32_t level) const override;
    double GetLevelDownsample(int32_t level) const override;
    int64_t GetWidth() const override;
    int64_t GetHeight() const override;
    uint32_t* ReadRegion(int32_t level, int64_t x, int64_t y,
                         int64_t width, int64_t height) override;
    std::string GetIdentifier() const override { return path_; }
    bool IsRemote() const override { return false; }

    // Get slide path (legacy accessor)
    const std::string& GetPath() const { return path_; }

private:
    void ConvertARGBtoRGBA(uint32_t* pixels, size_t count);
    void CheckError();

    openslide_t* slide_;
    std::string path_;
    std::string errorMessage_;
    std::vector<LevelDimensions> levelDimensions_;
    std::vector<double> levelDownsamples_;
};
