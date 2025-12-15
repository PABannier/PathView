"""MCP client exceptions."""

from typing import Any


class MCPException(Exception):
    """Base exception for MCP operations."""

    def __init__(
        self,
        code: int,
        message: str,
        retryable: bool = False,
        data: Any = None
    ):
        self.code = code
        self.message = message
        self.retryable = retryable
        self.data = data
        super().__init__(f"[{code}] {message}")


class MCPConnectionError(MCPException):
    """Connection-related errors (always retryable)."""

    def __init__(self, message: str):
        super().__init__(-32000, message, retryable=True)


class MCPTimeoutError(MCPException):
    """Timeout errors (retryable)."""

    def __init__(self, message: str = "Request timeout"):
        super().__init__(-32000, message, retryable=True)


class MCPToolError(MCPException):
    """Tool execution errors."""

    def __init__(self, code: int, message: str, retryable: bool = False):
        super().__init__(code, message, retryable=retryable)


class MCPNotImplementedError(MCPException):
    """Feature not yet implemented in MCP server."""

    def __init__(self, feature: str):
        super().__init__(
            -32601,
            f"Feature not implemented: {feature}",
            retryable=False
        )


# JSON-RPC error code mapping
ERROR_CODE_MAP = {
    -32700: "Parse Error",
    -32600: "Invalid Request",
    -32601: "Method Not Found",
    -32602: "Invalid Params",
    -32603: "Internal Error",
    -32000: "Server Error",
}
