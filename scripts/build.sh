#!/bin/bash

# Joyson Glove SDK - Build Script
# This script automates the build process

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Print colored message
print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Main script
main() {
    print_info "Starting Joyson Glove SDK build process..."

    # Check dependencies
    print_info "Checking dependencies..."

    if ! command_exists cmake; then
        print_error "CMake not found. Please install: sudo apt-get install cmake"
        exit 1
    fi

    if ! command_exists g++; then
        print_error "g++ not found. Please install: sudo apt-get install build-essential"
        exit 1
    fi

    # Check CMake version
    CMAKE_VERSION=$(cmake --version | head -n1 | cut -d' ' -f3)
    print_info "CMake version: $CMAKE_VERSION"

    # Check g++ version
    GCC_VERSION=$(g++ --version | head -n1 | cut -d' ' -f4)
    print_info "g++ version: $GCC_VERSION"

    # Parse command line arguments
    BUILD_TYPE="Release"
    BUILD_EXAMPLES="ON"
    BUILD_TESTS="ON"
    CLEAN_BUILD=false
    INSTALL=false
    NUM_JOBS=$(nproc)

    while [[ $# -gt 0 ]]; do
        case $1 in
            --debug)
                BUILD_TYPE="Debug"
                shift
                ;;
            --release)
                BUILD_TYPE="Release"
                shift
                ;;
            --no-examples)
                BUILD_EXAMPLES="OFF"
                shift
                ;;
            --no-tests)
                BUILD_TESTS="OFF"
                shift
                ;;
            --clean)
                CLEAN_BUILD=true
                shift
                ;;
            --install)
                INSTALL=true
                shift
                ;;
            -j)
                NUM_JOBS="$2"
                shift 2
                ;;
            --help)
                echo "Usage: $0 [OPTIONS]"
                echo ""
                echo "Options:"
                echo "  --debug          Build in Debug mode (default: Release)"
                echo "  --release        Build in Release mode"
                echo "  --no-examples    Don't build examples"
                echo "  --no-tests       Don't build tests"
                echo "  --clean          Clean build directory before building"
                echo "  --install        Install after building (requires sudo)"
                echo "  -j N             Use N parallel jobs (default: $(nproc))"
                echo "  --help           Show this help message"
                exit 0
                ;;
            *)
                print_error "Unknown option: $1"
                echo "Use --help for usage information"
                exit 1
                ;;
        esac
    done

    # Get project root directory
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
    BUILD_DIR="$PROJECT_ROOT/build"

    print_info "Project root: $PROJECT_ROOT"
    print_info "Build directory: $BUILD_DIR"
    print_info "Build type: $BUILD_TYPE"
    print_info "Build examples: $BUILD_EXAMPLES"
    print_info "Build tests: $BUILD_TESTS"
    print_info "Parallel jobs: $NUM_JOBS"

    # Clean build directory if requested
    if [ "$CLEAN_BUILD" = true ]; then
        print_info "Cleaning build directory..."
        rm -rf "$BUILD_DIR"
    fi

    # Create build directory
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    # Configure
    print_info "Configuring project..."
    cmake .. \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DBUILD_EXAMPLES="$BUILD_EXAMPLES" \
        -DBUILD_TESTS="$BUILD_TESTS"

    # Build
    print_info "Building project with $NUM_JOBS parallel jobs..."
    make -j"$NUM_JOBS"

    # Check build result
    if [ $? -eq 0 ]; then
        print_info "Build completed successfully!"

        # List generated files
        print_info "Generated files:"
        ls -lh libjoyson_glove_sdk.so 2>/dev/null || true
        ls -lh basic_motor_control 2>/dev/null || true
        ls -lh read_sensors 2>/dev/null || true
        ls -lh servo_mode_demo 2>/dev/null || true
        ls -lh test_protocol 2>/dev/null || true
        ls -lh test_udp_client 2>/dev/null || true

        # Run tests if built
        if [ "$BUILD_TESTS" = "ON" ] && [ -f "test_protocol" ]; then
            print_info "Running tests..."
            if command_exists ctest; then
                ctest --output-on-failure
            else
                print_warn "ctest not found, skipping tests"
            fi
        fi

        # Install if requested
        if [ "$INSTALL" = true ]; then
            print_info "Installing..."
            sudo make install
            print_info "Installation completed!"
        fi

        print_info "Build process completed successfully!"
        echo ""
        print_info "Next steps:"
        echo "  1. Run examples: cd build && ./basic_motor_control"
        echo "  2. Run tests: cd build && ctest"
        echo "  3. Install: cd build && sudo make install"
    else
        print_error "Build failed!"
        exit 1
    fi
}

# Run main function
main "$@"
