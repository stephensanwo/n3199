#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

// Platform detection
#ifdef __APPLE__
    #define PLATFORM_MACOS 1
    #define PLATFORM_NAME "macos"
#elif _WIN32
    #define PLATFORM_WINDOWS 1
    #define PLATFORM_NAME "windows"
#elif __linux__
    #define PLATFORM_LINUX 1
    #define PLATFORM_NAME "linux"
#else
    #define PLATFORM_UNKNOWN 1
    #define PLATFORM_NAME "unknown"
#endif

// Configuration structures
typedef struct {
    char name[256];
    char version[64];
    char bundle_id[256];
} app_config_t;

typedef struct {
    char title[64];
    char shortcut[16];  // e.g., "cmd+n", "cmd+o"
    char action[32];    // e.g., "new", "open", "save"
    bool enabled;
    bool separator_after;
} menu_item_config_t;

typedef struct {
    char title[64];
    menu_item_config_t items[16];
    int item_count;
    bool enabled;
} menu_config_t;

// Toolbar button configuration
typedef struct {
    char name[64];         // Button name/title
    char icon[64];         // SF Symbol name (e.g., "gear", "magnifyingglass")
    char action[64];       // Action function name to call
    char tooltip[128];     // Tooltip text
    bool enabled;          // Whether button is enabled
} toolbar_button_config_t;

// Toolbar groups configuration
typedef struct {
    toolbar_button_config_t buttons[8];  // Max 8 buttons per group
    int button_count;
} toolbar_group_config_t;

typedef struct {
    bool enabled;
    toolbar_group_config_t left;     // Left toolbar group
    toolbar_group_config_t middle;   // Middle toolbar group  
    toolbar_group_config_t right;    // Right toolbar group
} macos_toolbar_config_t;

typedef struct {
    macos_toolbar_config_t toolbar;
    bool show_title_bar;  // Show traditional title bar (default: false for modern appearance)
} macos_config_t;

typedef struct {
    menu_config_t file_menu;
    menu_config_t edit_menu;
    menu_config_t view_menu;
    menu_config_t window_menu;
    menu_config_t help_menu;
    bool enabled;
    bool show_about_item;
    bool show_preferences_item;
    bool show_services_menu;
} menubar_config_t;

typedef struct {
    char title[256];
    int width;
    int height;
    int min_width;
    int min_height;
    bool center;
    bool resizable;
    bool minimizable;
    bool maximizable;
    bool closable;
} window_config_t;

typedef struct {
    bool debug_mode;
    bool console_logging;
} development_config_t;

// Framework configuration
typedef struct {
    char build_command[256];          // Build command
    char dev_command[256];            // Development server command
    char dev_url[256];                // Development server URL
    char build_dir[64];               // Build output directory
    bool dev_mode;                    // Development mode flag
} webview_framework_config_t;

// WebView configuration
typedef struct {
    bool enabled;
    bool developer_extras;
    bool javascript_enabled;
    webview_framework_config_t framework;  // Framework-specific configuration
} webview_config_t;

// NEW: Streaming configuration structures
typedef struct {
    char host[64];                    // Server host (e.g., "127.0.0.1")
    int port;                         // Server port (e.g., 8080)
    int max_connections;              // Maximum concurrent connections
} stream_server_config_t;

typedef struct {
    char name[64];                    // Stream function name (e.g., "system.memory")
    char endpoint[128];               // HTTP endpoint (e.g., "/stream/memory")
    char handler[64];                 // Handler function name (e.g., "stream_system_memory")
    int interval_ms;                  // Update interval in milliseconds
    bool enabled;                     // Whether stream is enabled
    char description[256];            // Description of the stream
} stream_function_config_t;

typedef struct {
    bool enabled;                     // Whether streaming is enabled
    stream_server_config_t server;    // Server configuration
    stream_function_config_t streams[16];  // Array of stream functions
    int stream_count;                 // Number of configured streams
} streaming_config_t;

typedef struct {
    app_config_t app;
    window_config_t window;
    menubar_config_t menubar;
    webview_config_t webview;
    streaming_config_t streaming;     // NEW: Streaming configuration
    
    #ifdef PLATFORM_MACOS
    macos_config_t macos;
    #endif
    
    development_config_t development;
} app_configuration_t;

// Function declarations
app_configuration_t* load_config(const char* config_file);
void free_config(app_configuration_t* config);
void print_config(const app_configuration_t* config);
unsigned long get_window_style_mask(const app_configuration_t* config);

#endif // CONFIG_H 