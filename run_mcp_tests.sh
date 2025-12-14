#!/usr/bin/env bash

#
# PathView MCP Test Orchestration Script
#
# This script orchestrates the complete test environment:
# 1. Launches PathView GUI
# 2. Launches PathView MCP server
# 3. Waits for services to be ready
# 4. Runs the MCP test suite
# 5. Cleans up processes on exit
#

set -e  # Exit on error (except where handled)

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
PATHVIEW_BIN="${BUILD_DIR}/pathview"
PATHVIEW_MCP_BIN="${BUILD_DIR}/pathview-mcp"
TEST_SCRIPT="${SCRIPT_DIR}/test_mcp_client.py"

MCP_PORT=9000
HTTP_PORT=8080
SOCKET_PATH="/tmp/pathview-latest.sock"

# Process IDs
PATHVIEW_PID=""
MCP_SERVER_PID=""

# Cleanup function
cleanup() {
    echo ""
    echo -e "${BLUE}[CLEANUP]${NC} Stopping services..."

    if [ -n "$MCP_SERVER_PID" ]; then
        echo -e "${BLUE}[CLEANUP]${NC} Stopping MCP server (PID: $MCP_SERVER_PID)..."
        kill $MCP_SERVER_PID 2>/dev/null || true
        wait $MCP_SERVER_PID 2>/dev/null || true
    fi

    if [ -n "$PATHVIEW_PID" ]; then
        echo -e "${BLUE}[CLEANUP]${NC} Stopping PathView GUI (PID: $PATHVIEW_PID)..."
        kill $PATHVIEW_PID 2>/dev/null || true
        wait $PATHVIEW_PID 2>/dev/null || true
    fi

    # Clean up socket file
    if [ -S "$SOCKET_PATH" ]; then
        rm -f "$SOCKET_PATH"
    fi

    echo -e "${GREEN}[CLEANUP]${NC} Done"
}

# Set up trap to cleanup on exit
trap cleanup EXIT INT TERM

# Function to wait for a process to be ready
wait_for_service() {
    local service_name="$1"
    local check_command="$2"
    local max_wait="$3"
    local wait_time=0

    echo -e "${BLUE}[WAIT]${NC} Waiting for $service_name to be ready..."

    while [ $wait_time -lt $max_wait ]; do
        if eval "$check_command" 2>/dev/null; then
            echo -e "${GREEN}[WAIT]${NC} $service_name is ready!"
            return 0
        fi
        sleep 0.5
        wait_time=$((wait_time + 1))
    done

    echo -e "${RED}[ERROR]${NC} $service_name failed to start within ${max_wait}s"
    return 1
}

# Function to check if port is in use
check_port() {
    lsof -i :$1 >/dev/null 2>&1
}

# Print banner
echo "=========================================="
echo "PathView MCP Test Orchestration"
echo "=========================================="
echo ""

# Check prerequisites
echo -e "${BLUE}[CHECK]${NC} Checking prerequisites..."

if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}[ERROR]${NC} Build directory not found: $BUILD_DIR"
    echo "Please build PathView first:"
    echo "  cmake --build build"
    exit 1
fi

if [ ! -x "$PATHVIEW_BIN" ]; then
    echo -e "${RED}[ERROR]${NC} PathView binary not found or not executable: $PATHVIEW_BIN"
    echo "Please build PathView first:"
    echo "  cmake --build build"
    exit 1
fi

if [ ! -x "$PATHVIEW_MCP_BIN" ]; then
    echo -e "${RED}[ERROR]${NC} PathView MCP binary not found or not executable: $PATHVIEW_MCP_BIN"
    echo "Please build PathView first:"
    echo "  cmake --build build"
    exit 1
fi

if [ ! -f "$TEST_SCRIPT" ]; then
    echo -e "${RED}[ERROR]${NC} Test script not found: $TEST_SCRIPT"
    exit 1
fi

# Check Python
if ! command -v python3 &> /dev/null; then
    echo -e "${RED}[ERROR]${NC} Python 3 not found"
    exit 1
fi

# Check Python dependencies
echo -e "${BLUE}[CHECK]${NC} Checking Python dependencies..."
if ! python3 -c "import requests; import sseclient" 2>/dev/null; then
    echo -e "${YELLOW}[WARN]${NC} Missing Python dependencies"
    echo "Installing dependencies..."
    pip3 install -r "${SCRIPT_DIR}/requirements.txt" || {
        echo -e "${RED}[ERROR]${NC} Failed to install Python dependencies"
        exit 1
    }
fi

# Parse arguments
SLIDE_PATH=""
POLYGON_PATH=""
VERBOSE=""
JSON_OUTPUT=""
EXTRA_ARGS=()

while [[ $# -gt 0 ]]; do
    case $1 in
        --slide)
            SLIDE_PATH="$2"
            shift 2
            ;;
        --polygons)
            POLYGON_PATH="$2"
            shift 2
            ;;
        -v|--verbose)
            VERBOSE="-v"
            shift
            ;;
        --json)
            JSON_OUTPUT="--json"
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --slide PATH       Path to slide file (required)"
            echo "  --polygons PATH    Path to polygon file (optional)"
            echo "  -v, --verbose      Verbose output"
            echo "  --json             JSON output mode"
            echo "  -h, --help         Show this help"
            echo ""
            echo "Examples:"
            echo "  $0 --slide /path/to/slide.svs"
            echo "  $0 --slide /path/to/slide.svs --polygons /path/to/cells.pb -v"
            exit 0
            ;;
        *)
            EXTRA_ARGS+=("$1")
            shift
            ;;
    esac
done

