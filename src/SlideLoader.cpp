#include "SlideLoader.h"
#include <iostream>
#include <cstring>

SlideLoader::SlideLoader(const std::string& path)
    : slide_(nullptr)
    , path_(path)
    , errorMessage_("")
{
    // Detect if file is a valid slide
    const char* vendor = openslide_detect_vendor(path.c_str());
    if (!vendor) {
        errorMessage_ = "File is not a valid whole-slide image";
        std::cerr << "SlideLoader: " << errorMessage_ << ": " << path << std::endl;
        return;
    }

    std::cout << "Detected slide vendor: " << vendor << std::endl;

    // Open the slide
    slide_ = openslide_open(path.c_str());
    if (!slide_) {
        errorMessage_ = "Failed to open slide";
        std::cerr << "SlideLoader: " << errorMessage_ << ": " << path << std::endl;
        return;
    }

    // Check for errors
    CheckError();
    if (!IsValid()) {
        return;
    }

    // Get number of levels
    int32_t levelCount = openslide_get_level_count(slide_);
    std::cout << "Slide has " << levelCount << " pyramid levels" << std::endl;

    // Cache level dimensions and downsample factors
    levelDimensions_.reserve(levelCount);
    levelDownsamples_.reserve(levelCount);

    for (int32_t i = 0; i < levelCount; ++i) {
        int64_t w, h;
        openslide_get_level_dimensions(slide_, i, &w, &h);
        levelDimensions_.push_back({w, h});

        double downsample = openslide_get_level_downsample(slide_, i);
        levelDownsamples_.push_back(downsample);

        std::cout << "  Level " << i << ": " << w << "x" << h
                  << " (downsample: " << downsample << "x)" << std::endl;
    }

    std::cout << "SlideLoader initialized successfully" << std::endl;
}

SlideLoader::~SlideLoader() {
    if (slide_) {
        openslide_close(slide_);
        slide_ = nullptr;
    }
}

SlideLoader::SlideLoader(SlideLoader&& other) noexcept
    : slide_(other.slide_)
    , path_(std::move(other.path_))
    , errorMessage_(std::move(other.errorMessage_))
    , levelDimensions_(std::move(other.levelDimensions_))
    , levelDownsamples_(std::move(other.levelDownsamples_))
{
    other.slide_ = nullptr;
}

SlideLoader& SlideLoader::operator=(SlideLoader&& other) noexcept {
    if (this != &other) {
        if (slide_) {
            openslide_close(slide_);
        }

        slide_ = other.slide_;
        path_ = std::move(other.path_);
        errorMessage_ = std::move(other.errorMessage_);
        levelDimensions_ = std::move(other.levelDimensions_);
        levelDownsamples_ = std::move(other.levelDownsamples_);

        other.slide_ = nullptr;
    }
    return *this;
}

bool SlideLoader::IsValid() const {
    if (!slide_) {
        std::cerr << "SlideLoader: Invalid slide" << std::endl;
        return false;
    }

    const char* error = openslide_get_error(slide_);
    return error == nullptr;
}

const std::string& SlideLoader::GetError() const {
    return errorMessage_;
}

int32_t SlideLoader::GetLevelCount() const {
    if (!slide_) return 0;
    return static_cast<int32_t>(levelDimensions_.size());
}

LevelDimensions SlideLoader::GetLevelDimensions(int32_t level) const {
    if (level < 0 || level >= static_cast<int32_t>(levelDimensions_.size())) {
        return {0, 0};
    }
    return levelDimensions_[level];
}

double SlideLoader::GetLevelDownsample(int32_t level) const {
    if (level < 0 || level >= static_cast<int32_t>(levelDownsamples_.size())) {
        return 1.0;
    }
    return levelDownsamples_[level];
}

int64_t SlideLoader::GetWidth() const {
    if (levelDimensions_.empty()) {
        return 0;
    }
    return levelDimensions_[0].width;
}

int64_t SlideLoader::GetHeight() const {
    if (levelDimensions_.empty()) {
        return 0;
    }
    return levelDimensions_[0].height;
}

uint32_t* SlideLoader::ReadRegion(int32_t level, int64_t x, int64_t y, int64_t width, int64_t height) {
    if (!IsValid()) {
        std::cerr << "Cannot read region: slide is not valid" << std::endl;
        return nullptr;
    }

    if (level < 0 || level >= GetLevelCount()) {
        std::cerr << "Invalid level: " << level << std::endl;
        return nullptr;
    }

    // Allocate buffer for pixels
    int64_t pixelCount = width * height;
    uint32_t* pixels = new uint32_t[pixelCount];

    // Read region from OpenSlide
    // OpenSlide returns ARGB data (pre-multiplied alpha)
    openslide_read_region(slide_, pixels, x, y, level, width, height);

    // Check for errors
    CheckError();
    if (!IsValid()) {
        delete[] pixels;
        return nullptr;
    }

    // Convert ARGB to RGBA for SDL
    ConvertARGBtoRGBA(pixels, pixelCount);

    return pixels;
}

void SlideLoader::ConvertARGBtoRGBA(uint32_t* pixels, int64_t count) {
    for (int64_t i = 0; i < count; ++i) {
        uint32_t argb = pixels[i];

        // Extract components
        uint8_t a = (argb >> 24) & 0xFF;
        uint8_t r = (argb >> 16) & 0xFF;
        uint8_t g = (argb >> 8) & 0xFF;
        uint8_t b = argb & 0xFF;

        // Repack as RGBA
        pixels[i] = (r << 0) | (g << 8) | (b << 16) | (a << 24);
    }
}

void SlideLoader::CheckError() {
    if (!slide_) return;

    const char* error = openslide_get_error(slide_);
    if (error) {
        errorMessage_ = std::string(error);
        std::cerr << "OpenSlide error: " << errorMessage_ << std::endl;
    }
}
