#!/bin/bash

# Joyson Glove SDK - Installation Script
# This script installs the SDK to the system

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

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    print_error "This script must be run as root (use sudo)"
    exit 1
fi

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

main() {
    echo "=========================================="
    echo "  Joyson Glove SDK - Installation"
    echo "=========================================="
    echo ""

    # Check if build directory exists
    if [ ! -d "$BUILD_DIR" ]; then
        print_error "Build directory not found: $BUILD_DIR"
        print_info "Please build the project first: ./scripts/build.sh"
        exit 1
    fi

    cd "$BUILD_DIR"

    # Check if library is built
    if [ ! -f "libjoyson_glove_sdk.so" ]; then
        print_error "Library not found. Please build the project first."
        exit 1
    fi

    # Installation options
    PREFIX="/usr/local"
    INSTALL_EXAMPLES=false
    INSTALL_DOCS=true

    while [[ $# -gt 0 ]]; do
        case $1 in
            --prefix)
                PREFIX="$2"
                shift 2
                ;;
            --with-examples)
                INSTALL_EXAMPLES=true
                shift
                ;;
            --no-docs)
                INSTALL_DOCS=false
                shift
                ;;
            --help)
                echo "Usage: $0 [OPTIONS]"
                echo ""
                echo "Options:"
                echo "  --prefix PATH        Installation prefix (default: /usr/local)"
                echo "  --with-examples      Install example programs"
                echo "  --no-docs            Don't install documentation"
                echo "  --help               Show this help message"
                exit 0
                ;;
            *)
                print_error "Unknown option: $1"
                echo "Use --help for usage information"
                exit 1
                ;;
        esac
    done

    print_info "Installation prefix: $PREFIX"
    print_info "Install examples: $INSTALL_EXAMPLES"
    print_info "Install docs: $INSTALL_DOCS"
    echo ""

    # Step 1: Install library
    print_step "Installing library..."
    mkdir -p "$PREFIX/lib"
    cp -v libjoyson_glove_sdk.so "$PREFIX/lib/"
    chmod 755 "$PREFIX/lib/libjoyson_glove_sdk.so"
    print_info "✓ Library installed to $PREFIX/lib/"

    # Step 2: Install headers
    print_step "Installing headers..."
    mkdir -p "$PREFIX/include/joyson_glove"
    cp -v "$PROJECT_ROOT/include/joyson_glove/"*.hpp "$PREFIX/include/joyson_glove/"
    chmod 644 "$PREFIX/include/joyson_glove/"*.hpp
    print_info "✓ Headers installed to $PREFIX/include/joyson_glove/"

    # Step 3: Install CMake config
    print_step "Installing CMake configuration..."
    mkdir -p "$PREFIX/lib/cmake/joyson_glove_sdk"
    cp -v joyson_glove_sdkConfig.cmake "$PREFIX/lib/cmake/joyson_glove_sdk/"
    cp -v joyson_glove_sdkConfigVersion.cmake "$PREFIX/lib/cmake/joyson_glove_sdk/"
    chmod 644 "$PREFIX/lib/cmake/joyson_glove_sdk/"*.cmake
    print_info "✓ CMake config installed to $PREFIX/lib/cmake/joyson_glove_sdk/"

    # Step 4: Install examples (optional)
    if [ "$INSTALL_EXAMPLES" = true ]; then
        print_step "Installing examples..."
        mkdir -p "$PREFIX/share/joyson_glove_sdk/examples"

        if [ -f "basic_motor_control" ]; then
            cp -v basic_motor_control "$PREFIX/share/joyson_glove_sdk/examples/"
            chmod 755 "$PREFIX/share/joyson_glove_sdk/examples/basic_motor_control"
        fi

        if [ -f "read_sensors" ]; then
            cp -v read_sensors "$PREFIX/share/joyson_glove_sdk/examples/"
            chmod 755 "$PREFIX/share/joyson_glove_sdk/examples/read_sensors"
        fi

        if [ -f "servo_mode_demo" ]; then
            cp -v servo_mode_demo "$PREFIX/share/joyson_glove_sdk/examples/"
            chmod 755 "$PREFIX/share/joyson_glove_sdk/examples/servo_mode_demo"
        fi

        print_info "✓ Examples installed to $PREFIX/share/joyson_glove_sdk/examples/"
    fi

    # Step 5: Install documentation (optional)
    if [ "$INSTALL_DOCS" = true ]; then
        print_step "Installing documentation..."
        mkdir -p "$PREFIX/share/doc/joyson_glove_sdk"

        cp -v "$PROJECT_ROOT/README.md" "$PREFIX/share/doc/joyson_glove_sdk/"
        cp -v "$PROJECT_ROOT/QUICKSTART.md" "$PREFIX/share/doc/joyson_glove_sdk/"
        cp -v "$PROJECT_ROOT/API_REFERENCE.md" "$PREFIX/share/doc/joyson_glove_sdk/"
        cp -v "$PROJECT_ROOT/PROTOCOL.md" "$PREFIX/share/doc/joyson_glove_sdk/"
        cp -v "$PROJECT_ROOT/TROUBLESHOOTING.md" "$PREFIX/share/doc/joyson_glove_sdk/"
        cp -v "$PROJECT_ROOT/CHANGELOG.md" "$PREFIX/share/doc/joyson_glove_sdk/"
        cp -v "$PROJECT_ROOT/LICENSE" "$PREFIX/share/doc/joyson_glove_sdk/"

        chmod 644 "$PREFIX/share/doc/joyson_glove_sdk/"*
        print_info "✓ Documentation installed to $PREFIX/share/doc/joyson_glove_sdk/"
    fi

    # Step 6: Update library cache
    print_step "Updating library cache..."
    ldconfig
    print_info "✓ Library cache updated"

    # Step 7: Verify installation
    print_step "Verifying installation..."

    if [ -f "$PREFIX/lib/libjoyson_glove_sdk.so" ]; then
        print_info "✓ Library: $PREFIX/lib/libjoyson_glove_sdk.so"
    else
        print_error "✗ Library not found"
    fi

    if [ -d "$PREFIX/include/joyson_glove" ]; then
        HEADER_COUNT=$(ls -1 "$PREFIX/include/joyson_glove/"*.hpp 2>/dev/null | wc -l)
        print_info "✓ Headers: $HEADER_COUNT files in $PREFIX/include/joyson_glove/"
    else
        print_error "✗ Headers not found"
    fi

    # Done
    echo ""
    echo "=========================================="
    print_info "Installation completed successfully!"
    echo "=========================================="
    echo ""
    echo "Installation summary:"
    echo "  Library: $PREFIX/lib/libjoyson_glove_sdk.so"
    echo "  Headers: $PREFIX/include/joyson_glove/"
    echo "  CMake config: $PREFIX/lib/cmake/joyson_glove_sdk/"

    if [ "$INSTALL_EXAMPLES" = true ]; then
        echo "  Examples: $PREFIX/share/joyson_glove_sdk/examples/"
    fi

    if [ "$INSTALL_DOCS" = true ]; then
        echo "  Documentation: $PREFIX/share/doc/joyson_glove_sdk/"
    fi

    echo ""
    echo "Next steps:"
    echo "  1. Include in your project: #include <joyson_glove/glove_sdk.hpp>"
    echo "  2. Link with: -ljoyson_glove_sdk"
    echo "  3. Or use CMake: find_package(joyson_glove_sdk REQUIRED)"
    echo ""
}

# Run main function
main "$@"
