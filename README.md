# PathView - Digital Pathology Slide Viewer

A cross-platform C++ application for viewing whole-slide images (WSI) commonly used in digital pathology. Built with ImGui, SDL2, and OpenSlide.

## Features

- Load and view whole-slide images (.svs, .tiff, .ndpi, etc.)
- Smooth zoom and pan navigation
- Multi-resolution tile loading for performance
- Overview minimap with click-to-jump
- Cross-platform (macOS and Linux)

## Prerequisites

### macOS

```bash
# Install Homebrew if not present
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install cmake openslide vcpkg
```

### Linux (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install cmake build-essential libopenslide-dev git curl
```

### vcpkg Setup

If not already installed via package manager:

```bash
# Clone vcpkg
git clone https://github.com/Microsoft/vcpkg.git ~/vcpkg
cd ~/vcpkg
./bootstrap-vcpkg.sh

# Add to shell profile (optional)
export VCPKG_ROOT=~/vcpkg
export PATH=$VCPKG_ROOT:$PATH
```

## Building

### 1. Install vcpkg Dependencies

```bash
cd /path/to/pathview

# For macOS Apple Silicon
export VCPKG_DEFAULT_TRIPLET=arm64-osx

# For macOS Intel
# export VCPKG_DEFAULT_TRIPLET=x64-osx

# For Linux
# export VCPKG_DEFAULT_TRIPLET=x64-linux

# Install SDL2 via vcpkg (if using homebrew vcpkg)
vcpkg install sdl2 sdl2-image

# Or if using vcpkg from source
~/vcpkg/vcpkg install sdl2 sdl2-image
```

### 2. Configure with CMake

**Using Homebrew vcpkg (macOS)**:

```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$(brew --prefix vcpkg)/scripts/buildsystems/vcpkg.cmake
```

**Using vcpkg from source**:

```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### 3. Build

```bash
cmake --build build -j$(nproc)
```

### 4. Run

```bash
./build/pathview
```

## Usage

### Opening a Slide

1. Launch PathView
2. Click "File" → "Open Slide..." or press `Ctrl+O`
3. Select a whole-slide image file (.svs, .tiff, etc.)

### Navigation

- **Zoom**: Mouse wheel or trackpad pinch
- **Pan**: Click and drag with left mouse button
- **Reset View**: "View" → "Reset View"
- **Minimap**: Click on the overview in the corner to jump to that region

## Project Structure

```
pathview/
├── CMakeLists.txt           # Build configuration
├── vcpkg.json               # Dependency manifest
├── cmake/
│   └── FindOpenSlide.cmake  # Custom OpenSlide find module
├── src/
│   ├── main.cpp             # Entry point
│   ├── Application.{h,cpp}  # Main app controller
│   ├── SlideLoader.{h,cpp}  # OpenSlide wrapper
│   ├── Viewport.{h,cpp}     # Camera/viewport management
│   ├── SlideRenderer.{h,cpp}# Rendering orchestration
│   ├── TileCache.{h,cpp}    # LRU tile cache
│   ├── TextureManager.{h,cpp}# SDL texture management
│   └── Minimap.{h,cpp}      # Overview widget
└── external/
    └── imgui/               # ImGui library (downloaded)
```

## Development

### Debug Build

```bash
cmake -B build-debug \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake

cmake --build build-debug
```

### Debugging

**macOS**:
```bash
lldb ./build-debug/pathview
```

**Linux**:
```bash
gdb ./build-debug/pathview
```

## Troubleshooting

### OpenSlide Not Found

Ensure OpenSlide is installed:

**macOS**:
```bash
brew list openslide
pkg-config --libs openslide
```

**Linux**:
```bash
dpkg -l | grep openslide
pkg-config --libs openslide
```

If installed but not found, set `CMAKE_PREFIX_PATH`:

```bash
cmake -B build -DCMAKE_PREFIX_PATH=/opt/homebrew ...  # macOS Homebrew
```

### SDL2 Not Found with vcpkg

Verify vcpkg installation:

```bash
~/vcpkg/vcpkg list | grep sdl2
```

Ensure the correct triplet is set for your architecture.

### ImGui Errors

If ImGui files are missing:

```bash
cd external/imgui
git clone --depth 1 --branch v1.91.7 https://github.com/ocornut/imgui.git .
```

## Architecture

- **SlideLoader**: RAII wrapper around OpenSlide C API
- **Viewport**: Manages camera state and coordinate transformations
- **TileCache**: LRU cache for tile data (512MB default limit)
- **SlideRenderer**: Orchestrates rendering, selects pyramid levels, enumerates tiles
- **TextureManager**: Creates and manages SDL textures
- **Minimap**: Overview widget with click-to-jump navigation
- **Application**: Main controller, event handling, UI integration

## Supported Formats

PathView supports any format that OpenSlide can read:

- Aperio (.svs, .tif)
- Hamamatsu (.vms, .vmu, .ndpi)
- Leica (.scn)
- MIRAX (.mrxs)
- Philips (.tiff)
- Sakura (.svslide)
- Trestle (.tif)
- Ventana (.bif, .tif)
- Generic tiled TIFF

## License

MIT License (add your license details here)

## Contributing

Contributions welcome! Please open an issue or pull request.

## Acknowledgments

- [OpenSlide](https://openslide.org/) - Whole-slide image reading library
- [Dear ImGui](https://github.com/ocornut/imgui) - Immediate mode GUI library
- [SDL2](https://www.libsdl.org/) - Cross-platform multimedia library
- [vcpkg](https://vcpkg.io/) - C++ package manager
