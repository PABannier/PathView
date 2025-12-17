"""Unit tests for LangGraph node implementations."""

import pytest
import pytest_asyncio

from pathanalyze.graph import (
    AnalysisState,
    connect_node,
    acquire_lock_node,
    baseline_view_node,
    survey_node,
    roi_plan_node,
    draw_roi_node,
    summarize_node,
    release_node,
)
from pathanalyze.mcp.tools import MCPTools
from pathanalyze.mcp.exceptions import MCPNotImplementedError, MCPToolError


pytestmark = pytest.mark.unit


@pytest_asyncio.fixture
async def mcp_tools(mock_client):
    """Fixture providing MCPTools instance with mock client."""
    return MCPTools(mock_client)


@pytest_asyncio.fixture
def base_state():
    """Fixture providing base analysis state for tests."""
    return AnalysisState(
        run_id="test-123",
        slide_path="/test/slide.svs",
        task="Test analysis",
        mcp_base_url="http://127.0.0.1:9999"
    )


# ============================================================================
# connect_node tests
# ============================================================================


@pytest.mark.asyncio
async def test_connect_node_success(mcp_tools, base_state, mock_server):
    """Test connect node loads slide successfully."""

    # Mock load_slide response
    mock_server.add_tool_response("load_slide", {
        "path": "/test/slide.svs",
        "width": 10000,
        "height": 10000,
        "levels": 3,
        "vendor": "aperio",
        "viewport": {
            "position": {"x": 0, "y": 0},
            "zoom": 1.0,
            "screenWidth": 1920,
            "screenHeight": 1080
        }
    })

    result = await connect_node(base_state, mcp_tools)

    assert result.slide_info is not None
    assert result.slide_info.width == 10000
    assert result.slide_info.height == 10000
    assert result.current_viewport is not None
    assert result.current_step == "connect"
    assert result.status == "running"
    assert len(result.steps_log) == 1
    assert result.steps_log[0]["step"] == "connect"


@pytest.mark.asyncio
async def test_connect_node_error_handling(mcp_tools, base_state, mock_server):
    """Test connect node handles errors gracefully."""

    # Mock load_slide to fail
    mock_server.add_tool_error("load_slide", -32603, "Slide file not found")

    result = await connect_node(base_state, mcp_tools)

    assert result.status == "failed"
    assert "Connect failed" in result.error_message
    assert result.current_step == "connect"


# ============================================================================
# acquire_lock_node tests
# ============================================================================


@pytest.mark.asyncio
async def test_acquire_lock_node_real_lock(mcp_tools, base_state, mock_server):
    """Test lock acquisition with real implementation."""

    # Mock lock_navigation response
    mock_server.add_tool_response("nav.lock", {"token": "lock-token-123"})

    result = await acquire_lock_node(base_state, mcp_tools)

    assert result.lock_acquired is True
    assert result.lock_token == "lock-token-123"
    assert result.current_step == "acquire_lock"
    assert len(result.steps_log) == 1
    assert result.steps_log[0]["result"]["mocked"] is False


@pytest.mark.asyncio
async def test_acquire_lock_node_fallback_to_mock(mcp_tools, base_state, mock_server):
    """Test lock node falls back to mock when not implemented."""

    # Don't add response - tool will raise MCPNotImplementedError
    # (MockMCPServer raises MCPNotImplementedError for unknown tools)

    result = await acquire_lock_node(base_state, mcp_tools)

    # Should generate mock token without failing
    assert result.lock_acquired is True
    assert "mock-lock" in result.lock_token
    assert base_state.run_id in result.lock_token
    assert result.current_step == "acquire_lock"
    assert len(result.steps_log) == 1
    assert result.steps_log[0]["result"]["mocked"] is True


# ============================================================================
# baseline_view_node tests
# ============================================================================


@pytest.mark.asyncio
async def test_baseline_view_node_success(mcp_tools, base_state, mock_server):
    """Test baseline view reset and snapshot capture."""

    # Mock reset_view response
    mock_server.add_tool_response("reset_view", {
        "viewport": {
            "position": {"x": 0, "y": 0},
            "zoom": 0.1,
            "screenWidth": 1920,
            "screenHeight": 1080
        }
    })

    # Mock capture_snapshot response
    mock_server.add_tool_response("capture_snapshot", {
        "id": "snap-123",
        "url": "http://127.0.0.1:8080/snapshot/snap-123"
    })

    result = await baseline_view_node(base_state, mcp_tools)

    assert result.current_viewport is not None
    assert result.baseline_snapshot_url == "http://127.0.0.1:8080/snapshot/snap-123"
    assert result.current_step == "baseline_view"
    assert len(result.steps_log) == 1


