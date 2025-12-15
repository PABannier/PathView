"""CLI entrypoint: python -m pathanalyze"""

import uvicorn
from pathanalyze.config import settings


def main():
    """Start the FastAPI server."""
    uvicorn.run(
        "pathanalyze.main:app",
        host=settings.api_host,
        port=settings.api_port,
        reload=settings.api_reload,
    )


if __name__ == "__main__":
    main()
