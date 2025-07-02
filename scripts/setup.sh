#!/bin/bash

# Setup script for C Desktop Application
# This script handles initial project setup and dependency validation

set -e  # Exit on any error

echo "=== Setting up C Desktop Application ==="
echo ""

# Function to check if a command exists
command_exists() {
    command -v "$1" &> /dev/null
}

# Function to print status messages
print_status() {
    echo "✓ $1"
}

print_error() {
    echo "✗ $1" >&2
}

# Check required dependencies
echo "Checking dependencies..."

if ! command_exists make; then
    print_error "make is not installed. Please install Xcode command line tools:"
    echo "  xcode-select --install"
    exit 1
fi
print_status "make found"

if ! command_exists gcc; then
    print_error "gcc is not installed. Please install Xcode command line tools:"
    echo "  xcode-select --install"
    exit 1
fi
print_status "gcc found"

# Check for macOS-specific requirements
if [[ "$OSTYPE" == "darwin"* ]]; then
    if ! xcode-select -p &> /dev/null; then
        print_error "Xcode command line tools not properly installed"
        echo "Please run: xcode-select --install"
        exit 1
    fi
    print_status "Xcode command line tools found"
fi

# Create necessary directories
echo ""
echo "Creating project directories..."
mkdir -p webview/src/types
print_status "webview/src/types directory created"

# Make scripts executable
echo ""
echo "Setting up script permissions..."
chmod +x scripts/*.sh
print_status "Script permissions set"

# Sync types
echo ""
echo "Syncing bridge types..."
./scripts/sync-types.sh

echo ""
echo "Setup completed successfully!"
echo "You can now run 'make run' to build and start the application." 