@pytest.mark.asyncio
async def test_baseline_view_node_snapshot_fails(mcp_tools, base_state, mock_server):
    """Test baseline view continues if snapshot fails."""

    # Mock reset_view response
    mock_server.add_tool_response("reset_view", {
        "viewport": {
            "position": {"x": 0, "y": 0},
            "zoom": 0.1,
            "screenWidth": 1920,
            "screenHeight": 1080
        }
    })

    # Snapshot fails - don't mock it

    result = await baseline_view_node(base_state, mcp_tools)

    # Should continue without snapshot
    assert result.current_viewport is not None
    assert result.baseline_snapshot_url is None
    assert result.status != "failed"  # Should NOT fail
    assert result.current_step == "baseline_view"


# ============================================================================
# survey_node tests
# ============================================================================


@pytest.mark.asyncio
async def test_survey_node_success(mcp_tools, base_state, mock_server):
    """Test survey navigation to slide center."""

    # Set slide info in state
    base_state.slide_info = type('obj', (object,), {
        'width': 10000,
        'height': 8000,
        'levels': 3
    })()

    # Mock move_camera response
    mock_server.add_tool_response("move_camera", {"token": "move-123"})

    # Mock await_move response
    mock_server.add_tool_response("await_move", {"completed": True})

    # Mock get_slide_info response
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

    result = await survey_node(base_state, mcp_tools)

    assert result.current_viewport is not None
    assert result.current_step == "survey"
    assert len(result.steps_log) == 1
    assert result.steps_log[0]["result"]["center"] == [5000.0, 4000.0]
    assert result.steps_log[0]["result"]["zoom"] == 2.0


@pytest.mark.asyncio
async def test_survey_node_missing_slide_info(mcp_tools, base_state):
    """Test survey node fails gracefully without slide info."""

    result = await survey_node(base_state, mcp_tools)

    assert result.status == "failed"
    assert "Slide info not available" in result.error_message


# ============================================================================
# roi_plan_node tests
# ============================================================================


@pytest.mark.asyncio
async def test_roi_plan_node_default_center(mcp_tools, base_state):
    """Test ROI planning uses slide center by default."""

    # Set slide info
    base_state.slide_info = type('obj', (object,), {
        'width': 10000,
        'height': 8000,
        'levels': 3
    })()

    result = await roi_plan_node(base_state, mcp_tools)

    assert len(result.planned_roi_vertices) == 4  # Rectangle has 4 vertices
    assert result.current_step == "roi_plan"

    # Check center is at slide center
    center_x = result.steps_log[0]["result"]["center"][0]
    center_y = result.steps_log[0]["result"]["center"][1]
    assert center_x == 5000.0
    assert center_y == 4000.0


@pytest.mark.asyncio
async def test_roi_plan_node_with_hint(mcp_tools, base_state):
    """Test ROI planning uses user hint when provided."""

    # Set slide info and ROI hint
    base_state.slide_info = type('obj', (object,), {
        'width': 10000,
        'height': 8000,
        'levels': 3
    })()
    base_state.roi_hint = {
        "center": [3000, 2000],
        "size": 500
    }

    result = await roi_plan_node(base_state, mcp_tools)

    assert len(result.planned_roi_vertices) == 4

    # Check center matches hint
    center_x = result.steps_log[0]["result"]["center"][0]
    center_y = result.steps_log[0]["result"]["center"][1]
    assert center_x == 3000
    assert center_y == 2000
    assert result.steps_log[0]["result"]["size"] == 500


# ============================================================================
# draw_roi_node tests
# ============================================================================


