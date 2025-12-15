# PathAnalyze MVP — LangGraph Cursor Service

## Goal
Ship a Python (LangChain/LangGraph) “PathAnalyze” agent that, upon a client request, handshakes with the PathView MCP server, locks navigation, drives smooth camera moves, draws ROIs, fetches metrics, and streams Action Cards back to the viewer. Focus on a happy-path single-agent session.

## Architecture Snapshot
- **Entrypoints**: FastAPI (or Typer CLI) to accept “start analysis” requests from the GUI; background task spins a LangGraph run.
- **MCP client**: HTTP+SSE transport to PathView MCP (`/sse` + `/mcp`), wrapping tools: `agent_hello`, `nav.lock/unlock`, `move_camera/await`, `capture_snapshot` or stream URL, `annotations.create`, `roi_metrics`, `action_card.*`.
- **LangGraph graph**: Linear chain (ingest request → connect → lock → inspect → navigate → annotate → summarize → unlock) with error guardrails.
- **State**: Graph state holds MCP session handles, lock token, latest viewport info, snapshot URLs, Action Card ids, ROI ids, metrics cache.
- **Observability**: Structured logs + optional OpenAI trace IDs; timeout/heartbeat to cleanly unlock on failure.

## Plan
1) **Project Skeleton**
   - Create `pathanalyze/` with `__init__.py`, `config.py` (env for MCP base URL, auth token), `main.py` (FastAPI or Typer), `graph.py` (LangGraph definition), `mcp_client.py` (thin wrapper over requests/SSE).
   - Pin deps: `langchain`, `langgraph`, `httpx`, `sseclient-py` (or `aiohttp-sse`), `pydantic`, `fastapi[standard]` (if HTTP entrypoint), `uvicorn`.

2) **MCP Client Wrapper**
   - Implement handshake (`initialize` → wait message endpoint → `initialized`), tool call helper, SSE listener (async task) with reconnect/backoff.
   - Add convenience methods matching viewer MVP: `hello()`, `lock(owner, ttl)`, `unlock()`, `move_camera(center, zoom, duration)` returning token, `await_move(token)`, `create_roi(vertices)`, `roi_metrics(poly)`, `capture_snapshot(width/height)` or subscribe to stream URL, `action_card_{create,append,update}`.
   - Propagate structured errors and include retryable flags.

3) **LangGraph Definition**
   - State: `request` (slide id/goal), `mcp` (client instance), `lock_token`, `card_id`, `roi_ids`, `metrics`, `steps_log`.
   - Nodes:
     - `connect`: handshake, agent_hello, fetch stream URLs.
     - `acquire_lock`: call `lock`, record owner/ttl; if fail → short retry; hard-fail after N attempts.
     - `baseline_view`: move/await to fit/reset; capture snapshot; update Action Card start.
     - `survey`: optional coarse pan/zoom to collect screenshots; update steps.
     - `roi_plan`: select ROI target (placeholder heuristic or static for MVP).
     - `draw_roi`: create polygon via `annotations.create`; store id; fetch metrics.
     - `summarize`: craft short report; append reasoning to Action Card; set status done.
     - `release`: unlock, finalize Action Card status.
   - Edges: simple linear with error edges to `release` + error note.

4) **Action Card Streaming**
   - On each node, append `action_card.append` updates (status text + optional reasoning). Mark final status (`completed`/`failed`) in `summarize`/`release`.
   - Include URLs to snapshots (from capture or stream) when available.

5) **Snapshot Intake**
   - MVP: call `capture_snapshot` tool after key moves; store returned URL/id; attach to Action Card updates.
   - If stream URL available, subscribe in a background task and keep latest frame ref (no heavy buffering).

6) **GUI Trigger Surface**
   - FastAPI route `POST /analyze` with payload `{slide_path, task, roi_hint?}` spawns LangGraph run in background; respond with run id.
   - Health route and optional `/runs/{id}` status for debugging.

7) **Failure Handling**
   - Wrap graph in try/finally to always attempt `unlock`.
   - Timebox moves/awaits; on timeout, append error to Action Card and bail out.
   - Guard against missing slide/polygons by checking tool responses early.

8) **Testing & Demo**
   - Mock MCP client for unit tests of graph transitions.
   - Happy-path integration test with a live MCP server: start server, connect, lock, move, capture snapshot, create ROI, metrics, summarize, unlock.
   - Add README snippet describing how to start PathAnalyze and invoke `POST /analyze`.
