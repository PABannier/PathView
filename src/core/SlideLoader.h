#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <openslide/openslide.h>

struct LevelDimensions {
    int64_t width;
    int64_t height;
};

class SlideLoader {
public:
    explicit SlideLoader(const std::string& path);
    ~SlideLoader();

    // Delete copy, allow move
    SlideLoader(const SlideLoader&) = delete;
    SlideLoader& operator=(const SlideLoader&) = delete;
    SlideLoader(SlideLoader&& other) noexcept;
    SlideLoader& operator=(SlideLoader&& other) noexcept;

    // Query slide properties
    bool IsValid() const;
    const std::string& GetError() const;
    int32_t GetLevelCount() const;
    LevelDimensions GetLevelDimensions(int32_t level) const;
    double GetLevelDownsample(int32_t level) const;

    // Get level 0 dimensions (full resolution)
    int64_t GetWidth() const;
    int64_t GetHeight() const;

    // Read a region from the slide
    // Returns RGBA pixel data (caller must delete[])
    // x, y are in level 0 coordinates
    uint32_t* ReadRegion(int32_t level, int64_t x, int64_t y, int64_t width, int64_t height);

    // Get slide path
    const std::string& GetPath() const { return path_; }

private:
    void ConvertARGBtoRGBA(uint32_t* pixels, int64_t count);
    void CheckError();

    openslide_t* slide_;
    std::string path_;
    std::string errorMessage_;
    std::vector<LevelDimensions> levelDimensions_;
    std::vector<double> levelDownsamples_;
};
