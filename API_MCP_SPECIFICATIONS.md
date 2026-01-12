# PathView MCP API Specification

> **Version:** 1.0.0
> **Protocol:** Model Context Protocol (MCP) over HTTP + SSE
> **Transport:** JSON-RPC 2.0

## Table of Contents

1. [Overview](#overview)
2. [Connection](#connection)
3. [Authentication & Sessions](#authentication--sessions)
4. [Navigation Lock](#navigation-lock)
5. [API Reference](#api-reference)
   - [Session Tools](#session-tools)
   - [Slide Tools](#slide-tools)
   - [Navigation Tools](#navigation-tools)
   - [Snapshot Tools](#snapshot-tools)
   - [Polygon Tools](#polygon-tools)
   - [Annotation Tools](#annotation-tools)
   - [Action Card Tools](#action-card-tools)
6. [HTTP Endpoints](#http-endpoints)
7. [Error Reference](#error-reference)
8. [Type Definitions](#type-definitions)

---

## Overview

The PathView MCP API enables programmatic control of the PathView digital pathology viewer. Through this API, AI agents can:

- Load and inspect whole-slide images (WSI)
- Control viewport navigation with smooth animations
- Capture screenshots of the current view
- Load and query cell segmentation polygon overlays
- Create ROI annotations with automatic cell counting
- Display progress tracking cards in the UI

### Architecture

```
┌─────────────┐     MCP/JSON-RPC      ┌─────────────────┐      IPC       ┌─────────────┐
│  AI Agent   │ ◄──────────────────►  │  MCP Server     │ ◄────────────► │  PathView   │
│             │     HTTP + SSE        │  (port 9000)    │    TCP/JSON    │  GUI        │
└─────────────┘                       └─────────────────┘                └─────────────┘
                                              │
                                              │ Snapshots
                                              ▼
                                      ┌─────────────────┐
                                      │  HTTP Server    │
                                      │  (port 8080)    │
                                      └─────────────────┘
```

---

## Connection

### Endpoints

| Service | URL | Description |
|---------|-----|-------------|
| MCP Server | `http://127.0.0.1:9000` | Main MCP API endpoint |
| SSE Endpoint | `http://127.0.0.1:9000/sse` | Server-Sent Events for MCP |
| HTTP Server | `http://127.0.0.1:8080` | Snapshot and streaming server |

### Starting the Services

```bash
# Terminal 1: Start the PathView GUI
./build/pathview

# Terminal 2: Start the MCP server
./build/pathview-mcp
```

### Connection Example

```python
from mcp import Client

async with Client("http://127.0.0.1:9000") as client:
    result = await client.call_tool("agent_hello", {
        "agent_name": "my-agent",
        "agent_version": "1.0.0"
    })
```

---

## Authentication & Sessions

PathView uses a simple handshake-based session model. No authentication tokens are required for localhost connections.

### Session Lifecycle

1. **Connect** to the MCP server
2. **Handshake** via `agent_hello` to register and get session info
3. **Acquire lock** via `nav_lock` before navigation operations
4. **Perform operations** (load slides, navigate, capture, analyze)
5. **Release lock** via `nav_unlock` when done
6. **Disconnect** from the server

---

## Navigation Lock

The navigation lock prevents user interference during agent-controlled operations. All viewport manipulation tools require holding the navigation lock.

### Lock Behavior

| Property | Value |
|----------|-------|
| Maximum TTL | 3600 seconds (1 hour) |
| Default TTL | 300 seconds (5 minutes) |
| Auto-release on disconnect | Yes |
| Auto-release on TTL expiry | Yes |

### Tools Requiring Lock

All navigation tools require an active lock:

- `pan`
- `zoom`
- `zoom_at_point`
- `center_on`
- `reset_view`
- `move_camera`

### Lock Error Response

When invoking a navigation tool without holding the lock:

```json
{
  "error": {
    "code": -32603,
    "message": "Navigation locked by agent-xyz. Use nav_lock tool to acquire control."
  }
}
```

---

## API Reference

### Session Tools

#### `agent_hello`

Register agent identity and retrieve session information. This should be the first tool called after connecting.

**Parameters**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `agent_name` | `string` | Yes | Identifier for the AI agent |
| `agent_version` | `string` | No | Version string of the agent |

**Response**

```typescript
interface AgentHelloResponse {
  session_id: string;              // Unique session identifier
  agent_name: string;              // Echoed agent name
  pathview_version: string;        // PathView version (e.g., "0.2.0")
  mcp_server_url: string;          // MCP server URL
  http_server_url: string;         // HTTP server URL for snapshots
  stream_url: string;              // MJPEG stream base URL
  stream_fps_default: number;      // Default stream FPS (5)
  stream_fps_max: number;          // Maximum stream FPS (30)
  ipc_port: number;                // IPC server port
  navigation_locked: boolean;      // Whether navigation is currently locked
  lock_owner: string;              // UUID of current lock owner (empty if unlocked)
  viewport?: ViewportState;        // Current viewport state (if slide loaded)
  slide?: SlideInfo;               // Current slide info (if loaded)
}
```

**Example**

```json
// Request
{
  "agent_name": "pathology-analyzer",
  "agent_version": "1.0.0"
}

// Response
{
  "session_id": "550e8400-e29b-41d4-a716-446655440000",
  "agent_name": "pathology-analyzer",
  "pathview_version": "0.2.0",
  "mcp_server_url": "http://127.0.0.1:9000",
  "http_server_url": "http://127.0.0.1:8080",
  "stream_url": "http://127.0.0.1:8080/stream",
  "stream_fps_default": 5,
  "stream_fps_max": 30,
  "ipc_port": 9001,
  "navigation_locked": false,
  "lock_owner": ""
}
```

**Errors**

| Code | Message | Cause |
|------|---------|-------|
| `-32602` | `Missing 'agent_name' parameter` | `agent_name` not provided |

---

### Slide Tools

#### `load_slide`

Load a whole-slide image file into the viewer.

**Parameters**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `path` | `string` | Yes | Absolute path to the slide file |

**Supported Formats**

- Aperio (`.svs`, `.tif`)
- Hamamatsu (`.vms`, `.vmu`, `.ndpi`)
- Leica (`.scn`)
- MIRAX (`.mrxs`)
- Philips (`.tiff`)
- Sakura (`.svslide`)
- Trestle (`.tif`)
- Ventana (`.bif`, `.tif`)
- Generic tiled TIFF

**Response**

```typescript
interface LoadSlideResponse {
  width: number;    // Slide width in pixels (level 0)
  height: number;   // Slide height in pixels (level 0)
  levels: number;   // Number of pyramid levels
  path: string;     // Absolute path to loaded slide
}
```

**Example**

```json
// Request
{
  "path": "/data/slides/sample.svs"
}

// Response
{
  "width": 98304,
  "height": 75264,
  "levels": 7,
  "path": "/data/slides/sample.svs"
}
```

**Errors**

| Code | Message | Cause |
|------|---------|-------|
| `-32602` | `Missing 'path' parameter` | `path` not provided |
| `-32603` | `Failed to load slide` | File not found or unsupported format |

---

#### `get_slide_info`

Get information about the currently loaded slide and viewport state.

**Parameters**

None.

**Response**

```typescript
interface GetSlideInfoResponse {
  width: number;              // Slide width in pixels (level 0)
  height: number;             // Slide height in pixels (level 0)
  levels: number;             // Number of pyramid levels
  path: string;               // Path to current slide
  viewport?: {
    position: Position;       // Current viewport position
    zoom: number;             // Current zoom level
    window_width: number;     // Viewport window width
    window_height: number;    // Viewport window height
  };
}
```

**Example**

```json
// Response
{
  "width": 98304,
  "height": 75264,
  "levels": 7,
  "path": "/data/slides/sample.svs",
  "viewport": {
    "position": { "x": 25000, "y": 20000 },
    "zoom": 1.5,
    "window_width": 1920,
    "window_height": 1080
  }
}
```

**Errors**

| Code | Message | Cause |
|------|---------|-------|
| `-32603` | `No slide loaded` | No slide currently loaded |

---

### Navigation Tools

All navigation tools require holding the navigation lock. Without a lock, they return an error.

#### `nav_lock`

Acquire exclusive navigation control. If already locked by the same owner, the lock is renewed.

**Parameters**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `owner_uuid` | `string` | Yes | Unique identifier for the lock owner |
| `ttl_seconds` | `number` | No | Lock lifetime in seconds (1-3600, default: 300) |

**Response**

```typescript
// Success
interface NavLockSuccessResponse {
  success: true;
  lock_owner: string;         // Confirmed owner UUID
  granted_at: number;         // Timestamp (ms since epoch)
  ttl_ms: number;             // Granted TTL in milliseconds
}

// Failure (already locked by another agent)
interface NavLockFailureResponse {
  success: false;
  error: string;              // Error message
  lock_owner: string;         // Current lock owner
  time_remaining_ms: number;  // Time until lock expires
}
```

**Example**

```json
// Request
{
  "owner_uuid": "agent-12345",
  "ttl_seconds": 600
}

// Success Response
{
  "success": true,
  "lock_owner": "agent-12345",
  "granted_at": 1702732200000,
  "ttl_ms": 600000
}

// Failure Response
{
  "success": false,
  "error": "Navigation already locked by another agent",
  "lock_owner": "agent-other",
  "time_remaining_ms": 150000
}
```

**Errors**

| Code | Message | Cause |
|------|---------|-------|
| `-32602` | `Missing 'owner_uuid' parameter` | `owner_uuid` not provided |

---

#### `nav_unlock`

Release the navigation lock. Only the lock owner can release the lock.

**Parameters**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `owner_uuid` | `string` | Yes | UUID of the lock owner |

**Response**

```typescript
// Success
interface NavUnlockSuccessResponse {
  success: true;
  message: string;  // "Navigation unlocked"
}

// Failure
interface NavUnlockFailureResponse {
  success: false;
  error: string;           // Error message
  lock_owner?: string;     // Current owner (if locked by another)
}
```

**Example**

```json
// Request
{
  "owner_uuid": "agent-12345"
}

// Success Response
{
  "success": true,
  "message": "Navigation unlocked"
}

// Failure Response (not owner)
{
  "success": false,
  "error": "Not the lock owner",
  "lock_owner": "agent-other"
}

// Failure Response (not locked)
{
  "success": false,
  "error": "Navigation not locked"
}
```

**Errors**

| Code | Message | Cause |
|------|---------|-------|
| `-32602` | `Missing 'owner_uuid' parameter` | `owner_uuid` not provided |

---

#### `nav_lock_status`

Query the current navigation lock status.

**Parameters**

None.

**Response**

```typescript
// Unlocked
interface NavLockStatusUnlocked {
  locked: false;
}

// Locked
interface NavLockStatusLocked {
  locked: true;
  owner_uuid: string;           // Lock owner UUID
  time_remaining_ms: number;    // Time until expiry
  granted_at: number;           // Timestamp (ms since epoch)
}
```

**Example**

```json
// Response (locked)
{
  "locked": true,
  "owner_uuid": "agent-12345",
  "time_remaining_ms": 250000,
  "granted_at": 1702732200000
}

// Response (unlocked)
{
  "locked": false
}
```

---

#### `move_camera`

Move the camera to a target position with smooth animation. Returns a token for polling completion status.

**Parameters**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `center_x` | `number` | Yes | Target center X coordinate (slide space, level 0) |
| `center_y` | `number` | Yes | Target center Y coordinate (slide space, level 0) |
| `zoom` | `number` | Yes | Target zoom level (1.0 = fit to window) |
| `duration_ms` | `number` | No | Animation duration (50-5000ms, default: 300) |

**Response**

```typescript
interface MoveCameraResponse {
  token: string;  // Animation token for await_move
}
```

**Behavior**

- Initiates smooth animated camera movement to the target position
- Aborts any existing tracked animation
- Duration is clamped to range [50, 5000] ms
- Position is clamped to slide bounds

**Example**

```json
// Request
{
  "center_x": 50000,
  "center_y": 30000,
  "zoom": 2.0,
  "duration_ms": 500
}

// Response
{
  "token": "anim-550e8400-e29b-41d4-a716-446655440000"
}
```

**Errors**

| Code | Message | Cause |
|------|---------|-------|
| `-32602` | `Missing required parameters: center_x, center_y, zoom` | Required parameter missing |
| `-32603` | `No slide loaded...` | No slide loaded |
| `-32603` | `Navigation locked by...` | Lock not held |

---

#### `await_move`

Poll for camera animation completion status.

**Parameters**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `token` | `string` | Yes | Animation token from `move_camera` |

**Response**

```typescript
interface AwaitMoveResponse {
  completed: boolean;       // Whether animation has finished
  aborted: boolean;         // Whether animation was interrupted
  position: Position;       // Final or current position
  zoom: number;             // Final or current zoom level
}
```

**Example**

```json
// Request
{
  "token": "anim-550e8400-e29b-41d4-a716-446655440000"
}

// Response (in progress)
{
  "completed": false,
  "aborted": false,
  "position": { "x": 45000, "y": 28000 },
  "zoom": 1.8
}

// Response (completed)
{
  "completed": true,
  "aborted": false,
  "position": { "x": 50000, "y": 30000 },
  "zoom": 2.0
}
```

**Errors**

| Code | Message | Cause |
|------|---------|-------|
| `-32602` | `Missing required parameter: token` | Token not provided |
| `-32603` | `Unknown animation token: ...` | Invalid or expired token |

---

#### `pan`

Pan the viewport by a delta offset.

**Parameters**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `dx` | `number` | Yes | X delta in slide coordinates |
| `dy` | `number` | Yes | Y delta in slide coordinates |

**Response**

```typescript
interface PanResponse {
  position: Position;  // New viewport position
  zoom: number;        // Current zoom level
}
```

**Example**

```json
// Request
{
  "dx": 500,
  "dy": -200
}

// Response
{
  "position": { "x": 25500, "y": 19800 },
  "zoom": 1.5
}
```

**Errors**

| Code | Message | Cause |
|------|---------|-------|
| `-32602` | `Missing 'dx' or 'dy' parameters` | Required parameter missing |
| `-32603` | `No slide loaded...` | No slide loaded |
| `-32603` | `Navigation locked by...` | Lock not held |

---

#### `zoom`

Zoom in or out at the viewport center.

**Parameters**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `delta` | `number` | Yes | Zoom factor (>1.0 zooms in, <1.0 zooms out) |

**Response**

```typescript
interface ZoomResponse {
  zoom: number;        // New zoom level
  position: Position;  // Current position
}
```

**Example**

```json
// Request (zoom in 10%)
{
  "delta": 1.1
}

// Response
{
  "zoom": 1.65,
  "position": { "x": 25000, "y": 20000 }
}
```

**Errors**

| Code | Message | Cause |
|------|---------|-------|
| `-32602` | `Missing 'delta' parameter` | `delta` not provided |
| `-32603` | `No slide loaded...` | No slide loaded |
| `-32603` | `Navigation locked by...` | Lock not held |

---

#### `zoom_at_point`

Zoom at a specific screen coordinate.

**Parameters**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `screen_x` | `number` | Yes | Screen X coordinate |
| `screen_y` | `number` | Yes | Screen Y coordinate |
| `delta` | `number` | Yes | Zoom factor |

**Response**

```typescript
interface ZoomAtPointResponse {
  zoom: number;        // New zoom level
  position: Position;  // New position
}
```

**Errors**

| Code | Message | Cause |
|------|---------|-------|
| `-32602` | `Missing 'screen_x', 'screen_y', or 'delta' parameters` | Required parameter missing |
| `-32603` | `No slide loaded...` | No slide loaded |
| `-32603` | `Navigation locked by...` | Lock not held |

---

#### `center_on`

Center the viewport on specific slide coordinates.

**Parameters**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `x` | `number` | Yes | X coordinate (slide space, level 0) |
| `y` | `number` | Yes | Y coordinate (slide space, level 0) |

**Response**

```typescript
interface CenterOnResponse {
  position: Position;  // New position
  zoom: number;        // Current zoom level
}
```

**Errors**

| Code | Message | Cause |
|------|---------|-------|
| `-32602` | `Missing 'x' or 'y' parameters` | Required parameter missing |
| `-32603` | `No slide loaded...` | No slide loaded |
| `-32603` | `Navigation locked by...` | Lock not held |

---

#### `reset_view`

Reset the viewport to fit the entire slide in the window.

**Parameters**

None.

**Response**

```typescript
interface ResetViewResponse {
  position: Position;  // New position (top-left of slide)
  zoom: number;        // New zoom level (fit to window)
}
```

**Errors**

| Code | Message | Cause |
|------|---------|-------|
| `-32603` | `No slide loaded...` | No slide loaded |
| `-32603` | `Navigation locked by...` | Lock not held |

---

### Snapshot Tools

#### `capture_snapshot`

Capture the current viewport as a PNG image.

**Parameters**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `width` | `number` | No | Target width (default: viewport width) |
| `height` | `number` | No | Target height (default: viewport height) |

> **Note:** Custom width/height are currently not implemented. Snapshots are captured at window resolution.

**Response**

```typescript
interface CaptureSnapshotResponse {
  id: string;      // Unique snapshot identifier (UUID)
  url: string;     // URL to retrieve the snapshot
  width: number;   // Captured image width
  height: number;  // Captured image height
}
```

**Example**

```json
// Response
{
  "id": "550e8400-e29b-41d4-a716-446655440000",
  "url": "http://127.0.0.1:8080/snapshot/550e8400-e29b-41d4-a716-446655440000",
  "width": 1920,
  "height": 1080
}
```

**Errors**

| Code | Message | Cause |
|------|---------|-------|
| `-32603` | `Failed to capture screenshot` | Capture failed internally |

---

### Polygon Tools

#### `load_polygons`

Load cell segmentation polygon overlay from a Protocol Buffer file.

**Parameters**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `path` | `string` | Yes | Absolute path to `.pb` or `.protobuf` file |

**Response**

```typescript
interface LoadPolygonsResponse {
  count: number;      // Number of polygons loaded
  classes: number[];  // Array of class IDs present in the data
}
```

**Example**

```json
// Request
{
  "path": "/data/cells.pb"
}

// Response
{
  "count": 45678,
  "classes": [1, 2, 3, 4]
}
```

**Errors**

| Code | Message | Cause |
|------|---------|-------|
| `-32602` | `Missing 'path' parameter` | `path` not provided |
| `-32603` | `Failed to load polygons` | File not found or invalid format |

---

#### `query_polygons`

Query polygons within a rectangular region.

**Parameters**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `x` | `number` | Yes | Region X coordinate (slide space) |
| `y` | `number` | Yes | Region Y coordinate (slide space) |
| `w` | `number` | Yes | Region width |
| `h` | `number` | Yes | Region height |

**Response**

```typescript
interface QueryPolygonsResponse {
  polygons: Polygon[];  // Array of polygons in region
}
```

> **Note:** Full polygon query is not yet implemented. Currently returns an empty array.

**Errors**

| Code | Message | Cause |
|------|---------|-------|
| `-32602` | `Missing 'x', 'y', 'w', or 'h' parameters` | Required parameter missing |
| `-32603` | `No polygons loaded...` | No polygons loaded |

---

#### `set_polygon_visibility`

Show or hide the polygon overlay.

**Parameters**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `visible` | `boolean` | Yes | `true` to show, `false` to hide |

**Response**

```typescript
interface SetPolygonVisibilityResponse {
  visible: boolean;  // Current visibility state
}
```

**Example**

```json
// Request
{
  "visible": true
}

// Response
{
  "visible": true
}
```

**Errors**

| Code | Message | Cause |
|------|---------|-------|
| `-32602` | `Missing 'visible' parameter` | `visible` not provided |
| `-32603` | `No polygons loaded...` | No polygons loaded |

---

### Annotation Tools

Annotations are persistent ROI regions that can be used for cell counting and analysis.

#### `create_annotation`

Create a polygon annotation with automatic cell counting.

**Parameters**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `vertices` | `number[][]` | Yes | Array of `[x, y]` coordinate pairs (min 3 points) |
| `name` | `string` | No | Name/label for the annotation |

**Response**

```typescript
interface CreateAnnotationResponse {
  id: number;                    // Unique annotation ID
  name: string;                  // Annotation name
  vertex_count: number;          // Number of vertices
  bounding_box: BoundingBox;     // Bounding rectangle
  area: number;                  // Area in square pixels (level 0)
  cell_counts: CellCounts;       // Cell counts by class
  warning?: string;              // Warning if polygons not loaded
}
```

**Example**

```json
// Request
{
  "vertices": [
    [48000, 28000],
    [52000, 28000],
    [52000, 32000],
    [48000, 32000]
  ],
  "name": "Tumor Region 1"
}

// Response (with polygons loaded)
{
  "id": 1,
  "name": "Tumor Region 1",
  "vertex_count": 4,
  "bounding_box": {
    "x": 48000,
    "y": 28000,
    "width": 4000,
    "height": 4000
  },
  "area": 16000000.0,
  "cell_counts": {
    "1": 234,
    "2": 56,
    "3": 12,
    "total": 302
  }
}

// Response (without polygons loaded)
{
  "id": 1,
  "name": "Tumor Region 1",
  "vertex_count": 4,
  "bounding_box": {
    "x": 48000,
    "y": 28000,
    "width": 4000,
    "height": 4000
  },
  "area": 16000000.0,
  "cell_counts": {},
  "warning": "No polygons loaded. Cell counts unavailable. Use load_polygons to enable cell counting."
}
```

**Errors**

| Code | Message | Cause |
|------|---------|-------|
| `-32602` | `Missing 'vertices' parameter` | `vertices` not provided |
| `-32603` | `vertices must be an array with at least 3 points` | Invalid vertex array |
| `-32603` | `Each vertex must be an array of [x, y]` | Invalid vertex format |
| `-32603` | `No slide loaded...` | No slide loaded |
| `-32603` | `Failed to create annotation (invalid vertices)` | Invalid polygon geometry |

---

#### `list_annotations`

List all annotations with optional cell metrics.

**Parameters**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `include_metrics` | `boolean` | No | Include cell counts (default: `false`) |

**Response**

```typescript
interface ListAnnotationsResponse {
  annotations: AnnotationSummary[];  // Array of annotation summaries
  count: number;                     // Total annotation count
}

interface AnnotationSummary {
  id: number;
  name: string;
  vertex_count: number;
  bounding_box: BoundingBox;
  area: number;
  cell_counts?: CellCounts;  // Only if include_metrics is true
}
```

**Example**

```json
// Request
{
  "include_metrics": true
}

// Response
{
  "annotations": [
    {
      "id": 1,
      "name": "Tumor Region 1",
      "vertex_count": 4,
      "bounding_box": { "x": 48000, "y": 28000, "width": 4000, "height": 4000 },
      "area": 16000000.0,
      "cell_counts": { "1": 234, "2": 56, "total": 290 }
    }
  ],
  "count": 1
}
```

**Errors**

| Code | Message | Cause |
|------|---------|-------|
| `-32603` | `No slide loaded` | No slide loaded |

---

#### `get_annotation`

Get detailed information about a specific annotation.

**Parameters**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `id` | `number` | Yes | Annotation ID |

**Response**

```typescript
interface GetAnnotationResponse {
  id: number;
  name: string;
  vertices: number[][];        // Full vertex coordinates
  vertex_count: number;
  bounding_box: BoundingBox;
  area: number;
  perimeter: number;           // Perimeter in pixels
  cell_counts: CellCounts;
}
```

**Errors**

| Code | Message | Cause |
|------|---------|-------|
| `-32602` | `Missing 'id' parameter` | `id` not provided |
| `-32603` | `Annotation with id X not found` | Invalid annotation ID |
| `-32603` | `No slide loaded` | No slide loaded |

---

#### `delete_annotation`

Delete an annotation by ID.

**Parameters**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `id` | `number` | Yes | Annotation ID to delete |

**Response**

```typescript
interface DeleteAnnotationResponse {
  success: true;
  deleted_id: number;
}
```

**Errors**

| Code | Message | Cause |
|------|---------|-------|
| `-32602` | `Missing 'id' parameter` | `id` not provided |
| `-32603` | `Annotation with id X not found` | Invalid annotation ID |
| `-32603` | `No slide loaded` | No slide loaded |

---

#### `compute_roi_metrics`

Compute metrics for a polygon region without creating a persistent annotation (quick probe).

**Parameters**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `vertices` | `number[][]` | Yes | Array of `[x, y]` coordinate pairs (min 3 points) |

**Response**

```typescript
interface ComputeROIMetricsResponse {
  bounding_box: BoundingBox;
  area: number;
  perimeter: number;
  cell_counts: CellCounts;
  warning?: string;  // Warning if polygons not loaded
}
```

**Example**

```json
// Request
{
  "vertices": [
    [48000, 28000],
    [52000, 28000],
    [52000, 32000],
    [48000, 32000]
  ]
}

// Response
{
  "bounding_box": { "x": 48000, "y": 28000, "width": 4000, "height": 4000 },
  "area": 16000000.0,
  "perimeter": 16000.0,
  "cell_counts": { "1": 234, "2": 56, "total": 290 }
}
```

**Errors**

| Code | Message | Cause |
|------|---------|-------|
| `-32602` | `Missing 'vertices' parameter` | `vertices` not provided |
| `-32603` | `vertices must be an array with at least 3 points` | Invalid vertex array |
| `-32603` | `No slide loaded...` | No slide loaded |

---

### Action Card Tools

Action cards display agent progress and reasoning in the PathView UI.

#### `create_action_card`

Create a new action card for tracking agent task progress.

**Parameters**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `title` | `string` | Yes | Short title for the action (max ~50 chars) |
| `summary` | `string` | No | Brief description of the task |
| `reasoning` | `string` | No | Detailed reasoning or plan |
| `owner_uuid` | `string` | No | UUID of the creating agent |

**Response**

```typescript
interface CreateActionCardResponse {
  id: string;           // Unique card ID (UUID)
  title: string;        // Card title
  status: ActionStatus; // Initial status ("pending")
  created_at: number;   // Timestamp (ms since epoch)
}

type ActionStatus = "pending" | "in_progress" | "completed" | "failed" | "cancelled";
```

**Example**

```json
// Request
{
  "title": "Analyzing tumor region",
  "summary": "Scanning high-density cell area",
  "reasoning": "Step 1: Navigate to ROI. Step 2: Capture snapshots.",
  "owner_uuid": "agent-12345"
}

// Response
{
  "id": "card-550e8400-e29b-41d4-a716-446655440000",
  "title": "Analyzing tumor region",
  "status": "pending",
  "created_at": 1702732200000
}
```

**Errors**

| Code | Message | Cause |
|------|---------|-------|
| `-32602` | `Missing 'title' parameter` | `title` not provided |

**Edge Cases**

- Maximum 100 action cards are stored. When limit is reached, the oldest completed/failed/cancelled card is removed.

---

#### `update_action_card`

Update an action card's status or content.

**Parameters**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `id` | `string` | Yes | Action card ID |
| `status` | `string` | No | New status |
| `summary` | `string` | No | Updated summary |
| `reasoning` | `string` | No | Updated reasoning |

**Valid Status Values**

- `pending` - Task not yet started
- `in_progress` - Currently executing
- `completed` - Successfully finished
- `failed` - Ended with error
- `cancelled` - Manually cancelled

**Response**

```typescript
interface UpdateActionCardResponse {
  id: string;
  status: ActionStatus;
  updated_at: number;
}
```

**Example**

```json
// Request
{
  "id": "card-550e8400-e29b-41d4-a716-446655440000",
  "status": "completed",
  "summary": "Analysis complete. Found 302 cells."
}

// Response
{
  "id": "card-550e8400-e29b-41d4-a716-446655440000",
  "status": "completed",
  "updated_at": 1702732500000
}
```

**Errors**

| Code | Message | Cause |
|------|---------|-------|
| `-32602` | `Missing 'id' parameter` | `id` not provided |
| `-32603` | `Action card not found: ...` | Invalid card ID |

---

#### `append_action_card_log`

Append a log entry to an action card for incremental progress updates.

**Parameters**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `id` | `string` | Yes | Action card ID |
| `message` | `string` | Yes | Log message text |
| `level` | `string` | No | Log level (default: `info`) |

**Valid Log Levels**

- `info` - Informational message
- `success` - Success message (green)
- `warning` - Warning message (yellow)
- `error` - Error message (red)

**Response**

```typescript
interface AppendActionCardLogResponse {
  id: string;
  log_count: number;    // Total number of log entries
  updated_at: number;
}
```

**Example**

```json
// Request
{
  "id": "card-550e8400-e29b-41d4-a716-446655440000",
  "message": "Moved camera to position (50000, 30000)",
  "level": "info"
}

// Response
{
  "id": "card-550e8400-e29b-41d4-a716-446655440000",
  "log_count": 5,
  "updated_at": 1702732400000
}
```

**Errors**

| Code | Message | Cause |
|------|---------|-------|
| `-32602` | `Missing 'id' or 'message' parameter` | Required parameter missing |
| `-32603` | `Action card not found: ...` | Invalid card ID |

---

#### `list_action_cards`

List all action cards with summary information.

**Parameters**

None.

**Response**

```typescript
interface ListActionCardsResponse {
  cards: ActionCardSummary[];
  count: number;
}

interface ActionCardSummary {
  id: string;
  title: string;
  status: ActionStatus;
  summary: string;
  owner_uuid: string;
  log_entry_count: number;
  created_at: number;
  updated_at: number;
}
```

**Example**

```json
// Response
{
  "cards": [
    {
      "id": "card-550e8400-e29b-41d4-a716-446655440000",
      "title": "Analyzing tumor region",
      "status": "in_progress",
      "summary": "Scanning high-density cell area",
      "owner_uuid": "agent-12345",
      "log_entry_count": 3,
      "created_at": 1702732200000,
      "updated_at": 1702732400000
    }
  ],
  "count": 1
}
```

---

#### `delete_action_card`

Delete an action card by ID.

**Parameters**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `id` | `string` | Yes | Action card ID |

**Response**

```typescript
interface DeleteActionCardResponse {
  success: true;
  deleted_id: string;
}
```

**Errors**

| Code | Message | Cause |
|------|---------|-------|
| `-32602` | `Missing 'id' parameter` | `id` not provided |
| `-32603` | `Action card not found: ...` | Invalid card ID |

---

## HTTP Endpoints

The HTTP server (port 8080) provides snapshot retrieval and live streaming.

### `GET /health`

Health check endpoint.

**Response**

```
200 OK
Content-Type: text/plain

OK
```

---

### `GET /snapshot/{id}`

Retrieve a captured snapshot image.

**Parameters**

| Name | Location | Type | Description |
|------|----------|------|-------------|
| `id` | Path | `string` | Snapshot UUID from `capture_snapshot` |

**Response**

| Status | Content-Type | Body |
|--------|--------------|------|
| `200 OK` | `image/png` | PNG image data |
| `404 Not Found` | `text/plain` | `Snapshot not found` |

**Example**

```bash
curl http://127.0.0.1:8080/snapshot/550e8400-e29b-41d4-a716-446655440000 > snapshot.png
```

**Cache Behavior**

- Snapshots are cached for 1 hour (TTL)
- Maximum 50 snapshots are stored (LRU eviction)
- Automatic cleanup every 30 seconds

---

### `GET /stream`

MJPEG video stream of viewport snapshots.

**Query Parameters**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `fps` | `number` | No | Frames per second (1-30, default: 5) |

**Response**

```
200 OK
Content-Type: multipart/x-mixed-replace; boundary=frame
Cache-Control: no-cache
Connection: keep-alive
```

**Frame Format**

Each frame is delivered as:

```
--frame
Content-Type: image/png
Content-Length: <size>

<PNG binary data>
```

**Example**

```python
import httpx

async with httpx.AsyncClient(timeout=None) as client:
    async with client.stream("GET", "http://127.0.0.1:8080/stream?fps=10") as response:
        async for chunk in response.aiter_bytes():
            # Process MJPEG frames
            pass
```

**Notes**

- The stream only sends frames when snapshots are captured via `capture_snapshot`
- Maximum 30 FPS
- Stream maintains a circular buffer of the 3 most recent frames

---

### `GET /`

Server information page (HTML).

**Response**

```
200 OK
Content-Type: text/html
```

---

## Error Reference

### JSON-RPC Error Codes

| Code | Name | Description |
|------|------|-------------|
| `-32700` | Parse Error | Invalid JSON received |
| `-32600` | Invalid Request | JSON is not a valid request object |
| `-32601` | Method Not Found | Method does not exist |
| `-32602` | Invalid Params | Invalid method parameters |
| `-32603` | Internal Error | Internal JSON-RPC error |

### Application Error Codes

| Code | Name | Description |
|------|------|-------------|
| `-32000` | No Slide Loaded | Operation requires a loaded slide |
| `-32001` | No Polygons Loaded | Operation requires loaded polygons |
| `-32002` | File Not Found | Specified file does not exist |
| `-32003` | Invalid Operation | Operation not allowed in current state |
| `-32004` | Annotation Not Found | Specified annotation does not exist |

### Error Response Format

```typescript
interface ErrorResponse {
  error: {
    code: number;
    message: string;
    data?: any;
  }
}
```

**Example**

```json
{
  "error": {
    "code": -32603,
    "message": "No slide loaded. Use load_slide tool to load a whole-slide image first."
  }
}
```

---

## Type Definitions

### Position

```typescript
interface Position {
  x: number;  // X coordinate (slide space, level 0)
  y: number;  // Y coordinate (slide space, level 0)
}
```

### BoundingBox

```typescript
interface BoundingBox {
  x: number;       // Top-left X
  y: number;       // Top-left Y
  width: number;   // Width in pixels
  height: number;  // Height in pixels
}
```

### CellCounts

```typescript
interface CellCounts {
  [classId: string]: number;  // Count per class ID
  total?: number;             // Total cell count (if any cells present)
}
```

### ViewportState

```typescript
interface ViewportState {
  position: Position;
  zoom: number;
  window_width: number;
  window_height: number;
}
```

### SlideInfo

```typescript
interface SlideInfo {
  width: number;
  height: number;
  levels: number;
  path: string;
}
```

---

## Coordinate Systems

### Slide Coordinates (Level 0)

- **Origin:** Top-left corner of the slide
- **Units:** Pixels at highest resolution (level 0)
- **Used by:** `move_camera`, `center_on`, `create_annotation`, polygon data
- **Example:** `{ "x": 50000, "y": 30000 }` = 50,000 pixels from left edge

### Screen Coordinates

- **Origin:** Top-left corner of the viewport window
- **Units:** Screen pixels
- **Used by:** `zoom_at_point`
- **Example:** `{ "screen_x": 960, "screen_y": 540 }` = center of 1920x1080 viewport

---

## Rate Limits & Constraints

| Resource | Limit |
|----------|-------|
| Maximum navigation lock TTL | 3600 seconds (1 hour) |
| Default navigation lock TTL | 300 seconds (5 minutes) |
| Animation duration range | 50-5000 milliseconds |
| Maximum action cards | 100 |
| Maximum cached snapshots | 50 |
| Snapshot TTL | 1 hour |
| MJPEG stream max FPS | 30 |
| Stream frame buffer size | 3 frames |

---

## Changelog

### v1.0.0 (2025-01-06)

- Initial API specification
- 27 MCP tools documented
- HTTP snapshot and streaming endpoints
- Full error reference
