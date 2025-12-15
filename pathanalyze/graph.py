"""
LangGraph definition for PathAnalyze agent.

This is a stub for Step 3 implementation.
"""

from langgraph.graph import StateGraph
from typing import TypedDict


class AnalysisState(TypedDict):
    """State passed between LangGraph nodes."""
    slide_path: str
    task: str
    mcp_client: object  # Will be MCPClient instance
    lock_token: str | None
    status: str


def create_analysis_graph() -> StateGraph:
    """
    Create the analysis workflow graph.

    TODO: Implement in Step 3
    - Nodes: connect, lock, navigate, annotate, unlock
    - Error handling with cleanup
    - Streaming updates
    """
    raise NotImplementedError("LangGraph implementation pending (Step 3)")
