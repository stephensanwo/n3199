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

// UI operations
static void bridge_ui_show_alert(const char* json_args, const char* callback_id, app_window_t* window) {
    printf("UI: Show alert requested\n");
    
    // Parse parameters from JSON, using defaults if not provided
    char* title = NULL;
    char* message = NULL;
    char* ok_button = NULL;
    char* cancel_button = NULL;
    
    if (json_args && strlen(json_args) > 2) { // More than just "{}"
        title = bridge_get_string_param(json_args, "title");
        message = bridge_get_string_param(json_args, "message");
        ok_button = bridge_get_string_param(json_args, "okButton");
        cancel_button = bridge_get_string_param(json_args, "cancelButton");
    }
    
    // Call flexible platform function with parameters (will use defaults if params are NULL)
    bool result = platform_show_alert_with_params(window, title, message, ok_button, cancel_button);
    
    // Free allocated strings
    if (title) free(title);
    if (message) free(message);
    if (ok_button) free(ok_button);
    if (cancel_button) free(cancel_button);
    
    // Send response based on result
    if (result) {
        bridge_send_response(callback_id, "true", window);
    } else {
        bridge_send_response(callback_id, "false", window);
    }
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
    
    // UI functions
    bridge_register("ui.showAlert", bridge_ui_show_alert, "Show native alert dialog");
    
    // Streaming functions
    bridge_register("streaming.getConfig", bridge_streaming_get_config, "Get streaming configuration");
    bridge_register("streaming.getServerUrl", bridge_streaming_get_server_url, "Get streaming server URL");
} 