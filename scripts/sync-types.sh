#!/bin/bash

# Sync bridge types and implementation to webview project
# This creates symbolic links to the external bridge directory

set -e  # Exit on any error

echo "=== Syncing Bridge Types and Implementation ==="

# Ensure we're in the project root
cd "$(dirname "$0")/.." || exit 1

# Function to print status messages
print_status() {
    echo "✓ $1"
}

print_error() {
    echo "✗ $1" >&2
}

# Create webview bridge directory if it doesn't exist
WEBVIEW_BRIDGE_DIR="webview/src/bridge"
mkdir -p "$WEBVIEW_BRIDGE_DIR"
print_status "Bridge directory created: $WEBVIEW_BRIDGE_DIR"

# Remove existing bridge files in webview (if any)
if [ -f "$WEBVIEW_BRIDGE_DIR/bridge.d.ts" ] || [ -L "$WEBVIEW_BRIDGE_DIR/bridge.d.ts" ]; then
    rm -f "$WEBVIEW_BRIDGE_DIR/bridge.d.ts"
fi
if [ -f "$WEBVIEW_BRIDGE_DIR/bridge.ts" ] || [ -L "$WEBVIEW_BRIDGE_DIR/bridge.ts" ]; then
    rm -f "$WEBVIEW_BRIDGE_DIR/bridge.ts"
fi
if [ -f "$WEBVIEW_BRIDGE_DIR/index.ts" ] || [ -L "$WEBVIEW_BRIDGE_DIR/index.ts" ]; then
    rm -f "$WEBVIEW_BRIDGE_DIR/index.ts"
fi

# Store the project root path
PROJECT_ROOT="$(pwd)"

# Create symbolic links to the external bridge files
BRIDGE_SOURCE_DIR="$PROJECT_ROOT/bridge"
WEBVIEW_BRIDGE_FULL_DIR="$PROJECT_ROOT/$WEBVIEW_BRIDGE_DIR"

# Create relative paths for the symbolic links
RELATIVE_BRIDGE_DIR="../../../bridge"

echo "Creating symbolic links..."
cd "$WEBVIEW_BRIDGE_FULL_DIR"

# Link the type definitions
ln -sf "$RELATIVE_BRIDGE_DIR/bridge.d.ts" "bridge.d.ts"
print_status "Linked bridge.d.ts"

# Link the implementation
ln -sf "$RELATIVE_BRIDGE_DIR/bridge.ts" "bridge.ts"
print_status "Linked bridge.ts"

# Link the streaming type definitions
ln -sf "$RELATIVE_BRIDGE_DIR/stream.d.ts" "stream.d.ts"
print_status "Linked stream.d.ts"

# Link the streaming implementation
ln -sf "$RELATIVE_BRIDGE_DIR/stream.ts" "stream.ts"
print_status "Linked stream.ts"

# Go back to project root
cd "$PROJECT_ROOT"

# Remove the old bridge.ts from webview/src if it exists
if [ -f "webview/src/bridge.ts" ]; then
    rm -f "webview/src/bridge.ts"
    print_status "Removed old bridge.ts from webview/src"
fi

# Update the old types directory structure if it exists
if [ -d "webview/src/types" ]; then
    if [ -f "webview/src/types/bridge.ts" ] || [ -L "webview/src/types/bridge.ts" ]; then
        rm -f "webview/src/types/bridge.ts"
        print_status "Removed old bridge types from types directory"
    fi
    # Remove types directory if empty
    if [ -z "$(ls -A webview/src/types)" ]; then
        rmdir "webview/src/types"
        print_status "Removed empty types directory"
    fi
fi

echo ""
echo "Bridge types and implementation synced successfully!"
echo "Bridge files are now linked from: bridge/ -> webview/src/bridge/"
echo ""
echo "Files linked:"
echo "  - bridge.d.ts (type definitions)"
echo "  - bridge.ts (implementation)"
echo "  - stream.d.ts (streaming type definitions)"
echo "  - stream.ts (streaming implementation)" 