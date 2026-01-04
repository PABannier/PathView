#include "Minimap.h"
#include "ISlideSource.h"
#include "SlideLoader.h"  // For LevelDimensions
#include "Viewport.h"
#include <iostream>
#include <algorithm>

Minimap::Minimap(ISlideSource* source, SDL_Renderer* renderer, int windowWidth, int windowHeight)
    : source_(source)
    , renderer_(renderer)
    , overviewTexture_(nullptr)
    , overviewWidth_(0)
    , overviewHeight_(0)
    , windowWidth_(windowWidth)
    , windowHeight_(windowHeight)
{
    minimapRect_ = {0, 0, 0, 0};
    Initialize();
}

Minimap::~Minimap() {
    if (overviewTexture_) {
        SDL_DestroyTexture(overviewTexture_);
        overviewTexture_ = nullptr;
    }
}

void Minimap::Initialize() {
    if (!source_ || !source_->IsValid()) {
        std::cerr << "Minimap: Invalid slide source" << std::endl;
        return;
    }

    // Use the lowest resolution level for overview
    int32_t lowestLevel = source_->GetLevelCount() - 1;
    auto dims = source_->GetLevelDimensions(lowestLevel);

    std::cout << "Minimap: Loading overview from level " << lowestLevel
              << " (" << dims.width << "x" << dims.height << ")" << std::endl;

    // Read the entire level (it's small enough to fit in memory)
    uint32_t* pixels = source_->ReadRegion(lowestLevel, 0, 0, dims.width, dims.height);

    if (!pixels) {
        std::cerr << "Minimap: Failed to read overview region" << std::endl;
        return;
    }

    // Create texture
    overviewTexture_ = SDL_CreateTexture(
        renderer_,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STATIC,
        dims.width,
        dims.height
    );

    if (!overviewTexture_) {
        std::cerr << "Minimap: Failed to create texture: " << SDL_GetError() << std::endl;
        delete[] pixels;
        return;
    }

    // Upload pixel data
    int pitch = dims.width * sizeof(uint32_t);
    if (SDL_UpdateTexture(overviewTexture_, nullptr, pixels, pitch) != 0) {
        std::cerr << "Minimap: Failed to update texture: " << SDL_GetError() << std::endl;
        SDL_DestroyTexture(overviewTexture_);
        overviewTexture_ = nullptr;
        delete[] pixels;
        return;
    }

    delete[] pixels;

    overviewWidth_ = dims.width;
    overviewHeight_ = dims.height;

    CalculateMinimapRect();

    std::cout << "Minimap: Overview texture created successfully" << std::endl;
}

void Minimap::CalculateMinimapRect() {
    if (overviewWidth_ == 0 || overviewHeight_ == 0) {
        return;
    }

    // Calculate aspect ratio
    float aspectRatio = static_cast<float>(overviewWidth_) / overviewHeight_;

    // Calculate minimap size while maintaining aspect ratio
    int width, height;
    if (aspectRatio >= 1.0f) {
        // Landscape: width is limiting
        width = std::min(MINIMAP_MAX_SIZE, overviewWidth_);
        height = static_cast<int>(width / aspectRatio);
    } else {
        // Portrait: height is limiting
        height = std::min(MINIMAP_MAX_SIZE, overviewHeight_);
        width = static_cast<int>(height * aspectRatio);
    }

    // Position in bottom-right corner with margin
    int x = windowWidth_ - width - MINIMAP_MARGIN;
    int y = windowHeight_ - height - MINIMAP_MARGIN;

    minimapRect_ = {x, y, width, height};
}

void Minimap::SetWindowSize(int width, int height) {
    windowWidth_ = width;
    windowHeight_ = height;
    CalculateMinimapRect();
}

