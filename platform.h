#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include "config.h"
#include "webview_framework.h"

// Forward declarations for platform-specific window handles
typedef void* platform_window_handle_t;
typedef void* platform_webview_handle_t;

// Window structure
typedef struct {
    app_configuration_t* config;
    platform_window_handle_t native_window;
    platform_webview_handle_t webview;
} app_window_t;

// Platform initialization and cleanup
bool platform_init(const app_configuration_t* config);
void platform_cleanup(void);

// Window management
bool platform_create_window(app_window_t* window);
void platform_show_window(app_window_t* window);
void platform_hide_window(app_window_t* window);
void platform_close_window(app_window_t* window);

// WebView management
void platform_setup_webview(app_window_t* window);
void platform_webview_load_url(app_window_t* window, const char* url);
void platform_webview_evaluate_javascript(app_window_t* window, const char* script);

// Menu management
void platform_setup_menubar(app_window_t* window);
void platform_handle_menu_action(const char* action);

// Event loop
void platform_run_event_loop(void);

// Global configuration
extern app_configuration_t* app_config;

// Toolbar management (platform-specific)
#ifdef PLATFORM_MACOS
void platform_macos_setup_toolbar(app_window_t* window);
void platform_macos_add_toolbar_item(app_window_t* window, const char* identifier, const char* title);
#endif

#ifdef PLATFORM_WINDOWS
void platform_windows_setup_toolbar(app_window_t* window);
#endif

#ifdef PLATFORM_LINUX
void platform_linux_setup_toolbar(app_window_t* window);
#endif

// WebView functions
void platform_webview_load_html(app_window_t* window, const char* html);

#endif // PLATFORM_H 