"""
Mock MCP server for unit testing.

Implements HTTP+SSE endpoints to simulate PathView MCP server behavior
without requiring a live server.
"""

import asyncio
import json
from typing import Any, Callable
from contextlib import asynccontextmanager

from aiohttp import web
import aiohttp


class MockMCPServer:
    """
    Mock MCP server for testing without live server.

    Usage:
        async with MockMCPServer() as server:
            client = MCPClient(f"http://127.0.0.1:{server.port}")
            await client.initialize()
    """

    def __init__(self, port: int = 9999):
        self.port = port
        self.app: web.Application | None = None
        self.runner: web.AppRunner | None = None
        self.site: web.TCPSite | None = None
        self.requests: list[dict] = []
        self.tool_responses: dict[str, Callable[[dict], Any]] = {}
        self.sse_clients: list[web.StreamResponse] = []

    async def __aenter__(self):
        """Start mock server."""
        await self.start()
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        """Stop mock server."""
        await self.stop()

    async def start(self):
        """Start the mock HTTP+SSE server."""
        self.app = web.Application()

        # Register routes
        self.app.router.add_get("/sse", self.handle_sse)
        self.app.router.add_post("/message", self.handle_message)

        # Start server
        self.runner = web.AppRunner(self.app)
        await self.runner.setup()
        self.site = web.TCPSite(self.runner, "127.0.0.1", self.port)
        await self.site.start()

    async def stop(self):
        """Stop the mock server."""
        # Close all SSE connections
        for response in self.sse_clients:
            await response.write_eof()

        if self.site:
            await self.site.stop()
        if self.runner:
            await self.runner.cleanup()

    async def handle_sse(self, request: web.Request) -> web.StreamResponse:
        """
        Handle SSE endpoint (/sse).

        Sends:
        1. 'endpoint' event with message endpoint URL
        2. Keeps connection open for 'message' events
        """
        response = web.StreamResponse()
        response.headers["Content-Type"] = "text/event-stream"
        response.headers["Cache-Control"] = "no-cache"
        response.headers["Connection"] = "keep-alive"

        await response.prepare(request)

        # Send endpoint event
        endpoint_event = f"event: endpoint\ndata: /message\n\n"
        await response.write(endpoint_event.encode())

        # Keep connection alive and track for later messages
        self.sse_clients.append(response)

        try:
            # Keep connection open until client disconnects
            while True:
                await asyncio.sleep(1)
        except Exception:
            pass
        finally:
            self.sse_clients.remove(response)

        return response

    async def handle_message(self, request: web.Request) -> web.Response:
        """
        Handle JSON-RPC messages (/message).

        Processes:
        - initialize requests
        - initialized notifications
        - tools/call requests
        """
        try:
            data = await request.json()
            self.requests.append(data)

            # Handle initialize request
            if data.get("method") == "initialize":
                response = {
                    "jsonrpc": "2.0",
                    "id": data["id"],
                    "result": {
                        "protocolVersion": "2024-11-05",
                        "capabilities": {},
                        "serverInfo": {
                            "name": "MockMCPServer",
                            "version": "0.1.0"
                        }
                    }
                }
                # Send via SSE
                await self.send_sse_message(response)
                return web.Response(status=200)

            # Handle initialized notification
            if data.get("method") == "notifications/initialized":
                return web.Response(status=200)

            # Handle tools/call
            if data.get("method") == "tools/call":
                params = data.get("params", {})
                tool_name = params.get("name")
                arguments = params.get("arguments", {})

                # Get tool response handler
                if tool_name in self.tool_responses:
                    handler = self.tool_responses[tool_name]
                    try:
                        result = handler(arguments)
                        response = {
                            "jsonrpc": "2.0",
                            "id": data["id"],
                            "result": {"isError": False, "content": result}
                        }
                    except Exception as e:
                        response = {
                            "jsonrpc": "2.0",
                            "id": data["id"],
                            "result": {"isError": True, "content": str(e)}
                        }
                else:
                    # Tool not found
                    response = {
                        "jsonrpc": "2.0",
                        "id": data["id"],
                        "error": {
                            "code": -32601,
                            "message": f"Method not found: {tool_name}"
                        }
                    }

                # Send via SSE
                await self.send_sse_message(response)
                return web.Response(status=200)

            # Unknown method
            response = {
                "jsonrpc": "2.0",
                "id": data.get("id"),
                "error": {
                    "code": -32601,
                    "message": f"Method not found: {data.get('method')}"
                }
            }
            await self.send_sse_message(response)
            return web.Response(status=200)

        except Exception as e:
            return web.Response(
                status=500,
                text=json.dumps({"error": str(e)}),
                content_type="application/json"
            )

    async def send_sse_message(self, message: dict):
        """Send a message event to all SSE clients."""
        event = f"event: message\ndata: {json.dumps(message)}\n\n"
        for response in self.sse_clients:
            try:
                await response.write(event.encode())
            except Exception:
                pass  # Client disconnected

    def add_tool_response(self, tool_name: str, response: Any | Callable):
        """
        Configure mock response for a tool.

        Args:
            tool_name: Name of the tool
            response: Response data or callable that takes arguments and returns response
        """
        if callable(response):
            self.tool_responses[tool_name] = response
        else:
            self.tool_responses[tool_name] = lambda args: response

    def reset(self):
        """Reset server state (clear requests and responses)."""
        self.requests.clear()
        self.tool_responses.clear()
