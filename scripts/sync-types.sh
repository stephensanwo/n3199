#!/bin/bash

# Ensure the webview types directory exists
mkdir -p webview/src/types

# Copy bridge types to webview
cp bridge/bridge.d.ts webview/src/types/bridge.ts

echo "Bridge types synced to webview project" 