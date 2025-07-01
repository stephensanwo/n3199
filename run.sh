#!/bin/bash

# Simple script to build and run the C desktop application

echo "=== C Desktop App Builder ==="
echo ""

# Check if make is available
if ! command -v make &> /dev/null; then
    echo "Error: make is not installed. Please install Xcode command line tools."
    exit 1
fi

# Check if gcc is available
if ! command -v gcc &> /dev/null; then
    echo "Error: gcc is not installed. Please install Xcode command line tools."
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