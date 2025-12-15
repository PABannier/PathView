"""Configuration management using Pydantic settings."""

from pydantic_settings import BaseSettings, SettingsConfigDict
from pydantic import Field, HttpUrl


class Settings(BaseSettings):
    """Application settings loaded from environment variables."""

    model_config = SettingsConfigDict(
        env_prefix="PATHANALYZE_",
        env_file=".env",
        case_sensitive=False,
    )

    # MCP Server Configuration
    mcp_base_url: HttpUrl = Field(
        default="http://127.0.0.1:9000",
        description="Base URL for PathView MCP server"
    )
    mcp_sse_endpoint: str = Field(
        default="/sse",
        description="SSE endpoint path"
    )
    mcp_timeout: int = Field(
        default=30,
        description="Request timeout in seconds"
    )
    mcp_reconnect_attempts: int = Field(
        default=3,
        description="Number of reconnection attempts"
    )
    mcp_reconnect_delay: float = Field(
        default=1.0,
        description="Delay between reconnection attempts (seconds)"
    )

    # FastAPI Configuration
    api_host: str = Field(default="127.0.0.1", description="API server host")
    api_port: int = Field(default=8000, description="API server port")
    api_reload: bool = Field(default=False, description="Enable auto-reload")

    # Logging
    log_level: str = Field(default="INFO", description="Log level")
    log_format: str = Field(default="json", description="Log format (json|text)")

    # LangGraph Configuration (for future use)
    openai_api_key: str | None = Field(default=None, description="OpenAI API key")
    langsmith_api_key: str | None = Field(default=None, description="LangSmith API key")


# Global settings instance
settings = Settings()
