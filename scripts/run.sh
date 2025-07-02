#!/bin/bash

# Script to build and run the C desktop application with TypeScript type syncing

echo "=== C Desktop App Builder ==="
echo ""

# Check dependencies
if ! command -v make &> /dev/null; then
    echo "Error: make is not installed. Please install Xcode command line tools."
    exit 1
fi

if ! command -v gcc &> /dev/null; then
    echo "Error: gcc is not installed. Please install Xcode command line tools."
    exit 1
fi

# Ensure we're in the project root
cd "$(dirname "$0")/.." || exit 1

echo "Syncing bridge types..."
./scripts/sync-types.sh

if [ $? -ne 0 ]; then
    echo "Error: Failed to sync bridge types."
    exit 1
fi

echo "Building application..."
make clean
make all

if [ $? -eq 0 ]; then
    echo ""
    echo "Build successful! Starting application..."
    echo "Close the window or press Ctrl+C to quit."
    echo ""
    ./desktop_app
else
    echo ""
    echo "Build failed. Please check for errors above."
    exit 1
fi 