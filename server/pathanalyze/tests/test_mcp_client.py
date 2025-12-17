"""Unit and integration tests for MCPClient."""

import pytest
import asyncio
from pathanalyze.mcp.client import MCPClient
from pathanalyze.mcp.exceptions import (
    MCPConnectionError,
    MCPTimeoutError,
    MCPToolError,
)


@pytest.mark.unit
class TestMCPClientUnit:
    """Unit tests without live server (using mock server)."""

    def test_client_initialization(self):
        """Test client can be created."""
        client = MCPClient("http://127.0.0.1:9000")
        assert client.server_url == "http://127.0.0.1:9000"
        assert client.sse_endpoint == "/sse"
        assert not client.connected

    @pytest.mark.asyncio
    async def test_connect_and_initialize_with_mock(self, mock_server, mock_client):
        """Test successful connection and handshake with mock server."""
        assert mock_client.connected
        assert mock_client.message_endpoint == "/message"

    @pytest.mark.asyncio
    async def test_connection_timeout(self):
        """Test connection timeout when server is unreachable."""
        client = MCPClient("http://127.0.0.1:9998", timeout=0.5)
        with pytest.raises(MCPConnectionError):
            async with client:
                pass

    @pytest.mark.asyncio
    async def test_call_tool_with_mock(self, mock_server, mock_client):
        """Test calling a tool with mock server."""
        # Configure mock response
        mock_server.add_tool_response("test_tool", {"result": "success"})

        # Call tool
        result = await mock_client.call_tool("test_tool", {"arg": "value"})

        # Verify result
        assert result == {"result": "success"}

        # Verify request was logged
        assert len(mock_server.requests) > 0

    @pytest.mark.asyncio
    async def test_tool_error_handling(self, mock_server, mock_client):
        """Test tool error handling."""
        # Configure mock to raise error
        def error_handler(args):
            raise ValueError("Tool failed")

        mock_server.add_tool_response("failing_tool", error_handler)

        # Call should raise MCPToolError
        with pytest.raises(MCPToolError) as exc_info:
            await mock_client.call_tool("failing_tool")

        assert "Tool failed" in str(exc_info.value)


@pytest.mark.integration
class TestMCPClientIntegration:
    """Integration tests requiring live MCP server."""

    @pytest.mark.asyncio
    async def test_connect_and_initialize(self, mcp_client):
        """Test successful connection and handshake."""
        assert mcp_client.connected
        assert mcp_client.message_endpoint is not None

    @pytest.mark.asyncio
    async def test_call_nonexistent_tool(self, mcp_client):
        """Test calling a tool that doesn't exist."""
        with pytest.raises(MCPToolError) as exc_info:
            await mcp_client.call_tool("nonexistent_tool")

        # Should get Method Not Found error
        assert exc_info.value.code == -32601

    @pytest.mark.asyncio
    async def test_timeout_handling(self, mcp_client):
        """Test request timeout behavior."""
        # Save original timeout
        original_timeout = mcp_client.timeout

        # Set very short timeout
        mcp_client.timeout = 0.001

        # This should timeout
        with pytest.raises(MCPTimeoutError):
            await mcp_client.call_tool("load_slide", {"path": "/nonexistent"})

        # Restore timeout
        mcp_client.timeout = original_timeout

    @pytest.mark.asyncio
    async def test_context_manager(self):
        """Test async context manager protocol."""
        async with MCPClient("http://127.0.0.1:9000") as client:
            await client.initialize()
            assert client.connected

        # Client should be closed after context
        assert not client.connected
