#ifndef BRIDGE_H
#define BRIDGE_H

#include <stdbool.h>
#include <stddef.h>
#include "platform.h"

// Constants
#define MAX_BRIDGE_FUNCTIONS 256

// Bridge function handler type
typedef void (*bridge_handler_t)(const char* json_args, const char* callback_id, app_window_t* window);

// Bridge function registry entry
typedef struct {
    char name[128];
    bridge_handler_t handler;
    char description[256];
} bridge_function_t;

// Bridge initialization and cleanup
bool bridge_init(app_window_t* window);
void bridge_cleanup(void);

// Function registration
void bridge_register(const char* name, bridge_handler_t handler, const char* description);
void bridge_register_builtin_functions(void);
void bridge_register_custom_functions(void);
void bridge_list_functions(void);

// Message handling
void bridge_handle_message(const char* json_message, app_window_t* window);

// Native to bridge calling (NEW - for bidirectional communication)
bool bridge_call_function(const char* function_name, const char* json_params, app_window_t* window);
bool bridge_function_exists(const char* function_name);
void bridge_send_event(const char* event_name, const char* json_data, app_window_t* window);

// Toolbar action dispatcher (NEW - for dynamic toolbar callbacks)
void bridge_handle_toolbar_action(const char* action_name, app_window_t* window);

// Utility functions for handlers
void bridge_send_response(const char* callback_id, const char* result, app_window_t* window);
void bridge_send_error(const char* callback_id, const char* error, app_window_t* window);

// NEW: Streaming bridge functions
void bridge_streaming_get_config(const char* json_args, const char* callback_id, app_window_t* window);
void bridge_streaming_get_server_url(const char* json_args, const char* callback_id, app_window_t* window);
bool bridge_streaming_register_function(const char* name, const char* endpoint, int interval_ms, const char* description);

// JSON helper functions
char* bridge_get_string_param(const char* json_args, const char* key);
int bridge_get_int_param(const char* json_args, const char* key);
char* bridge_get_id_param(const char* json_args, const char* key);
char* bridge_get_json_value(const char* json_args, const char* key);
bool bridge_get_bool_param(const char* json_args, const char* key);

#endif // BRIDGE_H 