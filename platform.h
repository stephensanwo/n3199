#ifndef PLATFORM_H
#define PLATFORM_H

#include "config.h"

// Forward declarations for platform-specific window handles
typedef struct platform_window platform_window_t;

// Platform-independent window management interface
typedef struct {
    platform_window_t* native_window;
    app_configuration_t* config;
    bool is_visible;
    bool is_focused;
} app_window_t;

// Window creation and management functions
app_window_t* platform_create_window(app_configuration_t* config);
void platform_show_window(app_window_t* window);
void platform_hide_window(app_window_t* window);
void platform_set_window_title(app_window_t* window, const char* title);
void platform_set_window_size(app_window_t* window, int width, int height);
void platform_center_window(app_window_t* window);
void platform_destroy_window(app_window_t* window);

// Application lifecycle
void platform_run_app(app_window_t* window);
void platform_quit_app(void);

// Platform-specific initialization
int platform_init(void);
void platform_cleanup(void);

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

#endif // PLATFORM_H 