#pragma once

#include <SDL2/SDL.h>
#include <memory>
#include <string>

class SlideLoader;
class SlideRenderer;
class Minimap;
class Viewport;
class TextureManager;

class Application {
public:
    Application();
    ~Application();

    // Delete copy constructor and assignment
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    bool Initialize();
    void Run();
    void Shutdown();

private:
    void ProcessEvents();
    void Update();
    void Render();
    void RenderUI();
    void RenderSlidePreview();

    void OpenFileDialog();
    void LoadSlide(const std::string& path);

    // SDL objects
    SDL_Window* window_;
    SDL_Renderer* renderer_;

    // Application state
    bool running_;
    bool isPanning_;
    int lastMouseX_;
    int lastMouseY_;
    int windowWidth_;
    int windowHeight_;

    // Components
    std::unique_ptr<TextureManager> textureManager_;
    std::unique_ptr<SlideLoader> slideLoader_;
    std::unique_ptr<Viewport> viewport_;
    std::unique_ptr<SlideRenderer> slideRenderer_;
    std::unique_ptr<Minimap> minimap_;

    // Preview texture (Phase 2 simple display)
    SDL_Texture* previewTexture_;

    // Current slide path
    std::string currentSlidePath_;
};