# Check if slide path is provided
if [ -z "$SLIDE_PATH" ]; then
    echo -e "${RED}[ERROR]${NC} Slide path not provided"
    echo "Usage: $0 --slide /path/to/slide.svs [--polygons /path/to/cells.pb] [-v]"
    exit 1
fi

if [ ! -f "$SLIDE_PATH" ]; then
    echo -e "${RED}[ERROR]${NC} Slide file not found: $SLIDE_PATH"
    exit 1
fi

if [ -n "$POLYGON_PATH" ] && [ ! -f "$POLYGON_PATH" ]; then
    echo -e "${RED}[ERROR]${NC} Polygon file not found: $POLYGON_PATH"
    exit 1
fi

echo -e "${GREEN}[CHECK]${NC} All prerequisites satisfied"
echo ""

# Check if ports are already in use
if check_port $MCP_PORT; then
    echo -e "${YELLOW}[WARN]${NC} Port $MCP_PORT is already in use"
    echo "Attempting to kill existing process..."
    lsof -ti :$MCP_PORT | xargs kill -9 2>/dev/null || true
    sleep 1
fi

if check_port $HTTP_PORT; then
    echo -e "${YELLOW}[WARN]${NC} Port $HTTP_PORT is already in use"
    echo "Attempting to kill existing process..."
    lsof -ti :$HTTP_PORT | xargs kill -9 2>/dev/null || true
    sleep 1
fi

# Clean up old socket file
if [ -S "$SOCKET_PATH" ]; then
    echo -e "${BLUE}[CLEANUP]${NC} Removing old socket file..."
    rm -f "$SOCKET_PATH"
fi

# Launch PathView GUI
echo -e "${BLUE}[LAUNCH]${NC} Starting PathView GUI..."
"$PATHVIEW_BIN" > /dev/null 2>&1 &
PATHVIEW_PID=$!

if ! ps -p $PATHVIEW_PID > /dev/null; then
    echo -e "${RED}[ERROR]${NC} Failed to start PathView GUI"
    exit 1
fi

echo -e "${GREEN}[LAUNCH]${NC} PathView GUI started (PID: $PATHVIEW_PID)"

# Wait for PathView to create socket
if ! wait_for_service "PathView IPC socket" "[ -S $SOCKET_PATH ]" 10; then
    echo -e "${RED}[ERROR]${NC} PathView failed to create IPC socket"
    exit 1
fi

# Launch MCP Server
echo -e "${BLUE}[LAUNCH]${NC} Starting MCP server..."
"$PATHVIEW_MCP_BIN" \
    --socket "$SOCKET_PATH" \
    --http-port $HTTP_PORT \
    --mcp-port $MCP_PORT \
    > /tmp/pathview-mcp.log 2>&1 &
MCP_SERVER_PID=$!

if ! ps -p $MCP_SERVER_PID > /dev/null; then
    echo -e "${RED}[ERROR]${NC} Failed to start MCP server"
    echo "Check logs: /tmp/pathview-mcp.log"
    exit 1
fi

echo -e "${GREEN}[LAUNCH]${NC} MCP server started (PID: $MCP_SERVER_PID)"

# Wait for HTTP server to be ready
if ! wait_for_service "HTTP server" "curl -s http://127.0.0.1:$HTTP_PORT/health > /dev/null" 20; then
    echo -e "${RED}[ERROR]${NC} HTTP server failed to start"
    echo "Check logs: /tmp/pathview-mcp.log"
    cat /tmp/pathview-mcp.log
    exit 1
fi

# Wait for MCP server to be ready (give it a bit more time)
sleep 2

echo ""
echo "=========================================="
echo "Services Ready"
echo "=========================================="
echo -e "PathView GUI:  ${GREEN}Running${NC} (PID: $PATHVIEW_PID)"
echo -e "MCP Server:    ${GREEN}Running${NC} (PID: $MCP_SERVER_PID, Port: $MCP_PORT)"
echo -e "HTTP Server:   ${GREEN}Running${NC} (Port: $HTTP_PORT)"
echo "=========================================="
echo ""

# Build test command
TEST_CMD="python3 \"$TEST_SCRIPT\" \"$SLIDE_PATH\""

if [ -n "$POLYGON_PATH" ]; then
    TEST_CMD="$TEST_CMD --polygons \"$POLYGON_PATH\""
fi

if [ -n "$VERBOSE" ]; then
    TEST_CMD="$TEST_CMD $VERBOSE"
fi

if [ -n "$JSON_OUTPUT" ]; then
    TEST_CMD="$TEST_CMD $JSON_OUTPUT"
fi

# Add any extra arguments
for arg in "${EXTRA_ARGS[@]}"; do
    TEST_CMD="$TEST_CMD \"$arg\""
done

# Run tests
echo -e "${BLUE}[TEST]${NC} Running MCP test suite..."
echo -e "${BLUE}[TEST]${NC} Command: $TEST_CMD"
echo ""

# Execute test command and capture exit code
set +e  # Don't exit on error
eval $TEST_CMD
TEST_EXIT_CODE=$?
set -e

echo ""

# Report results
if [ $TEST_EXIT_CODE -eq 0 ]; then
    echo "=========================================="
    echo -e "${GREEN}SUCCESS${NC} - All tests passed!"
    echo "=========================================="
else
    echo "=========================================="
    echo -e "${RED}FAILURE${NC} - Some tests failed (exit code: $TEST_EXIT_CODE)"
    echo "=========================================="
fi

echo ""
echo "Logs:"
echo "  MCP Server: /tmp/pathview-mcp.log"
echo ""

# Cleanup will be called automatically by trap
exit $TEST_EXIT_CODE
