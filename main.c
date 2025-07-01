#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "config.h"
#include "platform.h"

// Global window reference for signal handling
static app_window_t* g_main_window = NULL;

static void cleanup(void) {
    if (g_main_window) {
        platform_close_window(g_main_window);
        free(g_main_window);
    }
    platform_cleanup();
}

static void signal_handler(int signum) {
    printf("\nReceived signal %d, shutting down gracefully...\n", signum);
    cleanup();
    exit(0);
}

int main(int argc, char* argv[]) {
    // Register signal handlers
    signal(SIGINT, signal_handler);  // Ctrl+C
    signal(SIGTERM, signal_handler); // Termination request
    
    printf("=== C Desktop Application Framework ===\n");
    printf("Platform: %s\n", PLATFORM_NAME);
    printf("Version: 1.0.0\n\n");
    
    // Load configuration
    const char* config_file = (argc > 1) ? argv[1] : "config.json";
    printf("Loading configuration from: %s\n", config_file);
    
    app_configuration_t* app_config = load_config(config_file);
    if (!app_config) {
        printf("Failed to load configuration\n");
        return 1;
    }
    
    print_config(app_config);
    
    // Initialize platform
    if (!platform_init(app_config)) {
        printf("Failed to initialize platform\n");
        free_config(app_config);
        return 1;
    }
    
    // Create main window
    g_main_window = malloc(sizeof(app_window_t));
    if (!g_main_window) {
        printf("Failed to allocate window structure\n");
        platform_cleanup();
        free_config(app_config);
        return 1;
    }
    
    // Initialize window structure
    g_main_window->config = app_config;
    g_main_window->native_window = NULL;
    
    // Create and show window
    if (!platform_create_window(g_main_window)) {
        printf("Failed to create window\n");
        free(g_main_window);
        platform_cleanup();
        free_config(app_config);
        return 1;
    }
    
    platform_show_window(g_main_window);
    
    // Setup webview if enabled
    if (app_config->webview.enabled) {
        platform_setup_webview(g_main_window);
        if (app_config->development.debug_mode) {
            printf("WebView setup completed\n");
        }
    }
    
    // Setup menubar if enabled
    if (app_config->menubar.enabled) {
        platform_setup_menubar(g_main_window);
        if (app_config->development.debug_mode) {
            printf("Menubar setup completed\n");
        }
    }
    
    // Add some demo toolbar items if toolbar is enabled
    #ifdef PLATFORM_MACOS
    if (app_config->macos.toolbar.enabled) {
        platform_macos_add_toolbar_item(g_main_window, "refresh", "Refresh");
        platform_macos_add_toolbar_item(g_main_window, "settings", "Settings");
    }
    #endif
    
    printf("Starting application event loop...\n");
    printf("Close the window or press Ctrl+C to quit.\n\n");
    
    // Run event loop
    platform_run_event_loop();
    
    // Cleanup
    cleanup();
    free_config(app_config);
    
    return 0;
} 