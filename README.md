# Cross platform desktop apps in C + Webview 

Multi framework support, react, vue, svelte

## Prerequisites

- macOS with Xcode command line tools installed
- GCC/Clang compiler (comes with Xcode tools)
- Make utility
- Node.js and pnpm (for WebView development)

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
gcc -Wall -Wextra -std=c99 -DPLATFORM_MACOS -framework Cocoa -framework Foundation -framework WebKit -o desktop_app main.c config.c platform_macos.c webview_framework.c
./desktop_app
```

## WebView Integration

### React + TypeScript + Vite Setup
The `webview/` directory contains a complete React application with:
- **React 19** with TypeScript support
- **Vite** for fast development and building
- **ESLint** for code quality
- **Hot Module Replacement** for instant updates

### WebView Configuration
```json
{
  "webview": {
    "enabled": true,
    "developer_extras": true,
    "javascript_enabled": true,
    "framework": {
      "build_command": "pnpm run build",
      "dev_command": "pnpm run dev",
      "dev_url": "http://localhost:5174",
      "build_dir": "dist",
      "dev_mode": true
    }
  }
}
```

### Development Workflow
1. **Development Mode**: Automatically starts Vite dev server and loads React app
2. **Production Mode**: Builds the React app and serves static files
3. **Hot Reload**: Changes in React code are instantly reflected in the desktop app
4. **Developer Tools**: WebKit inspector available for debugging

## Menu System

The framework includes a comprehensive native menu system with full customization:

### Menu Configuration
```json
{
  "menubar": {
    "enabled": true,
    "show_about_item": true,
    "show_preferences_item": true,
    "show_services_menu": false,
    "file_menu": {
      "enabled": true,
      "title": "File",
      "items": [
        {
          "title": "New",
          "shortcut": "cmd+n",
          "action": "new",
          "enabled": true,
          "separator_after": false
        }
      ]
    }
  }
}
```

### Supported Menus
- **File Menu** - New, Open, Save, Close operations
- **Edit Menu** - Undo, Redo, Cut, Copy, Paste
- **View Menu** - Zoom controls
- **Window Menu** - Minimize, Zoom window
- **Help Menu** - Documentation and support

### Menu Features
- Native keyboard shortcuts (cmd+n, cmd+s, etc.)
- Enable/disable individual items
- Separator lines between groups
- Custom actions and handlers

## Configuration System

The application uses JSON configuration files to customize window behavior and platform-specific features.

### Basic Usage
```bash
# Run with default config
./desktop_app

# Run with custom config
./desktop_app my_config.json
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
    "transparent": false
  }
}
```

#### macOS-Specific Features
```json
{
  "macos": {
    "toolbar": {
      "enabled": false
    }
  }
}
```

#### Development Options
```json
{
  "development": {
    "debug_mode": true,
    "console_logging": true
  }
}
```


### WebView Development
1. Navigate to the `webview/` directory
2. Install dependencies: `pnpm install`
3. Start development: `pnpm run dev` (or let the C app handle it)
4. Build for production: `pnpm run build`

### Adding Menu Actions
Menu actions are handled in the platform-specific code. To add new actions:
1. Add the action to your menu configuration
2. Implement the handler in `platform_macos.c` (or platform-specific file)
3. Update the action dispatcher

## Examples

### Basic Desktop App
```bash
./desktop_app
```
Creates a standard desktop application with native menus.

### WebView App with React
Enable WebView in your config and the app will automatically:
1. Build your React application
2. Start the development server
3. Load the React app in a native WebView
4. Provide hot reload during development

### Custom Menu Configuration
Create a custom menu structure by modifying the menu sections in your config file.

## API Reference

### WebView Functions
- `platform_webview_load_url()` - Load a URL in the WebView
- `platform_webview_load_html()` - Load HTML content directly
- `platform_webview_evaluate_javascript()` - Execute JavaScript code
- `platform_webview_navigate()` - Navigate to the configured URL

### Framework Functions
- `run_build_command()` - Build the web application
- `start_dev_server()` - Start development server
- `stop_dev_server()` - Stop development server
- `get_webview_url()` - Get the current WebView URL
