#include "Application.h"
#include <iostream>

int main() {
    std::cout << "PathView - Digital Pathology Slide Viewer" << std::endl;
    std::cout << "===========================================" << std::endl;

    Application app;

    if (!app.Initialize()) {
        std::cerr << "Failed to initialize application" << std::endl;
        return 1;
    }

    std::cout << "Application initialized. Running main loop..." << std::endl;
    app.Run();

    std::cout << "Application shutting down..." << std::endl;
    app.Shutdown();

    return 0;
}
