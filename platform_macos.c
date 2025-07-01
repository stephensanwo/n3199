#ifdef __APPLE__

#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <objc/objc.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <CoreGraphics/CoreGraphics.h>

// macOS-specific window structure
struct platform_window {
    id ns_window;
    id ns_app;
    id ns_toolbar;
    id window_delegate;
};

// Global app reference
static id g_app = NULL;

int platform_init(void) {
    if (g_app != NULL) return 0; // Already initialized
    
    Class NSApp = objc_getClass("NSApplication");
    g_app = ((id (*)(id, SEL))objc_msgSend)((id)NSApp, sel_registerName("sharedApplication"));
    
    // Set activation policy to regular app
    ((void (*)(id, SEL, long))objc_msgSend)(g_app, sel_registerName("setActivationPolicy:"), 0);
    
    printf("macOS platform initialized\n");
    return 0;
}

void platform_cleanup(void) {
    g_app = NULL;
    printf("macOS platform cleaned up\n");
}

app_window_t* platform_create_window(app_configuration_t* config) {
    if (!config) return NULL;
    
    // Ensure platform is initialized
    if (platform_init() != 0) return NULL;
    
    app_window_t* window = malloc(sizeof(app_window_t));
    platform_window_t* native = malloc(sizeof(platform_window_t));
    
    if (!window || !native) {
        free(window);
        free(native);
        return NULL;
    }
    
    window->native_window = native;
    window->config = config;
    window->is_visible = false;
    window->is_focused = false;
    
    native->ns_app = g_app;
    native->ns_toolbar = NULL;
    native->window_delegate = NULL;
    
    // Create window frame
    CGRect frame = CGRectMake(100, 100, config->window.width, config->window.height);
    
    // Get window style mask from configuration
    unsigned long style_mask = get_window_style_mask(config);
    
    if (config->development.debug_mode) {
        printf("Creating window with style mask: 0x%lx\n", style_mask);
        printf("Window size: %dx%d\n", config->window.width, config->window.height);
    }
    
    // Create NSWindow
    Class NSWindow = objc_getClass("NSWindow");
    Class NSString = objc_getClass("NSString");
    
    id ns_window = ((id (*)(id, SEL))objc_msgSend)((id)NSWindow, sel_registerName("alloc"));
    
    ns_window = ((id (*)(id, SEL, CGRect, unsigned long, unsigned long, int))objc_msgSend)(
        ns_window,
        sel_registerName("initWithContentRect:styleMask:backing:defer:"),
        frame,
        style_mask,
        2, // NSBackingStoreBuffered
        0  // defer = NO
    );
    
    native->ns_window = ns_window;
    
    // Set window title
    id title = ((id (*)(id, SEL, const char*))objc_msgSend)(
        (id)NSString,
        sel_registerName("stringWithUTF8String:"),
        config->window.title
    );
    ((void (*)(id, SEL, id))objc_msgSend)(ns_window, sel_registerName("setTitle:"), title);
    
    // Set minimum size if specified
    if (config->window.min_width > 0 && config->window.min_height > 0) {
        CGSize minSize = {config->window.min_width, config->window.min_height};
        ((void (*)(id, SEL, CGSize))objc_msgSend)(ns_window, sel_registerName("setMinSize:"), minSize);
    }
    
    // Setup toolbar if enabled
    if (config->macos.toolbar.enabled) {
        platform_macos_setup_toolbar(window);
    }
    
    if (config->development.debug_mode) {
        printf("macOS window created successfully\n");
    }
    
    return window;
}

void platform_macos_setup_toolbar(app_window_t* window) {
    if (!window || !window->native_window) return;
    
    platform_window_t* native = window->native_window;
    Class NSToolbar = objc_getClass("NSToolbar");
    Class NSString = objc_getClass("NSString");
    
    // Create toolbar identifier
    id identifier = ((id (*)(id, SEL, const char*))objc_msgSend)(
        (id)NSString,
        sel_registerName("stringWithUTF8String:"),
        "MainToolbar"
    );
    
    // Create toolbar
    id toolbar = ((id (*)(id, SEL))objc_msgSend)((id)NSToolbar, sel_registerName("alloc"));
    toolbar = ((id (*)(id, SEL, id))objc_msgSend)(toolbar, sel_registerName("initWithIdentifier:"), identifier);
    
    native->ns_toolbar = toolbar;
    
    // Set toolbar on window
    ((void (*)(id, SEL, id))objc_msgSend)(native->ns_window, sel_registerName("setToolbar:"), toolbar);
    
    if (window->config->development.debug_mode) {
        printf("macOS toolbar configured\n");
    }
}

