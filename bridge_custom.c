#include "bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global counter state
static int g_counter = 0;

// Counter functions
static void bridge_counter_get(const char* json_args, const char* callback_id, app_window_t* window) {
    (void)json_args; // Unused parameter
    
    char response[64];
    snprintf(response, sizeof(response), "%d", g_counter);
    bridge_send_response(callback_id, response, window);
}

static void bridge_counter_increment(const char* json_args, const char* callback_id, app_window_t* window) {
    (void)json_args; // Unused parameter
    
    g_counter++;
    printf("Counter incremented to: %d\n", g_counter);
    
    char response[64];
    snprintf(response, sizeof(response), "%d", g_counter);
    bridge_send_response(callback_id, response, window);
}

static void bridge_counter_decrement(const char* json_args, const char* callback_id, app_window_t* window) {
    (void)json_args; // Unused parameter
    
    g_counter--;
    printf("Counter decremented to: %d\n", g_counter);
    
    char response[64];
    snprintf(response, sizeof(response), "%d", g_counter);
    bridge_send_response(callback_id, response, window);
}

static void bridge_counter_reset(const char* json_args, const char* callback_id, app_window_t* window) {
    (void)json_args; // Unused parameter
    
    g_counter = 0;
    printf("Counter reset to: %d\n", g_counter);
    
    char response[64];
    snprintf(response, sizeof(response), "%d", g_counter);
    bridge_send_response(callback_id, response, window);
}

// Demo functions
static void bridge_demo_greet(const char* json_args, const char* callback_id, app_window_t* window) {
    char* name = bridge_get_string_param(json_args, "name");
    
    if (!name) {
        bridge_send_error(callback_id, "Name parameter is required", window);
        return;
    }
    
    char response[256];
    snprintf(response, sizeof(response), "\"Hello, %s! Greetings from C!\"", name);
    
    printf("Greeting: Hello, %s!\n", name);
    
    bridge_send_response(callback_id, response, window);
    free(name);
}

static void bridge_demo_calculate(const char* json_args, const char* callback_id, app_window_t* window) {
    int a = bridge_get_int_param(json_args, "a");
    int b = bridge_get_int_param(json_args, "b");
    char* operation = bridge_get_string_param(json_args, "operation");
    
    if (!operation) {
        bridge_send_error(callback_id, "Operation parameter is required", window);
        return;
    }
    
    int result = 0;
    bool valid = true;
    
    if (strcmp(operation, "add") == 0) {
        result = a + b;
    } else if (strcmp(operation, "subtract") == 0) {
        result = a - b;
    } else if (strcmp(operation, "multiply") == 0) {
        result = a * b;
    } else if (strcmp(operation, "divide") == 0) {
        if (b == 0) {
            bridge_send_error(callback_id, "Division by zero", window);
            free(operation);
            return;
        }
        result = a / b;
    } else {
        valid = false;
    }
    
    if (!valid) {
        bridge_send_error(callback_id, "Invalid operation", window);
    } else {
        char response[64];
        snprintf(response, sizeof(response), "%d", result);
        bridge_send_response(callback_id, response, window);
    }
    
    free(operation);
}

// Register all custom bridge functions
void bridge_register_custom_functions(void) {
    // Counter functions
    bridge_register("counter.getValue", bridge_counter_get, "Get current counter value");
    bridge_register("counter.increment", bridge_counter_increment, "Increment counter");
    bridge_register("counter.decrement", bridge_counter_decrement, "Decrement counter");
    bridge_register("counter.reset", bridge_counter_reset, "Reset counter to zero");
    
    // Demo functions
    bridge_register("demo.greet", bridge_demo_greet, "Greet user by name");
    bridge_register("demo.calculate", bridge_demo_calculate, "Perform calculation");
} 