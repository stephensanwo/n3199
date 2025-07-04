#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include "config.h"
#include "webview_framework.h"

#ifdef __APPLE__
// macOS-specific includes and type definitions
#include <CoreGraphics/CoreGraphics.h>
#include <objc/objc.h>

// Type definitions for Objective-C types (using system definitions)
typedef long NSInteger;
typedef unsigned long NSUInteger;

// Platform-specific window structure for macOS using modern APIs
typedef struct {
    id ns_app;
    id ns_window;
    id webview;                     // WKWebView (main content view)
    id webview_config;             // WKWebViewConfiguration
    id script_handler;             // WKScriptMessageHandler
    id toolbar;                    // NSToolbar
} platform_native_window_t;
#else
// Platform-agnostic fallback
typedef struct {
    void* native_handle;
} platform_native_window_t;
#endif

// Forward declarations for platform-specific window handles
typedef void* platform_window_handle_t;
typedef void* platform_webview_handle_t;

// Window structure
typedef struct {
    app_configuration_t* config;
    platform_window_handle_t native_window;
    platform_webview_handle_t webview;
} app_window_t;

// Global window reference
extern app_window_t* g_main_window;

// Platform initialization and cleanup
bool platform_init(const app_configuration_t* app_config);
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

#ifdef PLATFORM_WINDOWS
void platform_windows_setup_toolbar(app_window_t* window);
#endif

#ifdef PLATFORM_LINUX
void platform_linux_setup_toolbar(app_window_t* window);
#endif

// WebView functions
void platform_webview_load_html(app_window_t* window, const char* html);

// Platform-specific UI functions
bool platform_show_alert_with_params(app_window_t* window, const char* title, const char* message, const char* ok_button, const char* cancel_button);

// Direct C function for native calls (no bridge involved)
bool platform_show_alert_direct(app_window_t* window, const char* title, const char* message, const char* ok_button, const char* cancel_button);

#endif // PLATFORM_H 