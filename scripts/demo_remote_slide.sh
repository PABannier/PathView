#!/bin/bash
# ==============================================================================
# Remote Slide Viewer Demo
# ==============================================================================
#
# This script demonstrates PathView's remote slide viewing capability by:
#   1. Starting a WSIStreamer tile server with MinIO storage
#   2. Uploading test slides to the S3 bucket
#   3. Launching PathView for interactive testing
#
# Usage:
#   ./scripts/demo_remote_slide.sh <slide1.svs> [slide2.svs] [slide3.svs] ...
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
#   4. Browse and select the uploaded slide(s)
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

# Array to store slide files
SLIDE_FILES=()

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

    # Check all slide files
    for slide_file in "${SLIDE_FILES[@]}"; do
        if [ ! -f "$slide_file" ]; then
            log_error "Slide file not found: $slide_file"
            exit 1
        fi
        log_info "Slide file: $slide_file"
    done

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

upload_slides() {
    log_step "Uploading slides to MinIO"

    # Configure MinIO client
    mc alias rm local 2>/dev/null || true
    mc alias set local "$MINIO_URL" "$MINIO_USER" "$MINIO_PASS" --quiet

    # Upload each slide
    for slide_file in "${SLIDE_FILES[@]}"; do
        local slide_name=$(basename "$slide_file")
        log_info "Uploading: $slide_name"
        mc cp "$slide_file" "local/$BUCKET_NAME/"
    done

    log_info "Upload complete (${#SLIDE_FILES[@]} slide(s))"
}

verify_slides() {
    log_info "Verifying slides are accessible..."

    local response=$(curl -s "$WSI_STREAMER_URL/slides")
    local all_found=true

    for slide_file in "${SLIDE_FILES[@]}"; do
        local slide_name=$(basename "$slide_file")
        if echo "$response" | grep -q "$slide_name"; then
            log_info "Verified: $slide_name"
        else
            log_error "Slide not found in API: $slide_name"
            all_found=false
        fi
    done

    if [ "$all_found" = false ]; then
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
    echo -e "${BOLD}│${NC}  Uploaded slides: ${CYAN}${#SLIDE_FILES[@]}${NC}"
    for slide_file in "${SLIDE_FILES[@]}"; do
        echo -e "${BOLD}│${NC}    - ${CYAN}$(basename "$slide_file")${NC}"
    done
    echo -e "${BOLD}├─────────────────────────────────────────────────────────────┤${NC}"
    echo -e "${BOLD}│${NC}  ${YELLOW}To view the remote slides:${NC}"
    echo -e "${BOLD}│${NC}  1. File -> Connect to Server (Ctrl+Shift+O)"
    echo -e "${BOLD}│${NC}  2. Enter: ${CYAN}http://localhost:3000${NC}"
    echo -e "${BOLD}│${NC}  3. Click Connect, then browse and select a slide"
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
    echo "Usage: $0 <slide1.svs> [slide2.svs] [slide3.svs] ..."
    echo ""
    echo "Demonstrates PathView's remote slide viewing capability by starting"
    echo "a WSIStreamer tile server and uploading test slides."
    echo ""
    echo "Examples:"
    echo "  $0 ~/Downloads/sample.svs"
    echo "  $0 ~/Downloads/slide1.svs ~/Downloads/slide2.svs"
    echo "  $0 /path/to/*.svs"
}

main() {
    # Parse arguments
    if [ $# -lt 1 ]; then
        print_usage
        exit 1
    fi

    # Collect all slide files
    SLIDE_FILES=("$@")

    echo ""
    echo -e "${BOLD}╔═════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BOLD}║          PathView Remote Slide Viewer Demo                  ║${NC}"
    echo -e "${BOLD}╚═════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    log_info "Slides to upload: ${#SLIDE_FILES[@]}"

    # Set up cleanup trap
    trap cleanup EXIT

    # Run steps
    check_prerequisites
    start_services
    wait_for_minio
    wait_for_wsistreamer
    upload_slides
    verify_slides
    launch_pathview
}

main "$@"
