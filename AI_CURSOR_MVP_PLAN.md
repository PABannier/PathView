# AI Cursor MVP — Agent Operates the Viewer

## Current Footing
- GUI (`src/core/Application.*`) owns SDL/ImGui loop, navigation (`Viewport` with easing), polygon overlay, annotations, and IPC server for JSON-RPC over Unix sockets.
- MCP server (`src/api/mcp`) wraps an IPC client, exposes HTTP+SSE tools (`pan`, `zoom`, `center_on`, etc.), and spins a small HTTP server (`src/api/http`) with `/snapshot/{id}` but no producer; `capture_snapshot` tool is unimplemented.
- No navigation lock or concept of agent ownership; user input is always honored. Animation completion is local only (no status surfaced to MCP clients).
- Action cards/agent progress UI do not exist. ROI polygons can be drawn manually via `AnnotationManager`, which can compute per-class cell counts from loaded polygons.

## MVP Goal
Let an external “AI Cursor” service connect over HTTP MCP, take exclusive control of navigation, move the camera smoothly with completion signals, draw ROIs, fetch metrics, and stream live “Action Cards” (with collapsible reasoning) back into the viewer UI while consuming GUI screenshots from a streaming HTTP route.

## Plan
1) **Transport & Session Plumbing**
   - Parameterize MCP server host/port from CLI args (currently hard-coded to 9000 in `MCPServer`) and ensure `/sse` + HTTP snapshot/stream endpoints share the chosen port for the AI Cursor.
   - Add lightweight session/heartbeat tool (e.g., `agent_hello`) to confirm agent identity and return current stream URLs and lock status.

2) **Navigation Lock Lifecycle**
   - Add lock state to `Application` (owner id, TTL, timestamp) and gate mouse/keyboard nav handlers in `ProcessEvents`; show an on-screen indicator + “force release” failsafe for humans.
   - Expose IPC commands `nav.lock` / `nav.unlock` (and a status query) that return whether the lock was acquired, previous owner, and expiry. Surface as MCP tools with explicit owner metadata.
   - Auto-release lock if IPC client disconnects or TTL elapses to prevent deadlock.

3) **Smooth `move_camera` with Completion**
   - Add IPC command `viewport.move` that takes target center/zoom + optional duration; uses `Viewport` animation system and returns a token.
   - Track in-flight animation + token; add IPC/MCP method `viewport.await` (or event push) that signals completion/abort, including final position/zoom. Reuse this for `pan`/`zoom` tools when `mode=SMOOTH`.
   - Update tests (`test_mcp_client.py`) to sample intermediate positions and wait for completion instead of blind sleeps.

4) **Screenshot Streaming Route**
   - Implement `capture_snapshot` IPC path in `Application` (e.g., `SDL_RenderReadPixels` → PNG) and wire MCP `capture_snapshot` to store frames in `SnapshotManager`.
   - Extend HTTP server with a streaming route (MJPEG or SSE with base64 PNGs) that publishes the latest snapshot on a cadence or on-demand; document URL in the session handshake.
   - Add lightweight cache/cleanup so streaming doesn’t exhaust memory; ensure capture runs off the render thread to avoid frame hitches.

5) **ROI Creation & Metrics via MCP**
   - Add IPC command `annotations.create` that accepts slide-space vertices, creates an `AnnotationPolygon`, triggers `ComputeCellCounts` using `PolygonOverlay`, and returns counts/bbox/id.
   - Add IPC/MCP helpers to list/delete ROIs and to query cell metrics for an arbitrary polygon without persisting (for quick probes).

6) **Action Cards Panel + Event Ingestion**
   - Define an `ActionCard` model (id, title, status, short summary, optional reasoning blob, timestamps, log of steps). Store in `Application`.
   - Create a new sidebar tab/panel to render cards, with collapsible reasoning and incremental updates (append log entries). Show linkage to lock owner.
   - Expose IPC/MCP commands to add/update cards and append log items; include a way for the AI Cursor to stream progress back to the GUI (e.g., `action_card.append`).

7) **AI Cursor Flow Wiring**
   - Provide a minimal tool bundle for the service: `agent_hello`, `nav.lock/unlock`, `move_camera`/`await`, `capture_snapshot`/stream URL, `annotations.create`, `roi_metrics`, `action_card.*`.
   - Document the happy path: handshake → lock → start action card → stream snapshots → move camera (await completion) → draw ROI + metrics → update card → unlock.
   - Add guardrails: reject navigation tools when lock not held; return clear errors on missing slide/polygons.

8) **Testing & Docs**
   - Unit tests for nav lock state machine and animation completion tokens; integration test updates to `test_mcp_client.py` for new tools and stream URL presence.
   - Update README with AI Cursor usage (ports, stream URL, lock semantics) and add a quickstart snippet for connecting the service.
