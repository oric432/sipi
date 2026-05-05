#!/bin/bash

set -e

# Configuration
SIPI_TARGET="sip:test@192.168.1.50:5060"
NUM_CALLS=3
CALL_DURATION=5  # seconds
BASE_PORT=5080

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if baresip is installed
if ! command -v baresip &> /dev/null; then
    echo -e "${RED}Error: baresip not found. Install with: apt install baresip${NC}"
    exit 1
fi

# Create temporary baresip config
create_baresip_config() {
    local port=$1
    local config_dir="/tmp/baresip_${port}"
    mkdir -p "$config_dir"

    cat > "$config_dir/.baresiprc" << EOF
#
# Baresip configuration for testing (auto-generated)
#
listen          0.0.0.0:${port}
rtp_port        $((BASE_PORT + 1000))
rtp_bandwidth   512
rtcp_mux        yes
jitter_buffer   0-0
audio_codec     PCMA
audio_frame     20
EOF
    echo "$config_dir"
}

# Test single call
test_single() {
    echo -e "${YELLOW}=== Single Call Test ===${NC}"
    echo "Target: $SIPI_TARGET"
    echo "Duration: ${CALL_DURATION}s"
    echo ""

    config_dir=$(create_baresip_config $BASE_PORT)

    echo -e "${GREEN}Starting baresip...${NC}"
    # Run baresip with auto-answer and exit after call
    baresip -c "$config_dir/.baresiprc" \
        -e "call $SIPI_TARGET; sleep $CALL_DURATION; exit" \
        2>&1 | head -50

    echo ""
    echo -e "${GREEN}Single call test complete${NC}"

    # Cleanup
    rm -rf "$config_dir"
}

# Test multiple concurrent calls
test_multi() {
    echo -e "${YELLOW}=== Concurrent Calls Test (${NUM_CALLS} calls) ===${NC}"
    echo "Target: $SIPI_TARGET"
    echo "Duration: ${CALL_DURATION}s per call"
    echo ""

    pids=()

    # Start multiple baresip instances
    for i in $(seq 1 $NUM_CALLS); do
        port=$((BASE_PORT + i))
        config_dir=$(create_baresip_config $port)

        echo -e "${GREEN}[Call $i] Starting baresip on port $port...${NC}"

        # Run baresip in background with auto-answer
        (
            baresip -c "$config_dir/.baresiprc" \
                -e "call $SIPI_TARGET; sleep $CALL_DURATION; exit" \
                2>&1 | sed "s/^/[Call $i] /"
        ) &

        pids+=($!)
        sleep 0.5  # Stagger the starts slightly
    done

    echo ""
    echo -e "${GREEN}All calls started. Waiting for completion...${NC}"

    # Wait for all to complete
    for pid in "${pids[@]}"; do
        wait "$pid" 2>/dev/null || true
    done

    echo ""
    echo -e "${GREEN}All concurrent calls complete${NC}"

    # Cleanup temp configs
    for i in $(seq 1 $NUM_CALLS); do
        rm -rf "/tmp/baresip_$((BASE_PORT + i))"
    done
}

# Usage
usage() {
    cat << EOF
Usage: $0 [COMMAND] [OPTIONS]

Commands:
    single          Run a single call test
    multi           Run multiple concurrent calls test (default: $NUM_CALLS calls)

Options:
    -n, --calls NUM     Number of concurrent calls (for 'multi', default: $NUM_CALLS)
    -d, --duration SEC  Call duration in seconds (default: $CALL_DURATION)
    -t, --target URI    SIP target URI (default: $SIPI_TARGET)
    -h, --help          Show this help message

Examples:
    $0 single                           # Single call
    $0 multi                            # 3 concurrent calls
    $0 multi --calls 5 --duration 10   # 5 calls, 10 seconds each

Note: Ensure sipi is running before executing this script.
      Expected to see call-ids and RTP ports in sipi logs.
EOF
}

# Parse arguments
COMMAND="${1:-multi}"

shift 2>/dev/null || true

while [[ $# -gt 0 ]]; do
    case $1 in
        -n|--calls)
            NUM_CALLS="$2"
            shift 2
            ;;
        -d|--duration)
            CALL_DURATION="$2"
            shift 2
            ;;
        -t|--target)
            SIPI_TARGET="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

# Main
echo -e "${YELLOW}SIPI Baresip Test Suite${NC}"
echo "Target: $SIPI_TARGET"
echo ""

case "$COMMAND" in
    single)
        test_single
        ;;
    multi)
        test_multi
        ;;
    *)
        echo -e "${RED}Unknown command: $COMMAND${NC}"
        usage
        exit 1
        ;;
esac
