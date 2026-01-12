# PathView

[![CICD](https://github.com/PABannier/PathView/actions/workflows/CICD.yml/badge.svg)](https://github.com/PABannier/PathView/actions/workflows/CICD.yml)

A high-performance whole-slide image (WSI) viewer for digital pathology. PathView combines GPU-accelerated tiled rendering with polygon overlays for cell segmentation visualization, tissue classification heatmaps, and AI agent integration via the Model Context Protocol (MCP).

![PathView preview](./assets/pathview.gif)

---

## Table of Contents

- [Features](#features)
- [Quick Start](#quick-start)
- [Installation](#installation)
- [Usage](#usage)
- [Remote Slides](#remote-slides)
- [AI Agent Integration](#ai-agent-integration)
- [Data Formats](#data-formats)
- [Documentation](#documentation)
- [Contributing](#contributing)
- [License](#license)

---

## Features

| Feature | Description |
|---------|-------------|
| **High-Performance Rendering** | Smooth pan/zoom with multiresolution pyramid tile loading and LRU caching |
| **Remote Streaming** | Stream slides from S3-compatible storage via WSIStreamer tile server |
| **Polygon Overlays** | Render cell segmentation data with class-based styling from Protocol Buffers |
| **Tissue Heatmap Visualization** | GPU-accelerated tile-based tissue classification heatmap with spatial indexing for real-time visualization |
| **AI Agent Control** | Full programmatic control via MCP with 27 tools for navigation, snapshots, and ROI analysis |
| **Overview Minimap** | Click-to-jump navigation with real-time viewport indicator |
| **Cross-Platform** | Native support for macOS and Linux |

---

## Quick Start

```bash
# Clone the repository
git clone https://github.com/PABannier/PathView.git
cd PathView

# Build (macOS with Homebrew vcpkg)
export VCPKG_TARGET_TRIPLET=arm64-osx
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$(brew --prefix vcpkg)/scripts/buildsystems/vcpkg.cmake
cmake --build build -j$(nproc)

# Run
./build/pathview
```

---

## Installation

### Prerequisites

| Dependency | Description | Installation |
|------------|-------------|--------------|
| CMake 3.16+ | Build system | `brew install cmake` or `apt install cmake` |
| C++17 Compiler | GCC 8+, Clang 10+, or Apple Clang | Xcode CLI Tools or `apt install g++` |
| OpenSlide | WSI I/O library | `brew install openslide` or `apt install libopenslide-dev` |
| vcpkg | Package manager | `brew install vcpkg` or [install from source](https://vcpkg.io/en/getting-started.html) |

### Build from Source

#### macOS

```bash
# Set triplet for your architecture
export VCPKG_TARGET_TRIPLET=arm64-osx   # Apple Silicon
# export VCPKG_TARGET_TRIPLET=x64-osx   # Intel

# Configure and build
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$(brew --prefix vcpkg)/scripts/buildsystems/vcpkg.cmake

cmake --build build -j$(nproc)
```

#### Linux

```bash
export VCPKG_TARGET_TRIPLET=x64-linux

cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake

cmake --build build -j$(nproc)
```

### Verify Installation

```bash
./build/pathview --version
```

---

## Usage

### Opening Slides

| Action | Method |
|--------|--------|
| Open local slide | `File → Open Slide...` or `Ctrl+O` |
| Connect to server | `File → Connect to Server` or `Ctrl+Shift+O` |

### Navigation

| Action | Input |
|--------|-------|
| Pan | Left mouse drag |
| Zoom | Mouse wheel or trackpad pinch |
| Jump to region | Click on minimap |
| Reset view | `View → Reset View` |

### Loading Polygon Overlays

Load cell segmentation data from Protocol Buffer files:

```
File → Load Polygons...
```

Polygons are rendered with class-based coloring and level-of-detail optimization.

### Tissue Heatmap Visualization

PathView automatically loads tissue classification heatmaps when available in the Protocol Buffer file. The tissue map provides a pixel-level tissue classification overlay rendered as a colored heatmap.

**Features:**
- **Multi-level tiling**: Supports pyramid-structured tissue maps matching the slide's resolution levels
- **Class-based coloring**: Each tissue class is rendered with a distinct color
- **Spatial indexing**: O(k) viewport queries using a 2D grid spatial index for efficient rendering
- **Per-class visibility**: Toggle individual tissue classes on/off in the sidebar
- **Opacity control**: Adjust overlay transparency from 0% to 100%
- **GPU acceleration**: Tiles are cached as SDL textures for smooth rendering

**Controls:**
- View → Layers panel → Tissue Map section
- Toggle visibility with the checkbox
- Adjust opacity with the slider
- Enable/disable individual tissue classes
- Color indicators show each class's assigned color

The window title dynamically updates to show the currently loaded slide filename (for local slides) or slide ID (for remote slides), making it easy to track which slide you're viewing.

---

## Remote Slides

PathView streams slides from S3-compatible storage via [WSIStreamer](https://github.com/PABannier/WSIStreamer), enabling viewing of large slides without local download.

### Quick Demo

```bash
# Requires Docker, docker-compose, and MinIO client (mc)
./scripts/demo_remote_slide.sh ~/Downloads/sample.svs
```

This script:
1. Starts a local MinIO + WSIStreamer instance
2. Uploads your slide to the local S3 bucket
3. Launches PathView connected to the server

### Connecting to a Server

1. Open `File → Connect to Server` (`Ctrl+Shift+O`)
2. Enter the server URL (e.g., `http://localhost:3000`)
3. Enter authentication secret if required
4. Browse and select a slide from the server

### Production Deployment

For production environments, deploy WSIStreamer with your S3-compatible storage:

```bash
docker run -p 3000:3000 \
  -e WSI_S3_BUCKET=my-slides-bucket \
  -e AWS_ACCESS_KEY_ID=<your-access-key> \
  -e AWS_SECRET_ACCESS_KEY=<your-secret-key> \
  -e AWS_REGION=us-east-1 \
  wsistreamer/wsi-streamer
```

See the [WSIStreamer documentation](external/WSIStreamer/README.md) for configuration options including authentication, caching, and CORS.

---

## AI Agent Integration

PathView provides an MCP (Model Context Protocol) server for programmatic control by AI agents. The API enables automated tissue analysis workflows including navigation, screenshot capture, and ROI-based cell counting.

### Architecture

```
┌─────────────┐     MCP/JSON-RPC      ┌─────────────────┐      IPC       ┌─────────────┐
│  AI Agent   │ ◄──────────────────►  │  MCP Server     │ ◄────────────► │  PathView   │
│             │     HTTP + SSE        │  (port 9000)    │    TCP/JSON    │  GUI        │
└─────────────┘                       └─────────────────┘                └─────────────┘
                                              │
                                              ▼
                                      ┌─────────────────┐
                                      │  HTTP Server    │
                                      │  (port 8080)    │
                                      └─────────────────┘
```

### Starting the MCP Server

```bash
# Terminal 1: Start PathView GUI
./build/pathview

# Terminal 2: Start MCP server
./build/pathview-mcp
```

### Endpoints

| Service | URL | Description |
|---------|-----|-------------|
| MCP Server | `http://127.0.0.1:9000` | MCP API (JSON-RPC over HTTP+SSE) |
| HTTP Server | `http://127.0.0.1:8080` | Snapshot retrieval and MJPEG streaming |

### Available Tools

The MCP server exposes 27 tools across 7 categories:

| Category | Tools | Description |
|----------|-------|-------------|
| **Session** | `agent_hello` | Register agent and get session info |
| **Slide** | `load_slide`, `get_slide_info` | Load and inspect WSI files |
| **Navigation** | `nav_lock`, `nav_unlock`, `nav_lock_status`, `pan`, `zoom`, `zoom_at_point`, `center_on`, `reset_view`, `move_camera`, `await_move` | Viewport control with animation |
| **Snapshots** | `capture_snapshot` | Capture viewport as PNG |
| **Polygons** | `load_polygons`, `query_polygons`, `set_polygon_visibility` | Cell segmentation overlay |
| **Annotations** | `create_annotation`, `list_annotations`, `get_annotation`, `delete_annotation`, `compute_roi_metrics` | ROI creation and cell counting |
| **Action Cards** | `create_action_card`, `update_action_card`, `append_action_card_log`, `list_action_cards`, `delete_action_card` | Progress tracking UI |

### Example Workflow

```python
# 1. Connect and register
session = await client.call_tool("agent_hello", {
    "agent_name": "pathology-analyzer",
    "agent_version": "1.0.0"
})

# 2. Load resources
await client.call_tool("load_slide", {"path": "/data/slide.svs"})
await client.call_tool("load_polygons", {"path": "/data/cells.pb"})

# 3. Acquire navigation lock
await client.call_tool("nav_lock", {
    "owner_uuid": "agent-12345",
    "ttl_seconds": 300
})

# 4. Navigate to region of interest
move = await client.call_tool("move_camera", {
    "center_x": 50000, "center_y": 30000,
    "zoom": 2.0, "duration_ms": 500
})

# 5. Wait for animation and capture
while not (await client.call_tool("await_move", {"token": move["token"]}))["completed"]:
    await asyncio.sleep(0.05)
snapshot = await client.call_tool("capture_snapshot", {})

# 6. Analyze region
annotation = await client.call_tool("create_annotation", {
    "vertices": [[48000, 28000], [52000, 28000], [52000, 32000], [48000, 32000]],
    "name": "Tumor ROI"
})
print(f"Cell count: {annotation['cell_counts']['total']}")

# 7. Release lock
await client.call_tool("nav_unlock", {"owner_uuid": "agent-12345"})
```

### API Documentation

For complete API reference including all parameters, response schemas, and error codes:

- **[API Specification](API_MCP_SPECIFICATIONS.md)** - Full MCP API reference
- **[AI Agent Guide](docs/AI_AGENT_GUIDE.md)** - Integration guide and tutorials

---

## Data Formats

### Whole-Slide Images

PathView supports all formats recognized by [OpenSlide](https://openslide.org/) for local slides:

| Vendor | Extensions |
|--------|------------|
| Aperio | `.svs`, `.tif` |
| Hamamatsu | `.vms`, `.vmu`, `.ndpi` |
| Leica | `.scn` |
| MIRAX | `.mrxs` |
| Philips | `.tiff` |
| Sakura | `.svslide` |
| Trestle | `.tif` |
| Ventana | `.bif`, `.tif` |
| Generic | Tiled TIFF |

For streaming from an S3 bucket, PathView supports all formats recognized by [WSI Streamer](https://github.com/PABannier/WSIStreamer/):

| Vendor | Extensions |
|--------|------------|
| Aperio | `.svs`, `.tif` |

### Cell Segmentation and Tissue Maps

PathView uses Protocol Buffer files (`.pb`, `.protobuf`) for storing both cell polygon overlays and tissue classification heatmaps. The format supports:

**Cell Polygons:**
- Per-cell segmentation with polygon coordinates
- Cell type classification and confidence scores
- Centroid coordinates for each cell

**Tissue Heatmaps:**
- Tile-based tissue segmentation at multiple resolution levels
- Per-pixel tissue classification (uint8 class IDs)
- Optional zlib compression for compact storage
- Class ID to tissue type name mapping

**Data Structure:**
```protobuf
message SlideSegmentationData {
    required string slide_id;
    required int32 max_level;
    repeated TileSegmentationData tiles;
    map<int32, string> tissue_class_mapping;
}

message TileSegmentationData {
    required int32 level;
    required float x, y;           // Tile position in slide coordinates
    required int32 width, height;
    repeated SegmentationPolygon masks;     // Cell polygons
    required TissueSegmentationMap tissue_segmentation_map;  // Tissue heatmap
}
```

Both overlays are loaded simultaneously from a single file, with the tissue map providing context for cell-level analysis.

---

## Documentation

| Document | Description |
|----------|-------------|
| [API Specification](API_MCP_SPECIFICATIONS.md) | Complete MCP API reference with types, parameters, and examples |
| [AI Agent Guide](docs/AI_AGENT_GUIDE.md) | Integration guide for AI agent developers |
| [Architecture Guide](AGENTS.md) | Internal architecture and development patterns |

---

## Contributing

Contributions are welcome. Please follow these guidelines:

1. **Issues**: Include platform details, PathView version, and reproduction steps
2. **Pull Requests**: Follow existing code style, include tests where applicable
3. **Feature Requests**: Open an issue to discuss before implementing

### Development Build

```bash
cmake -B build-debug \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE=$(brew --prefix vcpkg)/scripts/buildsystems/vcpkg.cmake

cmake --build build-debug
```

### Running Tests

```bash
./build/test/unit_tests
```

---

## License

MIT License. See [LICENSE](LICENSE) for details.

---

<p align="center">
  <sub>Built for digital pathology research and AI-assisted tissue analysis.</sub>
</p>
