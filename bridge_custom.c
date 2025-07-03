#include "bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
#include "platform.h"
#include <objc/message.h>
#include <objc/objc.h>
#include <objc/runtime.h>
#endif

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

// Toolbar action implementations as bridge functions
static void bridge_toolbar_back(const char *json_args, const char *callback_id, app_window_t *window) {
  (void)json_args;
  (void)callback_id; // Unused parameters

  printf("Toolbar: Back button clicked\n");

  if (window && window->native_window) {
    platform_native_window_t *native =
        (platform_native_window_t *)window->native_window;
    if (native->webview) {
      // Go back in webview
      ((void (*)(id, SEL))objc_msgSend)(native->webview,
                                        sel_registerName("goBack"));
      printf("WebView navigated back\n");
    }
  }
}

static void bridge_toolbar_forward(const char *json_args, const char *callback_id, app_window_t *window) {
  (void)json_args;
  (void)callback_id; // Unused parameters

  printf("Toolbar: Forward button clicked\n");

  if (window && window->native_window) {
    platform_native_window_t *native =
        (platform_native_window_t *)window->native_window;
    if (native->webview) {
      // Go forward in webview
      ((void (*)(id, SEL))objc_msgSend)(native->webview,
                                        sel_registerName("goForward"));
      printf("WebView navigated forward\n");
    }
  }
}

static void bridge_toolbar_refresh(const char *json_args, const char *callback_id, app_window_t *window) {
  (void)json_args;
  (void)callback_id; // Unused parameters

  printf("Toolbar: Refresh button clicked\n");

  if (window && window->native_window) {
    platform_native_window_t *native =
        (platform_native_window_t *)window->native_window;
    if (native->webview) {
      // Reload the webview
      ((void (*)(id, SEL))objc_msgSend)(native->webview,
                                        sel_registerName("reload"));
      printf("WebView refreshed\n");
    }
  }
}

static void bridge_toolbar_star(const char *json_args, const char *callback_id, app_window_t *window) {
  (void)json_args;
  (void)callback_id; // Unused parameters

  printf("Toolbar: Star button clicked\n");

  // Send event to frontend to handle favorites
  bridge_send_event("toggle_favorites", NULL, window);
}

static void bridge_toolbar_search(const char *json_args, const char *callback_id, app_window_t *window) {
  (void)json_args;
  (void)callback_id; // Unused parameters

  printf("Toolbar: Search button clicked\n");

  // Send event to frontend to open search
  bridge_send_event("open_search", NULL, window);
}

static void bridge_toolbar_settings(const char *json_args, const char *callback_id, app_window_t *window) {
  (void)json_args;
  (void)callback_id; // Unused parameters

  printf("Toolbar: Settings button clicked\n");

  // Send event to frontend to open settings
  bridge_send_event("open_settings", NULL, window);
}

static void bridge_toolbar_share(const char *json_args, const char *callback_id, app_window_t *window) {
  (void)json_args;
  (void)callback_id; // Unused parameters

  printf("Toolbar: Share button clicked\n");

  // Send event to frontend to show share options
  bridge_send_event("show_share", NULL, window);
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

    // Toolbar action handlers - these can be called from toolbar buttons
    bridge_register("toolbar_back_callback", bridge_toolbar_back,
                    "Navigate back in webview");
    bridge_register("toolbar_forward_callback", bridge_toolbar_forward,
                    "Navigate forward in webview");
    bridge_register("toolbar_refresh_callback", bridge_toolbar_refresh,
                    "Refresh webview content");
    bridge_register("toolbar_star_callback", bridge_toolbar_star,
                    "Toggle favorites");
    bridge_register("toolbar_search_callback", bridge_toolbar_search,
                    "Open search interface");
    bridge_register("toolbar_settings_callback", bridge_toolbar_settings,
                    "Open settings panel");
    bridge_register("toolbar_share_callback", bridge_toolbar_share,
                    "Share current content");
} 