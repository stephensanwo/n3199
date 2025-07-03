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

typedef struct {
    char title[64];
    char action[32];    // e.g., "navigate_to_home", "show_settings"
    bool enabled;
    bool separator_after;
} sidebar_item_config_t;

typedef struct {
    char title[64];
    sidebar_item_config_t items[16];
    int item_count;
    bool enabled;
    int width;  // Sidebar width in pixels
    int max_width;  // Maximum sidebar width in pixels (0 = no limit)
    bool resizable;
    bool collapsible;
    bool start_collapsed;
} sidebar_config_t;

typedef struct {
    bool enabled;
    bool show_toggle_button;  // Show native sidebar toggle button in toolbar
} macos_toolbar_config_t;

typedef struct {
    macos_toolbar_config_t toolbar;
    sidebar_config_t sidebar;
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

typedef struct {
    app_config_t app;
    window_config_t window;
    menubar_config_t menubar;
    webview_config_t webview;
    
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