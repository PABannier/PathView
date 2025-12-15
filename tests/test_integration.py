"""End-to-end integration tests."""

import pytest
from pathanalyze.mcp.client import MCPClient
from pathanalyze.mcp.tools import MCPTools
from pathanalyze.config import settings


@pytest.mark.integration
@pytest.mark.slow
class TestEndToEnd:
    """Full workflow integration tests."""

    @pytest.mark.asyncio
    async def test_complete_workflow(self, slide_path):
        """Test complete workflow: connect, load, navigate, query."""
        async with MCPClient(str(settings.mcp_base_url)) as client:
            await client.initialize()
            tools = MCPTools(client)

            # Load slide
            info = await tools.load_slide(slide_path)
            assert info.width > 0
            assert info.height > 0

            # Reset view
            viewport = await tools.reset_view()
            assert viewport.zoom > 0

            # Navigate to center
            center_x = info.width / 2
            center_y = info.height / 2
            viewport = await tools.center_on(center_x, center_y)
            assert isinstance(viewport.position.x, float)

            # Zoom in
            viewport = await tools.zoom(2.0)
            assert viewport.zoom > 0

            # Pan around
            viewport = await tools.pan(100, 100)
            assert isinstance(viewport.position.x, float)

            # Get current info
            current_info = await tools.get_slide_info()
            assert current_info.viewport is not None
            assert current_info.path == slide_path

    @pytest.mark.asyncio
    async def test_polygon_workflow(self, slide_path, polygon_path):
        """Test polygon loading and querying."""
        async with MCPClient(str(settings.mcp_base_url)) as client:
            await client.initialize()
            tools = MCPTools(client)

            # Load slide
            slide_info = await tools.load_slide(slide_path)
            assert slide_info.width > 0

            # Load polygons
            poly_info = await tools.load_polygons(polygon_path)
            assert poly_info.count > 0
            assert len(poly_info.classes) > 0

            # Show polygons
            result = await tools.set_polygon_visibility(True)
            assert isinstance(result, dict)

            # Query region at center
            center_x = slide_info.width / 2
            center_y = slide_info.height / 2
            result = await tools.query_polygons(
                center_x - 500,
                center_y - 500,
                1000,
                1000
            )
            assert isinstance(result, dict)

            # Hide polygons
            result = await tools.set_polygon_visibility(False)
            assert isinstance(result, dict)

    @pytest.mark.asyncio
    async def test_multiple_zoom_levels(self, slide_path):
        """Test navigation across multiple zoom levels."""
        async with MCPClient(str(settings.mcp_base_url)) as client:
            await client.initialize()
            tools = MCPTools(client)

            # Load slide
            await tools.load_slide(slide_path)

            # Reset to base zoom
            viewport = await tools.reset_view()
            base_zoom = viewport.zoom

            # Zoom in progressively
            for _ in range(3):
                viewport = await tools.zoom(1.5)
                assert viewport.zoom > base_zoom

            # Zoom out back to base
            while viewport.zoom > base_zoom * 1.1:
                viewport = await tools.zoom(0.8)

            # Verify we're back near base zoom
            assert abs(viewport.zoom - base_zoom) < base_zoom * 0.5

    @pytest.mark.asyncio
    async def test_error_handling_no_slide(self):
        """Test error handling when no slide is loaded."""
        async with MCPClient(str(settings.mcp_base_url)) as client:
            await client.initialize()
            tools = MCPTools(client)

            # Try to get info without loading slide
            with pytest.raises(Exception):  # Should raise some error
                await tools.get_slide_info()

    @pytest.mark.asyncio
    async def test_load_invalid_slide_path(self):
        """Test error handling with invalid slide path."""
        async with MCPClient(str(settings.mcp_base_url)) as client:
            await client.initialize()
            tools = MCPTools(client)

            # Try to load non-existent slide
            with pytest.raises(Exception):  # Should raise some error
                await tools.load_slide("/nonexistent/path/slide.svs")
