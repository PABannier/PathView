"""Integration tests for complete LangGraph workflow."""

import pytest
import pytest_asyncio

from pathanalyze.graph import (
    AnalysisState,
    create_analysis_graph,
    run_graph_with_cleanup,
)
from pathanalyze.mcp.tools import MCPTools


pytestmark = pytest.mark.integration


@pytest_asyncio.fixture
async def mcp_tools(mcp_client):
    """Fixture providing MCPTools instance with live MCP client."""
    return MCPTools(mcp_client)


# ============================================================================
# Full Graph Execution Tests
# ============================================================================


@pytest.mark.asyncio
async def test_full_graph_execution_with_mock(mock_client):
    """Test complete graph execution with mock MCP server."""

    from tests.mocks.mock_mcp_server import MockMCPServer

    # Start mock server
    async with MockMCPServer(port=9998) as mock_server:
        # Setup mock responses for complete workflow
        mock_server.add_tool_response("load_slide", {
            "path": "/test/slide.svs",
            "width": 10000,
            "height": 8000,
            "levels": 3,
            "vendor": "aperio",
            "viewport": {
                "position": {"x": 0, "y": 0},
                "zoom": 1.0,
                "screenWidth": 1920,
                "screenHeight": 1080
            }
        })

        mock_server.add_tool_response("reset_view", {
            "viewport": {
                "position": {"x": 0, "y": 0},
                "zoom": 0.1,
                "screenWidth": 1920,
                "screenHeight": 1080
            }
        })

        mock_server.add_tool_response("move_camera", {"token": "move-123"})
        mock_server.add_tool_response("await_move", {"completed": True})

        mock_server.add_tool_response("get_slide_info", {
            "path": "/test/slide.svs",
            "width": 10000,
            "height": 8000,
            "levels": 3,
            "vendor": "aperio",
            "viewport": {
                "position": {"x": 5000, "y": 4000},
                "zoom": 2.0,
                "screenWidth": 1920,
                "screenHeight": 1080
            }
        })

        mock_server.add_tool_response("create_annotation", {
            "id": 42,
            "name": "ROI-test"
        })

        mock_server.add_tool_response("compute_roi_metrics", {
            "area": 1000000.0,
            "cell_counts": {
                "Lymphocyte": 150,
                "Tumor": 89,
                "Stroma": 45
            }
        })

        # Create MCP client and tools
        from pathanalyze.mcp.client import MCPClient

        client = MCPClient("http://127.0.0.1:9998")
        await client.connect()
        await client.initialize()
        mcp = MCPTools(client)

        try:
            # Create initial state
            initial_state = AnalysisState(
                run_id="test-integration-001",
                slide_path="/test/slide.svs",
                task="Find tumor regions",
                mcp_base_url="http://127.0.0.1:9998"
            )

            # Create and run graph
            graph = create_analysis_graph(mcp)
            final_state = await run_graph_with_cleanup(graph, initial_state, mcp)

            # Verify final state
            assert final_state.status == "completed"
            assert final_state.slide_info is not None
            assert final_state.slide_info.width == 10000
            assert len(final_state.roi_ids) == 1
            assert final_state.roi_ids[0] == 42
            assert len(final_state.roi_metrics) == 1
            assert final_state.roi_metrics[0]["cell_counts"]["Lymphocyte"] == 150
            assert final_state.summary is not None
            assert "Analysis complete" in final_state.summary
            assert not final_state.lock_acquired  # Lock should be released
            assert len(final_state.steps_log) == 8  # All 8 nodes executed

            # Verify steps executed in order
            steps = [step["step"] for step in final_state.steps_log]
            expected_steps = [
                "connect",
                "acquire_lock",
                "baseline_view",
                "survey",
                "roi_plan",
                "draw_roi",
                "summarize",
                "release"
            ]
            assert steps == expected_steps

        finally:
            await client.close()


@pytest.mark.asyncio
@pytest.mark.slow
async def test_full_graph_execution_live(mcp_client, slide_path):
    """
    Test complete graph execution with live MCP server.

    This test requires:
    - PathView MCP server running on http://127.0.0.1:9000
    - TEST_SLIDE_PATH environment variable set
    """

    mcp = MCPTools(mcp_client)

    initial_state = AnalysisState(
        run_id="test-integration-live-001",
        slide_path=slide_path,
        task="Automated tissue analysis",
        mcp_base_url="http://127.0.0.1:9000"
    )

    # Create and run graph
    graph = create_analysis_graph(mcp)
    final_state = await run_graph_with_cleanup(graph, initial_state, mcp)

    # Verify completion
    assert final_state.status == "completed"
    assert final_state.slide_info is not None
    assert len(final_state.roi_ids) > 0
    assert final_state.summary is not None
    assert not final_state.lock_acquired  # Lock should be released
    assert len(final_state.steps_log) == 8

    # Verify ROI metrics were computed
    assert len(final_state.roi_metrics) > 0
    assert "area" in final_state.roi_metrics[0]


