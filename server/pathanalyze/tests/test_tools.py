"""Tests for tool wrappers."""

import pytest
from pathanalyze.mcp.client import MCPClient
from pathanalyze.mcp.tools import MCPTools
from pathanalyze.mcp.exceptions import MCPNotImplementedError, MCPToolError
from pathanalyze.mcp.types import SlideInfo, ViewportInfo, PolygonInfo


@pytest.mark.unit
class TestPlaceholderTools:
    """Test placeholder tool methods."""

    @pytest.mark.asyncio
    async def test_agent_hello_not_implemented(self, mock_client):
        """Test agent_hello placeholder."""
        tools = MCPTools(mock_client)

        with pytest.raises(MCPNotImplementedError) as exc_info:
            await tools.agent_hello("test-agent", ["navigation"])

        assert "agent.hello" in str(exc_info.value)

    @pytest.mark.asyncio
    async def test_lock_navigation_not_implemented(self, mock_client):
        """Test lock_navigation placeholder."""
        tools = MCPTools(mock_client)

        with pytest.raises(MCPNotImplementedError):
            await tools.lock_navigation("test-agent")

    @pytest.mark.asyncio
    async def test_unlock_navigation_not_implemented(self, mock_client):
        """Test unlock_navigation placeholder."""
        tools = MCPTools(mock_client)

        with pytest.raises(MCPNotImplementedError):
            await tools.unlock_navigation("token")

    @pytest.mark.asyncio
    async def test_move_camera_not_implemented(self, mock_client):
        """Test move_camera placeholder."""
        tools = MCPTools(mock_client)

        with pytest.raises(MCPNotImplementedError):
            await tools.move_camera(1000, 1000, 1.0)

    @pytest.mark.asyncio
    async def test_await_move_not_implemented(self, mock_client):
        """Test await_move placeholder."""
        tools = MCPTools(mock_client)

        with pytest.raises(MCPNotImplementedError):
            await tools.await_move("token")

    @pytest.mark.asyncio
    async def test_create_roi_not_implemented(self, mock_client):
        """Test create_roi placeholder."""
        tools = MCPTools(mock_client)

        with pytest.raises(MCPNotImplementedError):
            await tools.create_roi([(0, 0), (100, 0), (100, 100), (0, 100)])

    @pytest.mark.asyncio
    async def test_roi_metrics_not_implemented(self, mock_client):
        """Test roi_metrics placeholder."""
        tools = MCPTools(mock_client)

        with pytest.raises(MCPNotImplementedError):
            await tools.roi_metrics("roi-id")

    @pytest.mark.asyncio
    async def test_action_card_not_implemented(self, mock_client):
        """Test action card placeholders."""
        tools = MCPTools(mock_client)

        with pytest.raises(MCPNotImplementedError):
            await tools.action_card_create("Test Card")

        with pytest.raises(MCPNotImplementedError):
            await tools.action_card_append("card-id", "message")

        with pytest.raises(MCPNotImplementedError):
            await tools.action_card_update("card-id", status="completed")


@pytest.mark.integration
class TestExistingTools:
    """Test wrappers for existing tools (requires live server)."""

    @pytest.mark.asyncio
    async def test_load_slide(self, mcp_client, slide_path):
        """Test load_slide wrapper."""
        tools = MCPTools(mcp_client)

        info = await tools.load_slide(slide_path)

        assert isinstance(info, SlideInfo)
        assert info.width > 0
        assert info.height > 0
        assert info.levels > 0
        assert info.path == slide_path

    @pytest.mark.asyncio
    async def test_get_slide_info(self, mcp_client, slide_path):
        """Test get_slide_info wrapper."""
        tools = MCPTools(mcp_client)

        # Load first
        await tools.load_slide(slide_path)

        # Get info
        info = await tools.get_slide_info()

        assert isinstance(info, SlideInfo)
        assert info.viewport is not None

    @pytest.mark.asyncio
    async def test_viewport_operations(self, mcp_client, slide_path):
        """Test viewport control tools."""
        tools = MCPTools(mcp_client)

        # Load slide first
        await tools.load_slide(slide_path)

        # Reset view
        viewport = await tools.reset_view()
        assert isinstance(viewport, ViewportInfo)
        assert viewport.zoom > 0

        # Zoom in
        viewport = await tools.zoom(1.5)
        assert viewport.zoom > 0

        # Pan
        viewport = await tools.pan(100, 100)
        assert isinstance(viewport.position.x, float)
        assert isinstance(viewport.position.y, float)

        # Center on point
        viewport = await tools.center_on(1000, 1000)
        assert isinstance(viewport, ViewportInfo)

        # Zoom at point
        viewport = await tools.zoom_at_point(500, 500, 1.2)
        assert isinstance(viewport, ViewportInfo)

    @pytest.mark.asyncio
    async def test_polygon_operations(self, mcp_client, slide_path, polygon_path):
        """Test polygon overlay tools."""
        tools = MCPTools(mcp_client)

        # Load slide first
        await tools.load_slide(slide_path)

        # Load polygons
        poly_info = await tools.load_polygons(polygon_path)
        assert isinstance(poly_info, PolygonInfo)
        assert poly_info.count > 0
        assert len(poly_info.classes) > 0

        # Query polygons in region
        result = await tools.query_polygons(0, 0, 1000, 1000)
        assert "polygons" in result or isinstance(result, dict)

        # Hide/show visibility
        result = await tools.set_polygon_visibility(False)
        assert isinstance(result, dict)

        result = await tools.set_polygon_visibility(True)
        assert isinstance(result, dict)

    @pytest.mark.asyncio
    async def test_capture_snapshot_not_implemented_yet(self, mcp_client, slide_path):
        """Test that capture_snapshot raises expected error (not yet implemented in server)."""
        tools = MCPTools(mcp_client)

        # Load slide first
        await tools.load_slide(slide_path)

        # This should fail because capture_snapshot is not implemented in PathView MCP server
        with pytest.raises((MCPToolError, Exception)) as exc_info:
            await tools.capture_snapshot(800, 600)

        # Should mention "not yet implemented" or similar
        error_msg = str(exc_info.value).lower()
        assert "not" in error_msg and ("implement" in error_msg or "available" in error_msg)
