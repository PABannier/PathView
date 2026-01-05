#pragma once

#include <SDL2/SDL.h>
#include <cstdint>

class ISlideSource;
class Viewport;
struct Rect;

class Minimap {
public:
    Minimap(ISlideSource* source, SDL_Renderer* renderer, int windowWidth, int windowHeight);
    ~Minimap();

    void Render(const Viewport& viewport, bool sidebarVisible = false, float sidebarWidth = 0.0f);
    bool Contains(int x, int y) const;
    void HandleClick(int x, int y, Viewport& viewport);
    void SetWindowSize(int width, int height);

private:
    void Initialize();
    void CalculateMinimapRect();
    SDL_Rect CalculateViewportRect(const Viewport& viewport) const;

    ISlideSource* source_;
    SDL_Renderer* renderer_;

    SDL_Texture* overviewTexture_;
    int overviewWidth_;
    int overviewHeight_;

    // Minimap position and size on screen
    SDL_Rect minimapRect_;

    // Window dimensions
    int windowWidth_;
    int windowHeight_;

    // Minimap settings
    static constexpr int MINIMAP_MARGIN = 10;     // Margin from window edge
    static constexpr int MINIMAP_MAX_SIZE = 250;  // Maximum width/height
};
