"""FastAPI application entrypoint."""

from contextlib import asynccontextmanager
from fastapi import FastAPI, HTTPException, BackgroundTasks
from fastapi.responses import JSONResponse
from pydantic import BaseModel, Field
import uuid

from pathanalyze.config import settings
from pathanalyze.mcp.client import MCPClient
from pathanalyze.mcp.exceptions import MCPException
from pathanalyze.utils.logging import setup_logging, logger


# Request/Response models
class AnalysisRequest(BaseModel):
    """Request to start analysis."""
    slide_path: str = Field(..., description="Path to slide file")
    task: str = Field(..., description="Analysis task description")
    roi_hint: dict | None = Field(None, description="Optional ROI hint")


class AnalysisResponse(BaseModel):
    """Response with run ID."""
    run_id: str = Field(..., description="Unique run identifier")
    status: str = Field(..., description="Initial status")


class RunStatus(BaseModel):
    """Status of an analysis run."""
    run_id: str
    status: str  # pending, running, completed, failed
    message: str | None = None


# In-memory run tracking (replace with DB in production)
runs: dict[str, RunStatus] = {}


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Lifecycle management."""
    setup_logging(settings.log_level, settings.log_format)
    logger.info("PathAnalyze starting", extra={"version": "0.1.0"})

    # Test MCP connection on startup
    try:
        async with MCPClient(str(settings.mcp_base_url)) as client:
            await client.initialize()
            logger.info("MCP connection verified")
    except Exception as e:
        logger.error("Failed to connect to MCP server", extra={"error": str(e)})
        # Continue anyway - connections will be per-request

    yield

    logger.info("PathAnalyze shutting down")


app = FastAPI(
    title="PathAnalyze",
    description="LangGraph-based pathology analysis agent",
    version="0.1.0",
    lifespan=lifespan,
)


@app.get("/health")
async def health_check():
    """Health check endpoint."""
    return {"status": "ok", "version": "0.1.0"}


@app.post("/analyze", response_model=AnalysisResponse)
async def start_analysis(
    request: AnalysisRequest,
    background_tasks: BackgroundTasks
) -> AnalysisResponse:
    """
    Start a new analysis task.

    This will spawn a LangGraph run in the background.
    """
    run_id = str(uuid.uuid4())

    # Create initial status
    runs[run_id] = RunStatus(
        run_id=run_id,
        status="pending",
        message="Analysis queued"
    )

    # Queue background task (placeholder for LangGraph execution)
    background_tasks.add_task(run_analysis, run_id, request)

    logger.info("Analysis started", extra={"run_id": run_id, "slide": request.slide_path})

    return AnalysisResponse(
        run_id=run_id,
        status="pending"
    )


@app.get("/runs/{run_id}", response_model=RunStatus)
async def get_run_status(run_id: str) -> RunStatus:
    """Get status of an analysis run."""
    if run_id not in runs:
        raise HTTPException(status_code=404, detail="Run not found")
    return runs[run_id]


async def run_analysis(run_id: str, request: AnalysisRequest):
    """
    Execute analysis (placeholder for LangGraph integration).

    This will be replaced with actual LangGraph execution in Step 3.
    """
    try:
        runs[run_id].status = "running"
        runs[run_id].message = "Connecting to MCP server"

        # Placeholder: Just test connection
        async with MCPClient(str(settings.mcp_base_url)) as client:
            await client.initialize()
            runs[run_id].message = "Connected to MCP server"

            # TODO: Execute LangGraph here

        runs[run_id].status = "completed"
        runs[run_id].message = "Analysis complete (placeholder)"

    except Exception as e:
        logger.error("Analysis failed", extra={"run_id": run_id, "error": str(e)})
        runs[run_id].status = "failed"
        runs[run_id].message = str(e)


@app.exception_handler(MCPException)
async def mcp_exception_handler(request, exc: MCPException):
    """Handle MCP exceptions."""
    return JSONResponse(
        status_code=500,
        content={
            "error": "mcp_error",
            "code": exc.code,
            "message": exc.message,
            "retryable": exc.retryable,
        }
    )
