"""Pytest fixtures for PathAnalyze tests."""

import os
import pytest
import pytest_asyncio

from pathanalyze.mcp.client import MCPClient
from pathanalyze.config import settings
from tests.mocks.mock_mcp_server import MockMCPServer


@pytest_asyncio.fixture
async def mock_server():
    """Fixture providing a mock MCP server for unit tests."""
    async with MockMCPServer(port=9999) as server:
        yield server


@pytest_asyncio.fixture
async def mock_client(mock_server):
    """Fixture providing an MCPClient connected to mock server."""
    client = MCPClient("http://127.0.0.1:9999")
    await client.connect()
    await client.initialize()
    yield client
    await client.close()


@pytest_asyncio.fixture
async def mcp_client():
    """Fixture providing a connected MCP client to live server (for integration tests)."""
    client = MCPClient(str(settings.mcp_base_url))
    await client.connect()
    await client.initialize()
    yield client
    await client.close()


@pytest.fixture
def slide_path():
    """Fixture providing test slide path from environment."""
    path = os.getenv("TEST_SLIDE_PATH")
    if not path:
        pytest.skip("TEST_SLIDE_PATH not set in environment")
    if not os.path.exists(path):
        pytest.skip(f"Test slide not found: {path}")
    return path


@pytest.fixture
def polygon_path():
    """Fixture providing test polygon path from environment."""
    path = os.getenv("TEST_POLYGON_PATH")
    if not path:
        pytest.skip("TEST_POLYGON_PATH not set in environment")
    if not os.path.exists(path):
        pytest.skip(f"Test polygon file not found: {path}")
    return path