void Minimap::Render(const Viewport& viewport, bool sidebarVisible, float sidebarWidth) {
    if (!overviewTexture_) {
        return;
    }

    // Calculate available width accounting for sidebar
    int availableWidth = windowWidth_;
    if (sidebarVisible) {
        availableWidth -= static_cast<int>(sidebarWidth);
    }

    // Recalculate minimap position based on available width
    if (overviewWidth_ > 0 && overviewHeight_ > 0) {
        // Calculate aspect ratio
        float aspectRatio = static_cast<float>(overviewWidth_) / overviewHeight_;

        // Calculate minimap size while maintaining aspect ratio
        int width, height;
        if (aspectRatio >= 1.0f) {
            // Landscape: width is limiting
            width = std::min(MINIMAP_MAX_SIZE, overviewWidth_);
            height = static_cast<int>(width / aspectRatio);
        } else {
            // Portrait: height is limiting
            height = std::min(MINIMAP_MAX_SIZE, overviewHeight_);
            width = static_cast<int>(height * aspectRatio);
        }

        // Position in bottom-right corner with margin, accounting for sidebar
        int x = availableWidth - width - MINIMAP_MARGIN;
        int y = windowHeight_ - height - MINIMAP_MARGIN;

        minimapRect_ = {x, y, width, height};
    }

    // Draw semi-transparent background
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 128);
    SDL_RenderFillRect(renderer_, &minimapRect_);

    // Draw overview texture
    SDL_RenderCopy(renderer_, overviewTexture_, nullptr, &minimapRect_);

    // Draw border around minimap
    SDL_SetRenderDrawColor(renderer_, 200, 200, 200, 255);
    SDL_RenderDrawRect(renderer_, &minimapRect_);

    // Draw viewport rectangle
    SDL_Rect viewportRect = CalculateViewportRect(viewport);

    SDL_SetRenderDrawColor(renderer_, 255, 50, 50, 255);
    SDL_RenderDrawRect(renderer_, &viewportRect);

    // Draw thicker viewport rectangle for visibility
    SDL_Rect innerRect = {
        viewportRect.x + 1,
        viewportRect.y + 1,
        std::max(0, viewportRect.w - 2),
        std::max(0, viewportRect.h - 2)
    };
    SDL_RenderDrawRect(renderer_, &innerRect);
}

SDL_Rect Minimap::CalculateViewportRect(const Viewport& viewport) const {
    // Get viewport bounds in slide coordinates
    Rect visibleRegion = viewport.GetVisibleRegion();

    // Get slide dimensions
    int64_t slideWidth = source_->GetWidth();
    int64_t slideHeight = source_->GetHeight();

    // Calculate viewport position and size as fractions of slide
    double leftFrac = visibleRegion.x / slideWidth;
    double topFrac = visibleRegion.y / slideHeight;
    double widthFrac = visibleRegion.width / slideWidth;
    double heightFrac = visibleRegion.height / slideHeight;

    // Map to minimap coordinates
    int x = minimapRect_.x + static_cast<int>(leftFrac * minimapRect_.w);
    int y = minimapRect_.y + static_cast<int>(topFrac * minimapRect_.h);
    int w = static_cast<int>(widthFrac * minimapRect_.w);
    int h = static_cast<int>(heightFrac * minimapRect_.h);

    // Ensure minimum size for visibility
    w = std::max(2, w);
    h = std::max(2, h);

    return {x, y, w, h};
}

bool Minimap::Contains(int x, int y) const {
    return x >= minimapRect_.x && x < minimapRect_.x + minimapRect_.w &&
           y >= minimapRect_.y && y < minimapRect_.y + minimapRect_.h;
}

void Minimap::HandleClick(int x, int y, Viewport& viewport) {
    if (!Contains(x, y)) {
        return;
    }

    // Convert click position to minimap-local coordinates
    int localX = x - minimapRect_.x;
    int localY = y - minimapRect_.y;

    // Calculate fractions within minimap
    double fracX = static_cast<double>(localX) / minimapRect_.w;
    double fracY = static_cast<double>(localY) / minimapRect_.h;

    // Convert to slide coordinates
    int64_t slideWidth = source_->GetWidth();
    int64_t slideHeight = source_->GetHeight();

    double slideX = fracX * slideWidth;
    double slideY = fracY * slideHeight;

    // Center viewport on clicked position
    viewport.CenterOn({slideX, slideY});

    std::cout << "Minimap: Jumped to position (" << slideX << ", " << slideY << ")" << std::endl;
}
