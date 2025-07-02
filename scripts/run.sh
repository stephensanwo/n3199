#!/bin/bash

# Script to build and run the C desktop application with TypeScript type syncing

echo "=== C Desktop App Runner ==="
echo ""

# Ensure we're in the project root
cd "$(dirname "$0")/.." || exit 1

# Configuration
OUTPUT_DIR="output"
EXECUTABLE="$OUTPUT_DIR/desktop_app"

# Parse command line arguments
DEBUG_MODE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            DEBUG_MODE=true
            shift
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --debug    Build and run with debug symbols"
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

# Build the application
echo "Building application..."
if [ "$DEBUG_MODE" = true ]; then
    ./scripts/build.sh --debug
else
    ./scripts/build.sh
fi

if [ $? -ne 0 ]; then
    echo "Error: Build failed."
    exit 1
fi

echo ""
echo "Build successful! Starting application..."
echo "Close the window or press Ctrl+C to quit."
echo ""

# Run the application
$EXECUTABLE 