@pytest.mark.asyncio
async def test_graph_with_roi_hint(mock_client):
    """Test graph execution with user-provided ROI hint."""

    from tests.mocks.mock_mcp_server import MockMCPServer

    async with MockMCPServer(port=9997) as mock_server:
        # Setup minimal mock responses
        mock_server.add_tool_response("load_slide", {
            "path": "/test/slide.svs",
            "width": 10000,
            "height": 8000,
            "levels": 3,
            "vendor": "aperio",
            "viewport": {
                "position": {"x": 0, "y": 0},
                "zoom": 1.0,
                "screenWidth": 1920,
                "screenHeight": 1080
            }
        })

        mock_server.add_tool_response("reset_view", {
            "viewport": {
                "position": {"x": 0, "y": 0},
                "zoom": 0.1,
                "screenWidth": 1920,
                "screenHeight": 1080
            }
        })

        mock_server.add_tool_response("move_camera", {"token": "move-123"})
        mock_server.add_tool_response("await_move", {"completed": True})
        mock_server.add_tool_response("get_slide_info", {
            "path": "/test/slide.svs",
            "width": 10000,
            "height": 8000,
            "levels": 3,
            "vendor": "aperio",
            "viewport": {
                "position": {"x": 5000, "y": 4000},
                "zoom": 2.0,
                "screenWidth": 1920,
                "screenHeight": 1080
            }
        })

        mock_server.add_tool_response("create_annotation", {
            "id": 100,
            "name": "ROI-custom"
        })

        mock_server.add_tool_response("compute_roi_metrics", {
            "area": 250000.0,
            "cell_counts": {"Lymphocyte": 50}
        })

        from pathanalyze.mcp.client import MCPClient

        client = MCPClient("http://127.0.0.1:9997")
        await client.connect()
        await client.initialize()
        mcp = MCPTools(client)

        try:
            # Create state with ROI hint
            initial_state = AnalysisState(
                run_id="test-roi-hint-001",
                slide_path="/test/slide.svs",
                task="Analyze specific region",
                roi_hint={
                    "center": [3000, 2500],
                    "size": 500
                },
                mcp_base_url="http://127.0.0.1:9997"
            )

            graph = create_analysis_graph(mcp)
            final_state = await run_graph_with_cleanup(graph, initial_state, mcp)

            assert final_state.status == "completed"

            # Verify ROI was planned using hint
            roi_plan_step = next(s for s in final_state.steps_log if s["step"] == "roi_plan")
            assert roi_plan_step["result"]["center"] == [3000, 2500]
            assert roi_plan_step["result"]["size"] == 500

        finally:
            await client.close()


# ============================================================================
# Error Handling and Cleanup Tests
# ============================================================================


@pytest.mark.asyncio
async def test_graph_cleanup_on_connect_failure(mock_client):
    """Verify graph handles connection failure gracefully."""

    from tests.mocks.mock_mcp_server import MockMCPServer

    async with MockMCPServer(port=9996) as mock_server:
        # Mock load_slide to fail
        mock_server.add_tool_error("load_slide", -32603, "Slide file not found")

        from pathanalyze.mcp.client import MCPClient

        client = MCPClient("http://127.0.0.1:9996")
        await client.connect()
        await client.initialize()
        mcp = MCPTools(client)

        try:
            initial_state = AnalysisState(
                run_id="test-error-001",
                slide_path="/nonexistent/slide.svs",
                task="Test error handling",
                mcp_base_url="http://127.0.0.1:9996"
            )

            graph = create_analysis_graph(mcp)
            final_state = await run_graph_with_cleanup(graph, initial_state, mcp)

            # Should fail but not crash
            assert final_state.status == "failed"
            assert "Slide file not found" in final_state.error_message or "Connect failed" in final_state.error_message
            assert not final_state.lock_acquired  # Lock should not be held

        finally:
            await client.close()


