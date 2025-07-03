#include "bridge.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Platform-specific includes
#ifdef __APPLE__
#include <objc/objc.h>
#include <objc/message.h>
#include <objc/runtime.h>
#endif

// Window operations
static void bridge_window_set_size(const char* json_args, const char* callback_id, app_window_t* window) {
    int width = bridge_get_int_param(json_args, "width");
    int height = bridge_get_int_param(json_args, "height");
    
    if (width <= 0 || height <= 0) {
        bridge_send_error(callback_id, "Invalid window size", window);
        return;
    }
    
    printf("Window resize requested: %dx%d\n", width, height);
    // TODO: Implement platform-specific window resizing
    bridge_send_response(callback_id, "null", window);
}

static void bridge_window_minimize(const char* json_args, const char* callback_id, app_window_t* window) {
    (void)json_args; // Unused parameter
    printf("Window minimize requested\n");
    platform_hide_window(window);
    bridge_send_response(callback_id, "null", window);
}

static void bridge_window_maximize(const char* json_args, const char* callback_id, app_window_t* window) {
    (void)json_args; // Unused parameter
    printf("Window maximize requested\n");
    // TODO: Implement platform-specific window maximizing
    bridge_send_response(callback_id, "null", window);
}

static void bridge_window_restore(const char* json_args, const char* callback_id, app_window_t* window) {
    (void)json_args; // Unused parameter
    printf("Window restore requested\n");
    platform_show_window(window);
    bridge_send_response(callback_id, "null", window);
}

static void bridge_window_get_size(const char* json_args, const char* callback_id, app_window_t* window) {
    (void)json_args; // Unused parameter
    (void)window; // Unused for now
    
    // Return default size for now
    char response[256];
    snprintf(response, sizeof(response), "{\"width\":800,\"height\":600}");
    bridge_send_response(callback_id, response, window);
}

// System operations
static void bridge_system_get_platform(const char* json_args, const char* callback_id, app_window_t* window) {
    (void)json_args; // Unused parameter
    bridge_send_response(callback_id, "\"darwin\"", window);
}

static void bridge_system_get_version(const char* json_args, const char* callback_id, app_window_t* window) {
    (void)json_args; // Unused parameter
    bridge_send_response(callback_id, "\"1.0.0\"", window);
}

static void bridge_system_get_config(const char* json_args, const char* callback_id, app_window_t* window) {
    (void)json_args; // Unused parameter
    
    char response[512];
    snprintf(response, sizeof(response), 
        "{\"name\":\"Desktop App\",\"version\":\"1.0.0\",\"debug\":true}");
    bridge_send_response(callback_id, response, window);
}

// Sidebar operations (formerly drawer operations)
static void bridge_sidebar_toggle(const char* json_args, const char* callback_id, app_window_t* window) {
    (void)json_args; // Unused parameter
    
    if (!window->config->macos.sidebar.enabled) {
        bridge_send_error(callback_id, "Sidebar not enabled", window);
        return;
    }
    
    printf("Sidebar toggle requested\n");
    platform_handle_sidebar_action("toggle", window);
    bridge_send_response(callback_id, "null", window);
}

static void bridge_sidebar_show(const char* json_args, const char* callback_id, app_window_t* window) {
    (void)json_args; // Unused parameter
    
    if (!window->config->macos.sidebar.enabled) {
        bridge_send_error(callback_id, "Sidebar not enabled", window);
        return;
    }
    
    printf("Sidebar show requested\n");
    platform_handle_sidebar_action("show", window);
    bridge_send_response(callback_id, "null", window);
}

static void bridge_sidebar_hide(const char* json_args, const char* callback_id, app_window_t* window) {
    (void)json_args; // Unused parameter
    
    if (!window->config->macos.sidebar.enabled) {
        bridge_send_error(callback_id, "Sidebar not enabled", window);
        return;
    }
    
    printf("Sidebar hide requested\n");
    platform_handle_sidebar_action("hide", window);
    bridge_send_response(callback_id, "null", window);
}

static void bridge_sidebar_get_state(const char* json_args, const char* callback_id, app_window_t* window) {
    (void)json_args; // Unused parameter
    
    if (!window->config->macos.sidebar.enabled) {
        bridge_send_error(callback_id, "Sidebar not enabled", window);
        return;
    }
    
    // Get sidebar state from platform layer
    #ifdef __APPLE__
    if (window->native_window) {
        platform_native_window_t* native = (platform_native_window_t*)window->native_window;
        char response[64];
        snprintf(response, sizeof(response), "{\"visible\":%s}", 
                 native->sidebar_visible ? "true" : "false");
        bridge_send_response(callback_id, response, window);
        return;
    }
    #endif
    
    // Default response if platform doesn't support getting state
    bridge_send_response(callback_id, "{\"visible\":false}", window);
}

// Register all built-in bridge functions
void bridge_register_builtin_functions(void) {
    // Window functions
    bridge_register("window.setSize", bridge_window_set_size, "Set window size");
    bridge_register("window.minimize", bridge_window_minimize, "Minimize window");
    bridge_register("window.maximize", bridge_window_maximize, "Maximize window");
    bridge_register("window.restore", bridge_window_restore, "Restore window");
    bridge_register("window.getSize", bridge_window_get_size, "Get window size");
    
    // System functions
    bridge_register("system.getPlatform", bridge_system_get_platform, "Get platform name");
    bridge_register("system.getVersion", bridge_system_get_version, "Get application version");
    bridge_register("system.getConfig", bridge_system_get_config, "Get application configuration");
    
    // Sidebar functions (modern API)
    bridge_register("sidebar.toggle", bridge_sidebar_toggle, "Toggle sidebar");
    bridge_register("sidebar.show", bridge_sidebar_show, "Show sidebar");
    bridge_register("sidebar.hide", bridge_sidebar_hide, "Hide sidebar");
    bridge_register("sidebar.getState", bridge_sidebar_get_state, "Get sidebar state");
} 