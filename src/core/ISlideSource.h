#pragma once

#include <string>
#include <cstdint>
#include "SlideTypes.h"

class ISlideSource {
public:
    virtual ~ISlideSource() = default;

    // Query slide validity and errors
    virtual bool IsValid() const = 0;
    virtual const std::string& GetError() const = 0;

    // Query slide properties
    virtual int32_t GetLevelCount() const = 0;
    virtual LevelDimensions GetLevelDimensions(int32_t level) const = 0;
    virtual double GetLevelDownsample(int32_t level) const = 0;

    // Get level 0 dimensions (full resolution)
    virtual int64_t GetWidth() const = 0;
    virtual int64_t GetHeight() const = 0;

    // Read a region from the slide
    // Returns RGBA pixel data (caller must delete[])
    // x, y are in level 0 coordinates
    virtual uint32_t* ReadRegion(int32_t level, int64_t x, int64_t y,
                                  int64_t width, int64_t height) = 0;

    // Get slide identifier (path for local, URL/ID for remote)
    virtual std::string GetIdentifier() const = 0;

    // Check if this is a remote slide source
    virtual bool IsRemote() const = 0;
};