void platform_macos_add_toolbar_item(app_window_t* window, const char* identifier, const char* title) {
    if (!window || !window->native_window || !window->native_window->ns_toolbar) return;
    
    // This is a simplified version - in a full implementation, you'd need to
    // set up a proper NSToolbarDelegate to handle toolbar items
    if (window->config->development.debug_mode) {
        printf("Adding toolbar item: %s (%s)\n", identifier, title);
    }
}

void platform_show_window(app_window_t* window) {
    if (!window || !window->native_window) return;
    
    platform_window_t* native = window->native_window;
    
    // Center window if requested
    if (window->config->window.center) {
        ((void (*)(id, SEL))objc_msgSend)(native->ns_window, sel_registerName("center"));
    }
    
    // Show window
    ((void (*)(id, SEL, id))objc_msgSend)(
        native->ns_window,
        sel_registerName("makeKeyAndOrderFront:"),
        native->ns_window
    );
    
    // Activate app
    ((void (*)(id, SEL, int))objc_msgSend)(native->ns_app, sel_registerName("activateIgnoringOtherApps:"), 1);
    
    window->is_visible = true;
    window->is_focused = true;
    
    if (window->config->development.debug_mode) {
        printf("Window shown and activated\n");
    }
}

void platform_hide_window(app_window_t* window) {
    if (!window || !window->native_window) return;
    
    ((void (*)(id, SEL, id))objc_msgSend)(window->native_window->ns_window, sel_registerName("orderOut:"), window->native_window->ns_window);
    window->is_visible = false;
}

void platform_set_window_title(app_window_t* window, const char* title) {
    if (!window || !window->native_window || !title) return;
    
    Class NSString = objc_getClass("NSString");
    id ns_title = ((id (*)(id, SEL, const char*))objc_msgSend)(
        (id)NSString,
        sel_registerName("stringWithUTF8String:"),
        title
    );
    
    ((void (*)(id, SEL, id))objc_msgSend)(
        window->native_window->ns_window,
        sel_registerName("setTitle:"),
        ns_title
    );
}

void platform_set_window_size(app_window_t* window, int width, int height) {
    if (!window || !window->native_window) return;
    
    CGRect frame = CGRectMake(0, 0, width, height);
    ((void (*)(id, SEL, CGRect, int))objc_msgSend)(
        window->native_window->ns_window,
        sel_registerName("setFrame:display:"),
        frame,
        1
    );
}

void platform_center_window(app_window_t* window) {
    if (!window || !window->native_window) return;
    
    ((void (*)(id, SEL))objc_msgSend)(window->native_window->ns_window, sel_registerName("center"));
}

void platform_run_app(app_window_t* window) {
    if (!window || !window->native_window) return;
    
    if (window->config->development.debug_mode) {
        printf("Starting macOS application event loop...\n");
        printf("Press Cmd+Q or close the window to quit.\n");
    }
    
    // Run the application event loop
    ((void (*)(id, SEL))objc_msgSend)(window->native_window->ns_app, sel_registerName("run"));
}

void platform_quit_app(void) {
    if (g_app) {
        ((void (*)(id, SEL, id))objc_msgSend)(g_app, sel_registerName("terminate:"), g_app);
    }
}

void platform_destroy_window(app_window_t* window) {
    if (!window) return;
    
    if (window->native_window) {
        // Close and release native window
        if (window->native_window->ns_window) {
            ((void (*)(id, SEL))objc_msgSend)(window->native_window->ns_window, sel_registerName("close"));
        }
        
        free(window->native_window);
    }
    
    free(window);
    
    if (window->config && window->config->development.debug_mode) {
        printf("Window destroyed\n");
    }
}

#endif // __APPLE__ 