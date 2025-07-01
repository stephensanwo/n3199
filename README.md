# C Desktop Application Framework

A cross-platform desktop application framework written in C with native window management and configuration-driven customization. Currently supports macOS with plans for Windows and Linux.

## Features

- ✅ **Platform-Aware Architecture** - Modular design supporting multiple platforms
- ✅ **JSON Configuration** - Flexible, user-friendly configuration system
- ✅ **Native Window Management** - Platform-specific window creation and styling
- ✅ **macOS Native Features** - Toolbar support, transparency, frameless windows
- ✅ **Debug Mode** - Comprehensive logging and development features
- 🔄 **Extensible Design** - Easy to add new platforms and features

## Project Structure

```
.
├── main.c                    # Main application entry point
├── config.h / config.c       # Configuration parsing system
├── platform.h               # Platform abstraction layer
├── platform_macos.c         # macOS-specific implementation
├── config.json              # Default application configuration
├── config_transparent.json  # Example: Transparent window config
├── config_frameless.json    # Example: Frameless window config
├── Makefile                  # Build system
├── run.sh                    # Build and run script
└── README.md                 # This file
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

# Build with debug information
make debug

# Run with default configuration
make run

# Clean build artifacts
make clean

# Show project information
make info
```

### Option 3: Manual compilation
```bash
gcc -Wall -Wextra -std=c99 -DPLATFORM_MACOS -framework Cocoa -framework Foundation -o desktop_app main.c config.c platform_macos.c
./desktop_app
```

## Configuration System

The application uses JSON configuration files to customize window behavior and platform-specific features.

### Basic Usage
```bash
# Run with default config
./desktop_app

# Run with custom config
./desktop_app config_transparent.json

# Run with frameless window
./desktop_app config_frameless.json
```

### Configuration Options

#### App Section
```json
{
  "app": {
    "name": "My Desktop App",
    "version": "1.0.0",
    "bundle_id": "com.example.app"
  }
}
```

#### Window Configuration
```json
{
  "window": {
    "title": "Window Title",
    "width": 1200,
    "height": 800,
    "min_width": 600,
    "min_height": 400,
    "center": true,
    "resizable": true,
    "minimizable": true,
    "maximizable": true,
    "closable": true,
    "frameless": false,
    "transparent": false,
    "always_on_top": false,
    "start_hidden": false
  }
}
```

#### macOS-Specific Features
```json
{
  "platform": {
    "macos": {
      "toolbar": {
        "enabled": true,
        "transparent": false,
        "unified": true,
        "full_size_content_view": false,
        "hide_title": false,
        "appearance": "auto"
      },
      "window_style": {
        "vibrant_background": false,
        "blur_effect": "none",
        "material": "window"
      },
      "title_bar": {
        "traffic_lights_position": "default",
        "custom_traffic_lights": false
      }
    }
  }
}
```

#### Development Options
```json
{
  "development": {
    "debug_mode": true,
    "hot_reload": true,
    "console_logging": true
  }
}
```

## Platform Support

### Current: macOS ✅
- Native NSWindow and NSToolbar integration
- Transparency and blur effects
- Frameless windows
- Custom window styling
- Native appearance support

### Planned: Windows 🔄
- Win32 API integration
- Custom frame rendering
- Windows 10/11 styling

### Planned: Linux 🔄
- GTK integration
- Window manager compatibility
- Cross-desktop support

## Examples

### Transparent Window
```bash
./desktop_app config_transparent.json
```
Creates a window with transparent background and native toolbar.

### Frameless Window
```bash
./desktop_app config_frameless.json
```
Creates a borderless window without title bar controls.

### Custom Configuration
Create your own config file based on the examples and run:
```bash
./desktop_app my_custom_config.json
```

## Development

### Debug Mode
Enable debug mode in your configuration to see detailed logging:
```json
{
  "development": {
    "debug_mode": true,
    "console_logging": true
  }
}
```

### Adding Platform Support
1. Create `platform_[os].c` with platform-specific implementations
2. Update `Makefile` to include the new platform
3. Add platform detection in `config.h`
4. Implement the platform interface from `platform.h`

## Next Steps

This framework is designed to eventually support:
- WebView integration for modern frontend frameworks (React, Vue, Svelte)
- IPC bridge between C backend and frontend
- Hot reload for development
- Asset bundling and embedding
- Cross-platform packaging

## Troubleshooting

### Build Issues
1. Ensure Xcode command line tools: `xcode-select --install`
2. Verify compiler: `gcc --version`
3. Check platform: `uname -a`

### Runtime Issues
1. Enable debug mode in configuration
2. Check console output for detailed logging
3. Verify configuration file syntax with a JSON validator

### Configuration Issues
1. Use `make info` to see build configuration
2. Test with provided example configurations
3. Validate JSON syntax 