@pytest.mark.asyncio
async def test_graph_cleanup_on_roi_failure(mock_client):
    """Verify lock is released even if ROI creation fails."""

    from tests.mocks.mock_mcp_server import MockMCPServer

    async with MockMCPServer(port=9995) as mock_server:
        # Setup responses up to ROI creation
        mock_server.add_tool_response("load_slide", {
            "path": "/test/slide.svs",
            "width": 10000,
            "height": 8000,
            "levels": 3,
            "vendor": "aperio",
            "viewport": {
                "position": {"x": 0, "y": 0},
                "zoom": 1.0,
                "screenWidth": 1920,
                "screenHeight": 1080
            }
        })

        mock_server.add_tool_response("reset_view", {
            "viewport": {
                "position": {"x": 0, "y": 0},
                "zoom": 0.1,
                "screenWidth": 1920,
                "screenHeight": 1080
            }
        })

        mock_server.add_tool_response("move_camera", {"token": "move-123"})
        mock_server.add_tool_response("await_move", {"completed": True})
        mock_server.add_tool_response("get_slide_info", {
            "path": "/test/slide.svs",
            "width": 10000,
            "height": 8000,
            "levels": 3,
            "vendor": "aperio",
            "viewport": {
                "position": {"x": 5000, "y": 4000},
                "zoom": 2.0,
                "screenWidth": 1920,
                "screenHeight": 1080
            }
        })

        # ROI creation fails
        mock_server.add_tool_error("create_annotation", -32603, "Failed to create ROI")

        from pathanalyze.mcp.client import MCPClient

        client = MCPClient("http://127.0.0.1:9995")
        await client.connect()
        await client.initialize()
        mcp = MCPTools(client)

        try:
            initial_state = AnalysisState(
                run_id="test-roi-error-001",
                slide_path="/test/slide.svs",
                task="Test ROI error handling",
                mcp_base_url="http://127.0.0.1:9995"
            )

            graph = create_analysis_graph(mcp)
            final_state = await run_graph_with_cleanup(graph, initial_state, mcp)

            # Should fail but cleanup should happen
            assert final_state.status == "failed"
            assert "ROI creation failed" in final_state.error_message or "Failed to create ROI" in final_state.error_message
            assert not final_state.lock_acquired  # Lock MUST be released

            # Verify some steps completed before failure
            assert len(final_state.steps_log) >= 5  # At least up to roi_plan

        finally:
            await client.close()


@pytest.mark.asyncio
async def test_graph_state_persistence(mock_client):
    """Test that graph state is properly persisted through checkpointer."""

    from tests.mocks.mock_mcp_server import MockMCPServer

    async with MockMCPServer(port=9994) as mock_server:
        # Setup complete mock responses
        mock_server.add_tool_response("load_slide", {
            "path": "/test/slide.svs",
            "width": 10000,
            "height": 8000,
            "levels": 3,
            "vendor": "aperio",
            "viewport": {
                "position": {"x": 0, "y": 0},
                "zoom": 1.0,
                "screenWidth": 1920,
                "screenHeight": 1080
            }
        })

        mock_server.add_tool_response("reset_view", {
            "viewport": {
                "position": {"x": 0, "y": 0},
                "zoom": 0.1,
                "screenWidth": 1920,
                "screenHeight": 1080
            }
        })

        mock_server.add_tool_response("move_camera", {"token": "move-123"})
        mock_server.add_tool_response("await_move", {"completed": True})
        mock_server.add_tool_response("get_slide_info", {
            "path": "/test/slide.svs",
            "width": 10000,
            "height": 8000,
            "levels": 3,
            "vendor": "aperio",
            "viewport": {
                "position": {"x": 5000, "y": 4000},
                "zoom": 2.0,
                "screenWidth": 1920,
                "screenHeight": 1080
            }
        })

        mock_server.add_tool_response("create_annotation", {"id": 42, "name": "ROI"})
        mock_server.add_tool_response("compute_roi_metrics", {
            "area": 1000000.0,
            "cell_counts": {"Lymphocyte": 150}
        })

        from pathanalyze.mcp.client import MCPClient

        client = MCPClient("http://127.0.0.1:9994")
        await client.connect()
        await client.initialize()
        mcp = MCPTools(client)

        try:
            initial_state = AnalysisState(
                run_id="test-persistence-001",
                slide_path="/test/slide.svs",
                task="Test state persistence",
                mcp_base_url="http://127.0.0.1:9994"
            )

            graph = create_analysis_graph(mcp)
            final_state = await run_graph_with_cleanup(graph, initial_state, mcp)

            # Verify state accumulated correctly throughout execution
            assert final_state.status == "completed"
            assert final_state.slide_info is not None  # From connect
            assert final_state.lock_token is not None  # From acquire_lock
            assert final_state.baseline_snapshot_url is None  # Snapshot failed (not mocked)
            assert len(final_state.planned_roi_vertices) == 4  # From roi_plan
            assert len(final_state.roi_ids) == 1  # From draw_roi
            assert len(final_state.roi_metrics) == 1  # From draw_roi
            assert final_state.summary is not None  # From summarize
            assert not final_state.lock_acquired  # From release

        finally:
            await client.close()
