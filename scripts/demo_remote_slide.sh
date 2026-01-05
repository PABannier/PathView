#!/bin/bash
# ==============================================================================
# Remote Slide Viewer Demo
# ==============================================================================
#
# This script demonstrates PathView's remote slide viewing capability by:
#   1. Starting a WSIStreamer tile server with MinIO storage
#   2. Uploading a test slide to the S3 bucket
#   3. Launching PathView for interactive testing
#
# Usage:
#   ./scripts/demo_remote_slide.sh <path_to_slide.svs>
#
# Prerequisites:
#   - Docker and docker-compose
#   - MinIO client (mc): brew install minio-mc
#   - PathView built: ./build/pathview
#
# Once PathView launches:
#   1. Go to File -> Connect to Server (or Ctrl+Shift+O)
#   2. Enter: http://localhost:3000
#   3. Click Connect
#   4. Browse and select the uploaded slide
#
# ==============================================================================

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
WSISTREAMER_DIR="$PROJECT_ROOT/external/WSIStreamer"
PATHVIEW_BINARY="$PROJECT_ROOT/build/pathview"

# Server configuration
WSI_STREAMER_URL="http://localhost:3000"
MINIO_URL="http://localhost:9000"
MINIO_USER="minioadmin"
MINIO_PASS="minioadmin"
BUCKET_NAME="slides"

# Timeout settings
STARTUP_TIMEOUT=120
HEALTH_CHECK_INTERVAL=2

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

# ==============================================================================
# Helper Functions
# ==============================================================================

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_step() {
    echo -e "\n${CYAN}${BOLD}==> $1${NC}"
}

# ==============================================================================
# Prerequisites Check
# ==============================================================================

check_prerequisites() {
    log_step "Checking prerequisites"

    # Check docker
    if ! command -v docker &> /dev/null; then
        log_error "docker is not installed"
        exit 1
    fi
    log_info "Docker: OK"

    # Check docker-compose
    if command -v docker-compose &> /dev/null; then
        DOCKER_COMPOSE="docker-compose"
    elif docker compose version &> /dev/null; then
        DOCKER_COMPOSE="docker compose"
    else
        log_error "docker-compose is not installed"
        exit 1
    fi
    log_info "Docker Compose: OK ($DOCKER_COMPOSE)"

    # Check MinIO client
    if ! command -v mc &> /dev/null; then
        log_error "MinIO client (mc) is not installed"
        log_error "Install with: brew install minio-mc"
        exit 1
    fi
    log_info "MinIO Client: OK"

    # Check slide file
    if [ ! -f "$SLIDE_FILE" ]; then
        log_error "Slide file not found: $SLIDE_FILE"
        exit 1
    fi
    log_info "Slide file: $SLIDE_FILE"

    # Check PathView binary
    if [ ! -f "$PATHVIEW_BINARY" ]; then
        log_error "PathView binary not found: $PATHVIEW_BINARY"
        log_error "Build with: cmake --build build"
        exit 1
    fi
    log_info "PathView binary: OK"

    # Check docker-compose.yml
    if [ ! -f "$WSISTREAMER_DIR/docker-compose.yml" ]; then
        log_error "WSIStreamer docker-compose.yml not found"
        log_error "Initialize submodule: git submodule update --init"
        exit 1
    fi
    log_info "WSIStreamer: OK"
}

# ==============================================================================
# Cleanup Handler
# ==============================================================================

cleanup() {
    echo ""
    log_step "Cleaning up"

    cd "$WSISTREAMER_DIR"
    $DOCKER_COMPOSE down --volumes --remove-orphans 2>/dev/null || true
    mc alias rm local 2>/dev/null || true

    log_info "Cleanup complete"
}

# ==============================================================================
# Service Management
# ==============================================================================

start_services() {
    log_step "Starting WSIStreamer services"

    cd "$WSISTREAMER_DIR"
    $DOCKER_COMPOSE up -d --build

    log_info "Services started"
}

wait_for_minio() {
    log_info "Waiting for MinIO..."

    local elapsed=0
    while [ $elapsed -lt $STARTUP_TIMEOUT ]; do
        if curl -s -o /dev/null -w "%{http_code}" "$MINIO_URL/minio/health/live" | grep -q "200"; then
            log_info "MinIO is ready"
            return 0
        fi
        sleep $HEALTH_CHECK_INTERVAL
        elapsed=$((elapsed + HEALTH_CHECK_INTERVAL))
        echo -n "."
    done

    echo ""
    log_error "MinIO failed to become ready"
    return 1
}

