#include "bridge.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BRIDGE_FUNCTIONS 256

// Bridge state
static bridge_function_t g_functions[MAX_BRIDGE_FUNCTIONS];
static size_t g_function_count = 0;
static app_window_t* g_bridge_window = NULL;

// Initialize the bridge system
bool bridge_init(app_window_t* window) {
    if (!window) {
        printf("Bridge init failed: No window provided\n");
        return false;
    }
    
    printf("Initializing bridge system...\n");
    g_bridge_window = window;
    g_function_count = 0;
    
    // Register built-in and custom functions
    bridge_register_builtin_functions();
    bridge_register_custom_functions();
    
    printf("Bridge initialization successful!\n");
    return true;
}

// Cleanup the bridge system
void bridge_cleanup(void) {
    printf("Cleaning up bridge system...\n");
    g_function_count = 0;
    g_bridge_window = NULL;
}

// Register a bridge function
void bridge_register(const char* name, bridge_handler_t handler, const char* description) {
    if (!name || !handler || !description) {
        printf("Bridge register failed: Invalid parameters\n");
        return;
    }
    
    if (g_function_count >= MAX_BRIDGE_FUNCTIONS) {
        printf("Bridge register failed: Maximum functions reached\n");
        return;
    }
    
    printf("Registering bridge function: %s - %s\n", name, description);
    
    strncpy(g_functions[g_function_count].name, name, sizeof(g_functions[g_function_count].name) - 1);
    g_functions[g_function_count].name[sizeof(g_functions[g_function_count].name) - 1] = '\0';
    
    g_functions[g_function_count].handler = handler;
    
    strncpy(g_functions[g_function_count].description, description, sizeof(g_functions[g_function_count].description) - 1);
    g_functions[g_function_count].description[sizeof(g_functions[g_function_count].description) - 1] = '\0';
    
    g_function_count++;
}

// Handle incoming bridge message
void bridge_handle_message(const char* json_message, app_window_t* window) {
    if (!json_message || !window) return;
    
    printf("Bridge received message: %s\n", json_message);
    
    // Extract method name and id from JSON (matching frontend format)
    char* method_name = bridge_get_string_param(json_message, "method");
    char* callback_id = bridge_get_id_param(json_message, "id");
    
    if (!method_name || !callback_id) {
        bridge_send_error(callback_id ? callback_id : "unknown", "Invalid message format", window);
        free(method_name);
        free(callback_id);
        return;
    }
    
    // Extract params using JSON value extraction
    char* params = bridge_get_json_value(json_message, "params");
    
    // Find and call the function
    bool found = false;
    for (size_t i = 0; i < g_function_count; i++) {
        if (strcmp(g_functions[i].name, method_name) == 0) {
            g_functions[i].handler(params ? params : "{}", callback_id, window);
            found = true;
            break;
        }
    }
    
    if (!found) {
        bridge_send_error(callback_id, "Function not found", window);
    }
    
    free(method_name);
    free(callback_id);
    free(params);
}

// Send success response
void bridge_send_response(const char* callback_id, const char* result, app_window_t* window) {
    if (!callback_id || !window) return;
    
    char response[1024];
    snprintf(response, sizeof(response), 
        "window.handleBridgeResponse(%s, true, %s);", 
        callback_id, result ? result : "null");
    
    platform_webview_evaluate_javascript(window, response);
}

// Send error response
void bridge_send_error(const char* callback_id, const char* error, app_window_t* window) {
    if (!callback_id || !window) return;
    
    char response[1024];
    snprintf(response, sizeof(response), 
        "window.handleBridgeResponse(%s, false, '%s');", 
        callback_id, error ? error : "Unknown error");
    
    platform_webview_evaluate_javascript(window, response);
}

// Simple JSON string extraction
char* bridge_get_string_param(const char* json_args, const char* key) {
    if (!json_args || !key) return NULL;
    
    char search_pattern[128];
    snprintf(search_pattern, sizeof(search_pattern), "\"%s\":", key);
    
    char* start = strstr(json_args, search_pattern);
    if (!start) return NULL;
    
    start += strlen(search_pattern);
    while (*start == ' ' || *start == '\t') start++; // Skip whitespace
    
    if (*start != '"') return NULL;
    start++; // Skip opening quote
    
    char* end = strchr(start, '"');
    if (!end) return NULL;
    
    size_t len = end - start;
    char* result = malloc(len + 1);
    if (!result) return NULL;
    
    strncpy(result, start, len);
    result[len] = '\0';
    
    return result;
}

// Simple JSON integer extraction
int bridge_get_int_param(const char* json_args, const char* key) {
    if (!json_args || !key) return 0;
    
    char search_pattern[128];
    snprintf(search_pattern, sizeof(search_pattern), "\"%s\":", key);
    
    char* start = strstr(json_args, search_pattern);
    if (!start) return 0;
    
    start += strlen(search_pattern);
    while (*start == ' ' || *start == '\t') start++; // Skip whitespace
    
    return atoi(start);
}

// Extract numeric ID and convert to string
char* bridge_get_id_param(const char* json_args, const char* key) {
    if (!json_args || !key) return NULL;
    
    char search_pattern[128];
    snprintf(search_pattern, sizeof(search_pattern), "\"%s\":", key);
    
    char* start = strstr(json_args, search_pattern);
    if (!start) return NULL;
    
    start += strlen(search_pattern);
    while (*start == ' ' || *start == '\t') start++; // Skip whitespace
    
    int id_value = atoi(start);
    
    char* result = malloc(32); // Enough for a large integer
    if (!result) return NULL;
    
    snprintf(result, 32, "%d", id_value);
    return result;
}

