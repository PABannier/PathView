#!/bin/bash
# ==============================================================================
# WSI Streamer E2E Test Script
# ==============================================================================
#
# This script sets up a WSIStreamer server via docker-compose, uploads a test
# slide to MinIO, runs integration tests, and cleans up.
#
# Usage:
#   ./wsi_streamer_e2e.sh <path_to_slide.svs>
#
# Prerequisites:
#   - Docker and docker-compose
#   - MinIO client (mc): brew install minio-mc
#   - Test slide file
#
# ==============================================================================

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
WSISTREAMER_DIR="$PROJECT_ROOT/external/WSIStreamer"
TEST_BINARY="$PROJECT_ROOT/build/test/wsi_streamer_integration_tests"

# Server configuration
WSI_STREAMER_URL="http://localhost:3000"
MINIO_URL="http://localhost:9000"
MINIO_USER="minioadmin"
MINIO_PASS="minioadmin"
BUCKET_NAME="slides"

# Timeout settings (seconds)
STARTUP_TIMEOUT=120
HEALTH_CHECK_INTERVAL=2

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

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

# ==============================================================================
# Prerequisites Check
# ==============================================================================

check_prerequisites() {
    log_info "Checking prerequisites..."

    # Check docker
    if ! command -v docker &> /dev/null; then
        log_error "docker is not installed"
        exit 1
    fi

    # Check docker-compose (try both commands)
    if command -v docker-compose &> /dev/null; then
        DOCKER_COMPOSE="docker-compose"
    elif docker compose version &> /dev/null; then
        DOCKER_COMPOSE="docker compose"
    else
        log_error "docker-compose is not installed"
        exit 1
    fi
    log_info "Using: $DOCKER_COMPOSE"

    # Check MinIO client
    if ! command -v mc &> /dev/null; then
        log_error "MinIO client (mc) is not installed. Install with: brew install minio-mc"
        exit 1
    fi

    # Check slide file
    if [ ! -f "$SLIDE_FILE" ]; then
        log_error "Slide file not found: $SLIDE_FILE"
        exit 1
    fi

    # Check test binary
    if [ ! -f "$TEST_BINARY" ]; then
        log_error "Test binary not found: $TEST_BINARY"
        log_error "Build with: cmake --build build --target wsi_streamer_integration_tests"
        exit 1
    fi

    # Check docker-compose.yml exists
    if [ ! -f "$WSISTREAMER_DIR/docker-compose.yml" ]; then
        log_error "docker-compose.yml not found at: $WSISTREAMER_DIR"
        exit 1
    fi

    log_info "All prerequisites satisfied"
}

# ==============================================================================
# Cleanup Handler
# ==============================================================================

cleanup() {
    log_info "Cleaning up..."

    cd "$WSISTREAMER_DIR"

    # Stop and remove containers
    $DOCKER_COMPOSE down --volumes --remove-orphans 2>/dev/null || true

    # Remove MinIO alias
    mc alias rm local 2>/dev/null || true

    log_info "Cleanup complete"
}

# ==============================================================================
# Service Management
# ==============================================================================

start_services() {
    log_info "Starting WSIStreamer services..."

    cd "$WSISTREAMER_DIR"

    # Build and start services in detached mode
    $DOCKER_COMPOSE up -d --build

    log_info "Services started"
}

wait_for_healthy() {
    log_info "Waiting for WSIStreamer to be healthy..."

    local elapsed=0

    while [ $elapsed -lt $STARTUP_TIMEOUT ]; do
        if curl -s "$WSI_STREAMER_URL/health" | grep -q "healthy"; then
            log_info "WSIStreamer is healthy!"
            return 0
        fi

        sleep $HEALTH_CHECK_INTERVAL
        elapsed=$((elapsed + HEALTH_CHECK_INTERVAL))
        echo -n "."
    done

    echo ""
    log_error "WSIStreamer failed to become healthy within ${STARTUP_TIMEOUT}s"

    # Show logs for debugging
    log_info "WSIStreamer logs:"
    cd "$WSISTREAMER_DIR"
    $DOCKER_COMPOSE logs --tail=50 wsi-streamer

    return 1
}

wait_for_minio() {
    log_info "Waiting for MinIO to be ready..."

    local elapsed=0

    while [ $elapsed -lt $STARTUP_TIMEOUT ]; do
        # Check if MinIO health endpoint returns HTTP 200
        if curl -s -o /dev/null -w "%{http_code}" "$MINIO_URL/minio/health/live" | grep -q "200"; then
            log_info "MinIO is ready!"
            return 0
        fi

        sleep $HEALTH_CHECK_INTERVAL
        elapsed=$((elapsed + HEALTH_CHECK_INTERVAL))
        echo -n "."
    done

    echo ""
    log_error "MinIO failed to become ready within ${STARTUP_TIMEOUT}s"
    return 1
}

# ==============================================================================
# Slide Upload
# ==============================================================================

configure_minio_client() {
    log_info "Configuring MinIO client..."

    # Remove existing alias if present
    mc alias rm local 2>/dev/null || true

    # Set up alias
    mc alias set local "$MINIO_URL" "$MINIO_USER" "$MINIO_PASS"

    log_info "MinIO client configured"
}

upload_slide() {
    log_info "Uploading slide to MinIO..."

    local slide_name=$(basename "$SLIDE_FILE")

    # Upload file
    mc cp "$SLIDE_FILE" "local/$BUCKET_NAME/"

    log_info "Slide uploaded: $slide_name"

    # Export for tests
    export WSI_TEST_SLIDE_ID="$slide_name"
}

verify_slide_accessible() {
    log_info "Verifying slide is accessible via API..."

    local response=$(curl -s "$WSI_STREAMER_URL/slides")

    if echo "$response" | grep -q "$WSI_TEST_SLIDE_ID"; then
        log_info "Slide verified in API response"
        return 0
    else
        log_error "Slide not found in API response"
        log_error "Response: $response"
        return 1
    fi
}

# ==============================================================================
# Test Execution
# ==============================================================================

run_tests() {
    log_info "Running integration tests..."

    export WSI_STREAMER_URL
    export WSI_TEST_SLIDE_ID

    log_info "WSI_STREAMER_URL=$WSI_STREAMER_URL"
    log_info "WSI_TEST_SLIDE_ID=$WSI_TEST_SLIDE_ID"

    # Run tests with color output
    "$TEST_BINARY" --gtest_color=yes

    return $?
}

# ==============================================================================
# Main
# ==============================================================================

main() {
    # Parse arguments
    SLIDE_FILE="${1:-$HOME/Downloads/TCGA-A6-2686-01Z-00-DX1.0540a027-2a0c-46c7-9af0-7b8672631de7.svs}"

    echo ""
    echo "=============================================="
    echo "  WSI Streamer E2E Integration Tests"
    echo "=============================================="
    echo ""
    echo "Slide file: $SLIDE_FILE"
    echo "Server URL: $WSI_STREAMER_URL"
    echo ""

    # Set up cleanup trap
    trap cleanup EXIT

    # Run steps
    check_prerequisites
    start_services
    wait_for_minio
    wait_for_healthy
    configure_minio_client
    upload_slide
    verify_slide_accessible

    # Run tests and capture exit code
    local test_exit_code=0
    run_tests || test_exit_code=$?

    echo ""
    if [ $test_exit_code -eq 0 ]; then
        log_info "All tests passed!"
    else
        log_error "Some tests failed (exit code: $test_exit_code)"
    fi

    # Return test exit code
    exit $test_exit_code
}

# Run main
main "$@"
