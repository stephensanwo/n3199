# C Desktop Application

A simple cross-platform desktop application framework written in C, starting with macOS native window support.

## Project Structure

```
.
├── main.c          # Main application source code
├── Makefile        # Build configuration
├── run.sh          # Build and run script
└── README.md       # This file
```

## Prerequisites

- macOS with Xcode command line tools installed
- GCC/Clang compiler (comes with Xcode tools)
- Make utility

## Building and Running

### Option 1: Using the run script (recommended)
```bash
./run.sh
```

### Option 2: Using make directly
```bash
# Build the application
make

# Run the application
make run

# Clean build artifacts
make clean
```

### Option 3: Manual compilation
```bash
gcc -Wall -Wextra -std=c99 -framework Cocoa -framework Foundation -o desktop_app main.c
./desktop_app
```

## What This Does

This simple application demonstrates:
- Creating a native macOS window using Objective-C runtime from C
- Basic window management (title, size, position)
- macOS application lifecycle integration
- Foundation for building more complex desktop applications

## Next Steps

This is the foundation for building a more complex desktop application framework that will eventually support:
- WebView integration for frontend frameworks (React, Vue, Svelte)
- Native toolbar customization
- IPC between C backend and frontend
- Cross-platform support (Windows, Linux)

## Troubleshooting

If you encounter build errors:
1. Ensure Xcode command line tools are installed: `xcode-select --install`
2. Verify gcc is available: `gcc --version`
3. Check that you're running on macOS (this version is macOS-specific) 