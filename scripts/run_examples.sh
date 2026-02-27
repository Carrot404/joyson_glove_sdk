#!/bin/bash

# Joyson Glove SDK - Run Examples Script
# This script helps run example programs with proper setup

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_step() {
    echo -e "${BLUE}[STEP]${NC} $1"
}

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

main() {
    echo "=========================================="
    echo "  Joyson Glove SDK - Run Examples"
    echo "=========================================="
    echo ""

    # Check if build directory exists
    if [ ! -d "$BUILD_DIR" ]; then
        print_error "Build directory not found: $BUILD_DIR"
        print_info "Please build the project first: ./scripts/build.sh"
        exit 1
    fi

    cd "$BUILD_DIR"

    # Check if examples are built
    if [ ! -f "basic_motor_control" ]; then
        print_error "Examples not found. Please build with examples enabled."
        print_info "Run: cmake -DBUILD_EXAMPLES=ON .. && make"
        exit 1
    fi

    # Check network connectivity
    print_step "Checking network connectivity..."
    GLOVE_IP="192.168.10.123"

    if ping -c 1 -W 1 "$GLOVE_IP" &>/dev/null; then
        print_info "✓ Glove is reachable at $GLOVE_IP"
    else
        print_warn "✗ Cannot reach glove at $GLOVE_IP"
        print_warn "Please check:"
        echo "  1. Glove hardware is powered on"
        echo "  2. Network cable is connected"
        echo "  3. Network is configured (run: sudo ./scripts/setup_network.sh)"
        echo ""
        read -p "Continue anyway? (y/n): " continue_anyway
        if [ "$continue_anyway" != "y" ] && [ "$continue_anyway" != "Y" ]; then
            exit 1
        fi
    fi

    # Show available examples
    echo ""
    print_step "Available examples:"
    echo "  1. basic_motor_control - Basic motor control demonstration"
    echo "  2. read_sensors        - Sensor data reading demonstration"
    echo "  3. servo_mode_demo     - High-frequency servo control demonstration"
    echo "  4. all                 - Run all examples sequentially"
    echo "  0. exit                - Exit"
    echo ""

    # Select example
    read -p "Select example to run (1-4, 0 to exit): " choice

    case $choice in
        1)
            run_example "basic_motor_control"
            ;;
        2)
            run_example "read_sensors"
            ;;
        3)
            run_example "servo_mode_demo"
            ;;
        4)
            run_all_examples
            ;;
        0)
            print_info "Exiting..."
            exit 0
            ;;
        *)
            print_error "Invalid choice: $choice"
            exit 1
            ;;
    esac
}

run_example() {
    local example_name=$1

    echo ""
    echo "=========================================="
    print_info "Running: $example_name"
    echo "=========================================="
    echo ""

    # Set library path
    export LD_LIBRARY_PATH="$BUILD_DIR:$LD_LIBRARY_PATH"

    # Run example
    if [ -f "$example_name" ]; then
        ./"$example_name"
        local exit_code=$?

        echo ""
        if [ $exit_code -eq 0 ]; then
            print_info "✓ Example completed successfully"
        else
            print_error "✗ Example failed with exit code: $exit_code"
        fi

        return $exit_code
    else
        print_error "Example not found: $example_name"
        return 1
    fi
}

run_all_examples() {
    local examples=("basic_motor_control" "read_sensors" "servo_mode_demo")
    local failed=0

    for example in "${examples[@]}"; do
        run_example "$example"
        if [ $? -ne 0 ]; then
            ((failed++))
        fi

        # Wait between examples
        if [ "$example" != "${examples[-1]}" ]; then
            echo ""
            print_info "Waiting 3 seconds before next example..."
            sleep 3
        fi
    done

    echo ""
    echo "=========================================="
    if [ $failed -eq 0 ]; then
        print_info "✓ All examples completed successfully"
    else
        print_warn "✗ $failed example(s) failed"
    fi
    echo "=========================================="
}

# Run main function
main "$@"