@pytest.mark.asyncio
async def test_draw_roi_node_success(mcp_tools, base_state, mock_server):
    """Test ROI creation and metrics computation."""

    # Set planned vertices
    base_state.planned_roi_vertices = [
        (4500, 3500),
        (5500, 3500),
        (5500, 4500),
        (4500, 4500)
    ]

    # Mock create_annotation response
    mock_server.add_tool_response("create_annotation", {
        "id": 42,
        "name": "ROI-test-123"
    })

    # Mock compute_roi_metrics response
    mock_server.add_tool_response("compute_roi_metrics", {
        "area": 1000000.0,
        "cell_counts": {
            "Lymphocyte": 150,
            "Tumor": 89,
            "Stroma": 45
        }
    })

    result = await draw_roi_node(base_state, mcp_tools)

    assert len(result.roi_ids) == 1
    assert result.roi_ids[0] == 42
    assert len(result.roi_metrics) == 1
    assert result.roi_metrics[0]["cell_counts"]["Lymphocyte"] == 150
    assert result.current_step == "draw_roi"


@pytest.mark.asyncio
async def test_draw_roi_node_missing_vertices(mcp_tools, base_state):
    """Test draw_roi fails without planned vertices."""

    result = await draw_roi_node(base_state, mcp_tools)

    assert result.status == "failed"
    assert "No planned ROI vertices" in result.error_message


# ============================================================================
# summarize_node tests
# ============================================================================


@pytest.mark.asyncio
async def test_summarize_node_success(mcp_tools, base_state):
    """Test summary generation with ROI metrics."""

    # Set up state with slide info and ROI data
    base_state.slide_info = type('obj', (object,), {
        'width': 10000,
        'height': 8000,
        'levels': 3
    })()
    base_state.roi_ids = [42]
    base_state.roi_metrics = [{
        "area": 1000000.0,
        "cell_counts": {
            "Lymphocyte": 150,
            "Tumor": 89
        }
    }]

    result = await summarize_node(base_state, mcp_tools)

    assert result.summary is not None
    assert "Analysis complete" in result.summary
    assert "10000x8000" in result.summary
    assert "ROI 42" in result.summary
    assert "Lymphocyte: 150" in result.summary
    assert result.status == "completed"
    assert result.current_step == "summarize"


@pytest.mark.asyncio
async def test_summarize_node_no_rois(mcp_tools, base_state):
    """Test summary generation without ROIs."""

    base_state.slide_info = type('obj', (object,), {
        'width': 10000,
        'height': 8000,
        'levels': 3
    })()

    result = await summarize_node(base_state, mcp_tools)

    assert result.summary is not None
    assert "ROIs created: 0" in result.summary
    assert result.status == "completed"


# ============================================================================
# release_node tests
# ============================================================================


@pytest.mark.asyncio
async def test_release_node_with_real_lock(mcp_tools, base_state, mock_server):
    """Test navigation lock release."""

    # Set lock state
    base_state.lock_acquired = True
    base_state.lock_token = "lock-token-123"

    # Mock unlock_navigation response
    mock_server.add_tool_response("nav.unlock", {"success": True})

    result = await release_node(base_state, mcp_tools)

    assert result.lock_acquired is False
    assert result.current_step == "release"
    assert result.status == "completed"
    assert len(result.steps_log) == 1


@pytest.mark.asyncio
async def test_release_node_with_mock_lock(mcp_tools, base_state):
    """Test release with mock lock (unlock not implemented)."""

    # Set mock lock state
    base_state.lock_acquired = True
    base_state.lock_token = "mock-lock-test-123"

    # Don't mock unlock - will raise MCPNotImplementedError

    result = await release_node(base_state, mcp_tools)

    # Should succeed even though unlock not implemented
    assert result.lock_acquired is False
    assert result.current_step == "release"
    assert result.status == "completed"


@pytest.mark.asyncio
async def test_release_node_no_lock(mcp_tools, base_state):
    """Test release when no lock was acquired."""

    # No lock acquired
    base_state.lock_acquired = False
    base_state.lock_token = None

    result = await release_node(base_state, mcp_tools)

    assert result.lock_acquired is False
    assert result.current_step == "release"
    assert result.status == "completed"


@pytest.mark.asyncio
async def test_release_node_preserves_error_status(mcp_tools, base_state):
    """Test release preserves failed status from earlier nodes."""

    # Simulate earlier failure
    base_state.status = "failed"
    base_state.error_message = "Some earlier error"
    base_state.lock_acquired = True
    base_state.lock_token = "mock-lock-test"

    result = await release_node(base_state, mcp_tools)

    # Should preserve failed status
    assert result.status == "failed"
    assert result.lock_acquired is False
