#pragma once

#include <SDL2/SDL.h>
#include <array>

namespace pathview {
namespace polygons {

inline constexpr std::array<SDL_Color, 10> kDefaultPalette = {{
    {255, 0, 0, 255},      // Red
    {0, 255, 0, 255},      // Green
    {0, 0, 255, 255},      // Blue
    {255, 255, 0, 255},    // Yellow
    {255, 0, 255, 255},    // Magenta
    {0, 255, 255, 255},    // Cyan
    {255, 128, 0, 255},    // Orange
    {128, 0, 255, 255},    // Purple
    {255, 192, 203, 255},  // Pink
    {128, 128, 128, 255}   // Gray
}};

}  // namespace polygons
}  // namespace pathview
