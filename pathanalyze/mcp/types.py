"""Type definitions for MCP client."""

from typing import Any, Literal
from pydantic import BaseModel, Field


class ViewportPosition(BaseModel):
    """Viewport position in slide coordinates."""
    x: float
    y: float


class ViewportInfo(BaseModel):
    """Viewport state information."""
    position: ViewportPosition
    zoom: float


class SlideInfo(BaseModel):
    """Slide metadata."""
    width: int
    height: int
    levels: int
    path: str
    viewport: ViewportInfo | None = None


class PolygonInfo(BaseModel):
    """Polygon loading result."""
    count: int
    classes: list[str]


class PolygonQueryResult(BaseModel):
    """Result of polygon query."""
    polygons: list[dict]  # TODO: Define polygon schema


class ToolCallResult(BaseModel):
    """Result of a tool call."""
    content: Any
    is_error: bool = Field(alias="isError", default=False)

    class Config:
        populate_by_name = True


class JSONRPCRequest(BaseModel):
    """JSON-RPC 2.0 request."""
    jsonrpc: Literal["2.0"] = "2.0"
    id: int
    method: str
    params: dict[str, Any] | None = None


class JSONRPCError(BaseModel):
    """JSON-RPC 2.0 error object."""
    code: int
    message: str
    data: Any | None = None


class JSONRPCResponse(BaseModel):
    """JSON-RPC 2.0 response."""
    jsonrpc: Literal["2.0"] = "2.0"
    id: int
    result: Any | None = None
    error: JSONRPCError | None = None
