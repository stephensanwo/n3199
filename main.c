#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "config.h"
#include "platform.h"

// Global window reference for signal handling
static app_window_t* g_main_window = NULL;

void signal_handler(int signal) {
    printf("\nReceived signal %d, shutting down gracefully...\n", signal);
    
    if (g_main_window) {
        platform_destroy_window(g_main_window);
        g_main_window = NULL;
    }
    
    platform_cleanup();
    exit(0);
}

void setup_signal_handlers() {
    signal(SIGINT, signal_handler);   // Ctrl+C
    signal(SIGTERM, signal_handler);  // Termination signal
}

int main(int argc, char* argv[]) {
    printf("=== C Desktop Application Framework ===\n");
    printf("Platform: %s\n", PLATFORM_NAME);
    printf("Version: 1.0.0\n\n");
    
    // Setup signal handlers for graceful shutdown
    setup_signal_handlers();
    
    // Load configuration
    const char* config_file = "config.json";
    if (argc > 1) {
        config_file = argv[1];
    }
    
    printf("Loading configuration from: %s\n", config_file);
    app_configuration_t* config = load_config(config_file);
    
    if (!config) {
        printf("Error: Failed to load configuration\n");
        return 1;
    }
    
    // Print configuration if in debug mode
    if (config->development.debug_mode) {
        print_config(config);
    }
    
    // Initialize platform
    if (platform_init() < 0) {  // Only fail on actual error (-1)
        printf("Error: Failed to initialize platform\n");
        free_config(config);
        return 1;
    }
    
    // Create main window
    printf("Creating application window...\n");
    app_window_t* window = platform_create_window(config);
    g_main_window = window;
    
    if (!window) {
        printf("Error: Failed to create window\n");
        platform_cleanup();
        free_config(config);
        return 1;
    }
    
    // Show the window
    printf("Showing window...\n");
    platform_show_window(window);
    
    // Setup menubar if enabled
    if (config->menubar.enabled) {
        platform_setup_menubar(window);
        if (config->development.debug_mode) {
            printf("Menubar setup completed\n");
        }
    }
    
    // Add some demo toolbar items if toolbar is enabled
    #ifdef PLATFORM_MACOS
    if (config->macos.toolbar.enabled) {
        platform_macos_add_toolbar_item(window, "refresh", "Refresh");
        platform_macos_add_toolbar_item(window, "settings", "Settings");
    }
    #endif
    
    printf("Application started successfully!\n");
    printf("Window configuration:\n");
    printf("  - Title: %s\n", config->window.title);
    printf("  - Size: %dx%d\n", config->window.width, config->window.height);
    printf("  - Resizable: %s\n", config->window.resizable ? "Yes" : "No");
    
    #ifdef PLATFORM_MACOS
    printf("  - Toolbar: %s\n", config->macos.toolbar.enabled ? "Enabled" : "Disabled");
    #endif
    
    printf("\nStarting application event loop...\n");
    printf("Close the window or press Ctrl+C to quit.\n\n");
    
    // Run the application event loop
    platform_run_app(window);
    
    // Cleanup (this will only be reached if the app quits normally)
    printf("Application shutting down...\n");
    platform_destroy_window(window);
    platform_cleanup();
    free_config(config);
    
    return 0;
} 