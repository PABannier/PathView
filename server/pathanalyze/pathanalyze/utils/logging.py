"""Structured logging setup."""

import logging
import sys
import json
from datetime import datetime


class JSONFormatter(logging.Formatter):
    """Format logs as JSON."""

    def format(self, record: logging.LogRecord) -> str:
        log_data = {
            "timestamp": datetime.utcnow().isoformat(),
            "level": record.levelname,
            "logger": record.name,
            "message": record.getMessage(),
        }

        # Add extra fields
        if hasattr(record, "run_id"):
            log_data["run_id"] = record.run_id
        if hasattr(record, "error"):
            log_data["error"] = record.error

        return json.dumps(log_data)


def setup_logging(level: str = "INFO", format_type: str = "json"):
    """Configure logging."""
    logger = logging.getLogger("pathanalyze")
    logger.setLevel(level.upper())

    handler = logging.StreamHandler(sys.stdout)

    if format_type == "json":
        handler.setFormatter(JSONFormatter())
    else:
        handler.setFormatter(
            logging.Formatter(
                "%(asctime)s - %(name)s - %(levelname)s - %(message)s"
            )
        )

    logger.addHandler(handler)
    return logger


# Global logger instance
logger = logging.getLogger("pathanalyze")