// Simple JSON boolean extraction
bool bridge_get_bool_param(const char* json_args, const char* key) {
    if (!json_args || !key) return false;
    
    char search_pattern[128];
    snprintf(search_pattern, sizeof(search_pattern), "\"%s\":", key);
    
    char* start = strstr(json_args, search_pattern);
    if (!start) return false;
    
    start += strlen(search_pattern);
    while (*start == ' ' || *start == '\t') start++; // Skip whitespace
    
    return (strncmp(start, "true", 4) == 0);
}

// Extract JSON value (string, object, array, or null)
char* bridge_get_json_value(const char* json_args, const char* key) {
    if (!json_args || !key) return NULL;
    
    char search_pattern[128];
    snprintf(search_pattern, sizeof(search_pattern), "\"%s\":", key);
    
    char* start = strstr(json_args, search_pattern);
    if (!start) return NULL;
    
    start += strlen(search_pattern);
    while (*start == ' ' || *start == '\t') start++; // Skip whitespace
    
    char* end;
    
    // Handle different JSON value types
    if (*start == '"') {
        // String value
        start++; // Skip opening quote
        end = strchr(start, '"');
        if (!end) return NULL;
    } else if (*start == '{') {
        // Object value - find matching closing brace
        int brace_count = 1;
        end = start + 1;
        while (*end && brace_count > 0) {
            if (*end == '{') brace_count++;
            else if (*end == '}') brace_count--;
            end++;
        }
        if (brace_count != 0) return NULL;
        end--; // Point to closing brace
        
        // Return the full object including braces
        size_t len = end - start + 1;
        char* result = malloc(len + 1);
        if (!result) return NULL;
        strncpy(result, start, len);
        result[len] = '\0';
        return result;
    } else if (strncmp(start, "null", 4) == 0) {
        // Null value
        return NULL;
    } else {
        // Number or boolean - find next delimiter
        end = start;
        while (*end && *end != ',' && *end != '}' && *end != ']' && *end != ' ' && *end != '\t' && *end != '\n') {
            end++;
        }
        end--; // Back up one
    }
    
    size_t len = end - start;
    char* result = malloc(len + 1);
    if (!result) return NULL;
    
    strncpy(result, start, len);
    result[len] = '\0';
    
    return result;
}

// List all registered functions
void bridge_list_functions(void) {
    printf("=== Registered Bridge Functions ===\n");
    for (size_t i = 0; i < g_function_count; i++) {
        printf("  %s - %s\n", g_functions[i].name, g_functions[i].description);
    }
    printf("===================================\n");
}

// NEW: Native to bridge function calling
bool bridge_call_function(const char* function_name, const char* json_params, app_window_t* window) {
    if (!function_name || !window) {
        printf("Bridge call failed: Invalid parameters\n");
        return false;
    }
    
    // Find the function
    for (size_t i = 0; i < g_function_count; i++) {
        if (strcmp(g_functions[i].name, function_name) == 0) {
            printf("Native bridge call: %s\n", function_name);
            // Call with a dummy callback ID since this is a native call
            g_functions[i].handler(json_params ? json_params : "{}", "native_call", window);
            return true;
        }
    }
    
    printf("Bridge call failed: Function '%s' not found\n", function_name);
    return false;
}

// NEW: Check if a bridge function exists
bool bridge_function_exists(const char* function_name) {
    if (!function_name) return false;
    
    for (size_t i = 0; i < g_function_count; i++) {
        if (strcmp(g_functions[i].name, function_name) == 0) {
            return true;
        }
    }
    
    return false;
}

// NEW: Send event to frontend (for toolbar actions that should trigger frontend events)
void bridge_send_event(const char* event_name, const char* json_data, app_window_t* window) {
    if (!event_name || !window) return;
    
    char event_js[1024];
    if (json_data && strlen(json_data) > 0) {
        snprintf(event_js, sizeof(event_js), 
            "if (window.bridge?.onNativeEvent) { window.bridge.onNativeEvent('%s', %s); }", 
            event_name, json_data);
    } else {
        snprintf(event_js, sizeof(event_js), 
            "if (window.bridge?.onNativeEvent) { window.bridge.onNativeEvent('%s'); }", 
            event_name);
    }
    
    printf("Sending native event: %s\n", event_name);
    platform_webview_evaluate_javascript(window, event_js);
}

// NEW: Toolbar action dispatcher - handles toolbar button clicks dynamically
void bridge_handle_toolbar_action(const char* action_name, app_window_t* window) {
    if (!action_name || !window) return;
    
    printf("Handling toolbar action: %s\n", action_name);
    
    // First, try to call it as a registered bridge function
    if (bridge_call_function(action_name, "{}", window)) {
        printf("Toolbar action '%s' handled by bridge function\n", action_name);
        return;
    }
    
    // If not found as bridge function, send as an event to frontend
    char event_data[256];
    snprintf(event_data, sizeof(event_data), "{\"action\":\"%s\"}", action_name);
    bridge_send_event("toolbar_action", event_data, window);
    
    printf("Toolbar action '%s' sent as frontend event\n", action_name);
} 