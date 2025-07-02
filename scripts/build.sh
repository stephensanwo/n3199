#!/bin/bash

# Build script for C Desktop Application
# This script handles the compilation and linking of the desktop app

set -e  # Exit on any error

echo "=== Building C Desktop Application ==="
echo ""

# Ensure we're in the project root
cd "$(dirname "$0")/.." || exit 1

# Configuration
CC="gcc"
CFLAGS="-Wall -Wextra -std=c99"
PLATFORM_FLAGS="-DPLATFORM_MACOS -framework Cocoa -framework Foundation -framework WebKit"
SRCS="main.c config.c webview_framework.c platform_macos.c bridge.c bridge_builtin.c bridge_custom.c"
TARGET="desktop_app"

# Function to print status messages
print_status() {
    echo "✓ $1"
}

print_error() {
    echo "✗ $1" >&2
}

# Parse command line arguments
BUILD_TYPE="release"
CLEAN_FIRST=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="debug"
            CFLAGS="$CFLAGS -g -DDEBUG"
            echo "Building in debug mode..."
            shift
            ;;
        --clean)
            CLEAN_FIRST=true
            shift
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --debug    Build with debug symbols and DEBUG flag"
            echo "  --clean    Clean before building"
            echo "  --help     Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Clean if requested
if [ "$CLEAN_FIRST" = true ]; then
    echo "Cleaning previous build artifacts..."
    rm -f $TARGET *.o
    print_status "Clean completed"
    echo ""
fi

# Sync types before building
echo "Syncing bridge types..."
./scripts/sync-types.sh
print_status "Types synced"
echo ""

# Compile source files
echo "Compiling source files..."
for src in $SRCS; do
    obj="${src%.c}.o"
    echo "  Compiling $src -> $obj"
    if ! $CC $CFLAGS $PLATFORM_FLAGS -c "$src" -o "$obj"; then
        print_error "Failed to compile $src"
        exit 1
    fi
done
print_status "All source files compiled"

# Link the application
echo ""
echo "Linking application..."
OBJS=$(echo $SRCS | sed 's/\.c/.o/g')
if ! $CC $CFLAGS $PLATFORM_FLAGS -o $TARGET $OBJS; then
    print_error "Failed to link application"
    exit 1
fi
print_status "Application linked successfully"

# Show build information
echo ""
echo "Build completed successfully!"
echo "Target: $TARGET"
echo "Build type: $BUILD_TYPE"
echo "Size: $(ls -lh $TARGET | awk '{print $5}')"
echo ""
echo "Run './scripts/run.sh' to start the application" 