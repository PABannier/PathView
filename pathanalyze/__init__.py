"""
PathAnalyze: LangGraph-based pathology analysis agent.

Connects to PathView MCP server for slide navigation and annotation.
"""

__version__ = "0.1.0"

from pathanalyze.mcp.client import MCPClient
from pathanalyze.mcp.exceptions import MCPException, MCPConnectionError

__all__ = ["MCPClient", "MCPException", "MCPConnectionError", "__version__"]