wait_for_wsistreamer() {
    log_info "Waiting for WSIStreamer..."

    local elapsed=0
    while [ $elapsed -lt $STARTUP_TIMEOUT ]; do
        if curl -s "$WSI_STREAMER_URL/health" | grep -q "healthy"; then
            log_info "WSIStreamer is ready"
            return 0
        fi
        sleep $HEALTH_CHECK_INTERVAL
        elapsed=$((elapsed + HEALTH_CHECK_INTERVAL))
        echo -n "."
    done

    echo ""
    log_error "WSIStreamer failed to become ready"
    return 1
}

# ==============================================================================
# Slide Upload
# ==============================================================================

upload_slide() {
    log_step "Uploading slide to MinIO"

    local slide_name=$(basename "$SLIDE_FILE")

    # Configure MinIO client
    mc alias rm local 2>/dev/null || true
    mc alias set local "$MINIO_URL" "$MINIO_USER" "$MINIO_PASS" --quiet

    # Upload
    log_info "Uploading: $slide_name"
    mc cp "$SLIDE_FILE" "local/$BUCKET_NAME/"

    log_info "Upload complete"
}

verify_slide() {
    log_info "Verifying slide is accessible..."

    local slide_name=$(basename "$SLIDE_FILE")
    local response=$(curl -s "$WSI_STREAMER_URL/slides")

    if echo "$response" | grep -q "$slide_name"; then
        log_info "Slide verified in API"
    else
        log_error "Slide not found in API response"
        return 1
    fi
}

# ==============================================================================
# Launch PathView
# ==============================================================================

launch_pathview() {
    log_step "Launching PathView"

    echo ""
    echo -e "${BOLD}┌─────────────────────────────────────────────────────────────┐${NC}"
    echo -e "${BOLD}│  PathView Remote Slide Demo                                 │${NC}"
    echo -e "${BOLD}├─────────────────────────────────────────────────────────────┤${NC}"
    echo -e "${BOLD}│${NC}  Server URL: ${CYAN}$WSI_STREAMER_URL${NC}"
    echo -e "${BOLD}│${NC}  Slide: ${CYAN}$(basename "$SLIDE_FILE")${NC}"
    echo -e "${BOLD}├─────────────────────────────────────────────────────────────┤${NC}"
    echo -e "${BOLD}│${NC}  ${YELLOW}To view the remote slide:${NC}"
    echo -e "${BOLD}│${NC}  1. File -> Connect to Server (Ctrl+Shift+O)"
    echo -e "${BOLD}│${NC}  2. Enter: ${CYAN}http://localhost:3000${NC}"
    echo -e "${BOLD}│${NC}  3. Click Connect, then browse and select the slide"
    echo -e "${BOLD}├─────────────────────────────────────────────────────────────┤${NC}"
    echo -e "${BOLD}│${NC}  ${RED}Press Ctrl+C to stop the server when done${NC}"
    echo -e "${BOLD}└─────────────────────────────────────────────────────────────┘${NC}"
    echo ""

    # Launch PathView (blocks until user closes it)
    "$PATHVIEW_BINARY"
}

# ==============================================================================
# Main
# ==============================================================================

print_usage() {
    echo "Usage: $0 <path_to_slide.svs>"
    echo ""
    echo "Demonstrates PathView's remote slide viewing capability by starting"
    echo "a WSIStreamer tile server and uploading a test slide."
    echo ""
    echo "Example:"
    echo "  $0 ~/Downloads/sample.svs"
}

main() {
    # Parse arguments
    if [ $# -lt 1 ]; then
        print_usage
        exit 1
    fi

    SLIDE_FILE="$1"

    echo ""
    echo -e "${BOLD}╔═════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BOLD}║          PathView Remote Slide Viewer Demo                  ║${NC}"
    echo -e "${BOLD}╚═════════════════════════════════════════════════════════════╝${NC}"
    echo ""

    # Set up cleanup trap
    trap cleanup EXIT

    # Run steps
    check_prerequisites
    start_services
    wait_for_minio
    wait_for_wsistreamer
    upload_slide
    verify_slide
    launch_pathview
}

main "$@"
