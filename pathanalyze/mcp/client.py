"""
Async MCP client for PathView server.

This client implements the MCP protocol over HTTP+SSE transport:
1. Connect to SSE endpoint (/sse)
2. Wait for 'endpoint' event with message endpoint URL
3. Send 'initialize' JSON-RPC request
4. Send 'initialized' notification
5. Call tools via 'tools/call' method
6. Receive responses via SSE 'message' events
"""

import asyncio
import json
from typing import Any
from urllib.parse import urljoin

import httpx
from httpx_sse import aconnect_sse

from pathanalyze.mcp.exceptions import (
    MCPException,
    MCPConnectionError,
    MCPTimeoutError,
    MCPToolError,
)
from pathanalyze.mcp.types import JSONRPCRequest
from pathanalyze.utils.logging import logger


class MCPClient:
    """
    Async MCP client for PathView MCP server.

    Usage:
        async with MCPClient("http://127.0.0.1:9000") as client:
            await client.initialize()
            result = await client.call_tool("load_slide", {"path": "/path/to/slide"})
    """

    def __init__(
        self,
        server_url: str,
        sse_endpoint: str = "/sse",
        timeout: float = 30.0,
        max_retries: int = 3,
        retry_delay: float = 1.0,
    ):
        self.server_url = server_url.rstrip("/")
        self.sse_endpoint = sse_endpoint
        self.timeout = timeout
        self.max_retries = max_retries
        self.retry_delay = retry_delay

        # Connection state
        self.http_client: httpx.AsyncClient | None = None
        self.message_endpoint: str | None = None
        self.request_id = 1
        self.pending_responses: dict[int, asyncio.Future] = {}
        self.sse_task: asyncio.Task | None = None
        self.connected = False
        self._lock = asyncio.Lock()

    async def __aenter__(self):
        """Async context manager entry."""
        await self.connect()
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        """Async context manager exit."""
        await self.close()

    async def connect(self):
        """Connect to MCP server and start SSE listener."""
        if self.connected:
            return

        logger.info("Connecting to MCP server", extra={"url": self.server_url})

        # Create HTTP client
        self.http_client = httpx.AsyncClient(timeout=self.timeout)

        # Start SSE listener
        self.sse_task = asyncio.create_task(self._sse_listener())

        # Wait for message endpoint (with timeout)
        try:
            await asyncio.wait_for(
                self._wait_for_endpoint(),
                timeout=self.timeout
            )
        except asyncio.TimeoutError:
            await self.close()
            raise MCPConnectionError("Timeout waiting for message endpoint")

        logger.info("Connected to MCP server", extra={"endpoint": self.message_endpoint})

    async def _wait_for_endpoint(self):
        """Wait for message endpoint to be set by SSE listener."""
        while self.message_endpoint is None:
            await asyncio.sleep(0.1)

    async def initialize(
        self,
        client_name: str = "PathAnalyze",
        client_version: str = "0.1.0"
    ) -> dict[str, Any]:
        """
        Perform MCP handshake: initialize → wait → initialized.

        Returns:
            Server initialization response
        """
        if not self.message_endpoint:
            raise MCPConnectionError("Not connected to server")

        logger.info("Initializing MCP session")

        # Send initialize request
        result = await self._send_jsonrpc("initialize", {
            "protocolVersion": "2024-11-05",
            "capabilities": {},
            "clientInfo": {
                "name": client_name,
                "version": client_version,
            }
        })

        # Send initialized notification (no response expected)
        await self._send_notification("initialized", {})

        self.connected = True
        logger.info("MCP session initialized")

        return result

    async def call_tool(
        self,
        tool_name: str,
        arguments: dict[str, Any] | None = None
    ) -> Any:
        """
        Call an MCP tool and return the result.

        Args:
            tool_name: Name of the tool to call
            arguments: Tool arguments (optional)

        Returns:
            Tool result (parsed from response)

        Raises:
            MCPToolError: If tool call fails
        """
        if not self.connected:
            raise MCPConnectionError("Not connected to server")

        logger.debug("Calling tool", extra={"tool": tool_name, "args": arguments})

        response = await self._send_jsonrpc("tools/call", {
            "name": tool_name,
            "arguments": arguments or {},
        })

        # Parse PathView MCP response format
        # Expected: {isError: bool, content: {...}}
        if isinstance(response, dict):
            is_error = response.get("isError", False)
            content = response.get("content")

            if is_error:
                error_msg = str(content) if content else "Unknown error"
                # Check if retryable (connection issues, timeouts)
                retryable = "timeout" in error_msg.lower() or "connection" in error_msg.lower()
                raise MCPToolError(-32603, error_msg, retryable=retryable)

            # Return content directly
            if isinstance(content, dict):
                return content

            # Handle array format (standard MCP)
            if isinstance(content, list) and len(content) > 0:
                text_content = content[0].get("text", "{}")
                return json.loads(text_content)

        return response

    async def close(self):
        """Close the MCP client connection."""
        logger.info("Closing MCP connection")

        # Cancel SSE listener
        if self.sse_task and not self.sse_task.done():
            self.sse_task.cancel()
            try:
                await self.sse_task
            except asyncio.CancelledError:
                pass

        # Close HTTP client
        if self.http_client:
            await self.http_client.aclose()

        # Reject pending requests
        for future in self.pending_responses.values():
            if not future.done():
                future.set_exception(MCPConnectionError("Connection closed"))

        self.connected = False
        self.pending_responses.clear()

    async def _sse_listener(self):
        """Background task to listen for SSE events."""
        sse_url = f"{self.server_url}{self.sse_endpoint}"
        logger.debug("Starting SSE listener", extra={"url": sse_url})

        try:
            async with aconnect_sse(
                self.http_client,
                "GET",
                sse_url
            ) as event_source:
                async for event in event_source.aiter_sse():
                    await self._handle_sse_event(event)

        except Exception as e:
            logger.error("SSE listener error", extra={"error": str(e)})
            self.message_endpoint = None

    async def _handle_sse_event(self, event):
        """Handle a single SSE event."""
        logger.debug("SSE event", extra={"type": event.event, "data": event.data})

        # Handle endpoint event (server sends message endpoint)
        if event.event == "endpoint":
            self.message_endpoint = event.data
            return

        # Handle message event (JSON-RPC response)
        if event.event == "message":
            try:
                data = json.loads(event.data)

                # Route to pending request
                if "id" in data and data["id"] is not None:
                    req_id = data["id"]
                    async with self._lock:
                        if req_id in self.pending_responses:
                            future = self.pending_responses.pop(req_id)

                            # Check for error
                            if "error" in data:
                                error = data["error"]
                                future.set_exception(
                                    MCPToolError(
                                        error.get("code", -32000),
                                        error.get("message", "Unknown error")
                                    )
                                )
                            else:
                                future.set_result(data.get("result"))

            except Exception as e:
                logger.error("Failed to parse SSE message", extra={"error": str(e)})

    async def _send_jsonrpc(
        self,
        method: str,
        params: dict[str, Any] | None = None
    ) -> Any:
        """
        Send a JSON-RPC 2.0 request and wait for response.

        Args:
            method: JSON-RPC method name
            params: Method parameters

        Returns:
            Response result

        Raises:
            MCPException: On error
        """
        if self.message_endpoint is None:
            raise MCPConnectionError("No message endpoint available")

        # Generate request ID and create future
        async with self._lock:
            req_id = self.request_id
            self.request_id += 1
            future: asyncio.Future = asyncio.Future()
            self.pending_responses[req_id] = future

        # Build request
        request = JSONRPCRequest(
            id=req_id,
            method=method,
            params=params,
        )

        logger.debug("Sending JSON-RPC request", extra={"id": req_id, "method": method})

        # Send request
        try:
            endpoint_url = urljoin(self.server_url, self.message_endpoint)
            response = await self.http_client.post(
                endpoint_url,
                json=request.model_dump(exclude_none=True),
            )
            response.raise_for_status()

        except httpx.HTTPError as e:
            async with self._lock:
                self.pending_responses.pop(req_id, None)
            raise MCPConnectionError(f"Request failed: {e}")

        # Wait for response via SSE (with timeout)
        try:
            result = await asyncio.wait_for(future, timeout=self.timeout)
            return result

        except asyncio.TimeoutError:
            async with self._lock:
                self.pending_responses.pop(req_id, None)
            raise MCPTimeoutError(f"Request {req_id} timeout")

    async def _send_notification(
        self,
        method: str,
        params: dict[str, Any] | None = None
    ):
        """
        Send a JSON-RPC 2.0 notification (no response expected).

        Args:
            method: Notification method name
            params: Method parameters
        """
        if self.message_endpoint is None:
            raise MCPConnectionError("No message endpoint available")

        # Build notification (no id field)
        notification = {
            "jsonrpc": "2.0",
            "method": f"notifications/{method}",
        }

        if params is not None:
            notification["params"] = params

        logger.debug("Sending notification", extra={"method": method})

        # Send notification
        try:
            endpoint_url = urljoin(self.server_url, self.message_endpoint)
            response = await self.http_client.post(
                endpoint_url,
                json=notification,
            )
            response.raise_for_status()

        except httpx.HTTPError as e:
            raise MCPConnectionError(f"Notification failed: {e}")
