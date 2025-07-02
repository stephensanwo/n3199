#ifdef __APPLE__

#include <objc/objc.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <CoreGraphics/CoreGraphics.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "platform.h"
#include "config.h"
#include "webview_framework.h"
#include "bridge.h"

// Objective-C runtime
#include <objc/runtime.h>
#include <objc/message.h>

// macOS frameworks
#include <CoreGraphics/CoreGraphics.h>

// Global application state
static id g_app = nil;
static const app_configuration_t* stored_config = NULL;

// Function declarations
id create_script_message_handler(app_window_t* window);

// Type definitions for Objective-C types
typedef long NSInteger;
typedef unsigned long NSUInteger;

// Platform-specific window structure
typedef struct {
    id ns_app;
    id ns_window;
    id ns_toolbar;
    id window_delegate;
    id webview;
    id webview_config;
    id script_handler;
} platform_native_window_t;

bool platform_init(const app_configuration_t* app_config) {
    if (!app_config) return false;
    
    // Store config for later use
    stored_config = app_config;
    
    // Initialize NSApplication
    Class NSApplication = objc_getClass("NSApplication");
    if (!NSApplication) {
        printf("Failed to get NSApplication class\n");
        return false;
    }
    
    g_app = ((id (*)(id, SEL))objc_msgSend)((id)NSApplication, sel_registerName("sharedApplication"));
    if (!g_app) {
        printf("Failed to create NSApplication instance\n");
        return false;
    }
    
    // Set activation policy to regular app (shows in dock)
    ((void (*)(id, SEL, long))objc_msgSend)(g_app, sel_registerName("setActivationPolicy:"), 0); // NSApplicationActivationPolicyRegular
    
    printf("macOS platform initialized\n");
    
    // If webview is enabled, initialize the framework
    if (app_config->webview.enabled) {
        printf("\nInitializing webview framework...\n");
        
        // Check if webview directory exists
        if (access("webview", F_OK) != 0) {
            printf("Error: webview directory not found. Please create your project in the 'webview' directory.\n");
            return false;
        }
        
        // Build the project first
        printf("Building project...\n");
        if (!run_build_command(&app_config->webview.framework)) {
            printf("Build failed\n");
            return false;
        }
        
        // Start development server if in dev mode
        if (app_config->webview.framework.dev_mode) {
            if (!start_dev_server(&app_config->webview.framework)) {
                printf("Failed to start development server\n");
                return false;
            }
        }
        
        printf("Webview framework initialized successfully\n");
    }
    
    return true;
}

void platform_cleanup(void) {
    // Stop dev server if running
    stop_dev_server();
    
    // Cleanup platform resources
    g_app = nil;
    printf("macOS platform cleaned up\n");
}

bool platform_create_window(app_window_t* window) {
    if (!window) return false;
    
    // Create native window structure
    platform_native_window_t* native = malloc(sizeof(platform_native_window_t));
    if (!native) {
        printf("Failed to allocate native window structure\n");
        return false;
    }
    
    // Initialize native window structure
    memset(native, 0, sizeof(platform_native_window_t));
    native->ns_app = g_app;
    
    // Store native window in app_window structure
    window->native_window = native;
    
    // Create window style mask based on configuration
    unsigned long style_mask = get_window_style_mask(window->config);
    printf("Creating window with style mask: 0x%lx\n", style_mask);
    
    // Create window rect
    CGRect frame = CGRectMake(0, 0, 
                             window->config->window.width,
                             window->config->window.height);
    printf("Window size: %dx%d\n", window->config->window.width, window->config->window.height);
    
    // Get NSWindow class
    Class NSWindow = objc_getClass("NSWindow");
    if (!NSWindow) {
        printf("Failed to get NSWindow class\n");
        free(native);
        return false;
    }
    
    // Create window
    id ns_window = ((id (*)(id, SEL))objc_msgSend)((id)NSWindow, sel_registerName("alloc"));
    ns_window = ((id (*)(id, SEL, CGRect, unsigned long, unsigned long, BOOL))objc_msgSend)(
        ns_window,
        sel_registerName("initWithContentRect:styleMask:backing:defer:"),
        frame, style_mask, 2, NO  // 2 = NSBackingStoreBuffered
    );
    
    if (!ns_window) {
        printf("Failed to create NSWindow\n");
        free(native);
        return false;
    }
    
    // Set window properties
    ((void (*)(id, SEL, BOOL))objc_msgSend)(ns_window, sel_registerName("setReleasedWhenClosed:"), NO);
    
    // Set window title
    id title = ((id (*)(id, SEL, const char*))objc_msgSend)(
        (id)objc_getClass("NSString"),
        sel_registerName("stringWithUTF8String:"),
        window->config->window.title
    );
    ((void (*)(id, SEL, id))objc_msgSend)(ns_window, sel_registerName("setTitle:"), title);
    
    // Store window in native structure
    native->ns_window = ns_window;
    
    printf("macOS window created successfully\n");
    return true;
}

void platform_macos_setup_toolbar(app_window_t* window) {
    if (!window || !window->native_window) return;
    
    platform_native_window_t* native = (platform_native_window_t*)window->native_window;
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
    if (!window || !window->native_window) return;
    
    platform_native_window_t* native = (platform_native_window_t*)window->native_window;
    if (!native->ns_toolbar) return;
    
    // This is a simplified version - in a full implementation, you'd need to
    // set up a proper NSToolbarDelegate to handle toolbar items
    if (window->config->development.debug_mode) {
        printf("Adding toolbar item: %s (%s)\n", identifier, title);
    }
}

void platform_show_window(app_window_t* window) {
    if (!window || !window->native_window) return;
    
    platform_native_window_t* native = (platform_native_window_t*)window->native_window;
    
    // Show window
    ((void (*)(id, SEL, id))objc_msgSend)(native->ns_window, sel_registerName("makeKeyAndOrderFront:"), nil);
    
    // Activate the application
    ((void (*)(id, SEL, BOOL))objc_msgSend)(g_app, sel_registerName("activateIgnoringOtherApps:"), YES);
    
    printf("Window shown and activated\n");
}

void platform_hide_window(app_window_t* window) {
    if (!window || !window->native_window) return;
    
    platform_native_window_t* native = (platform_native_window_t*)window->native_window;
    ((void (*)(id, SEL, id))objc_msgSend)(native->ns_window, sel_registerName("orderOut:"), native->ns_window);
}

void platform_set_window_title(app_window_t* window, const char* title) {
    if (!window || !window->native_window || !title) return;
    
    platform_native_window_t* native = (platform_native_window_t*)window->native_window;
    Class NSString = objc_getClass("NSString");
    id ns_title = ((id (*)(id, SEL, const char*))objc_msgSend)(
        (id)NSString,
        sel_registerName("stringWithUTF8String:"),
        title
    );
    
    ((void (*)(id, SEL, id))objc_msgSend)(
        native->ns_window,
        sel_registerName("setTitle:"),
        ns_title
    );
}

void platform_set_window_size(app_window_t* window, int width, int height) {
    if (!window || !window->native_window) return;
    
    platform_native_window_t* native = (platform_native_window_t*)window->native_window;
    
    CGRect frame = CGRectMake(0, 0, width, height);
    ((void (*)(id, SEL, CGRect, int))objc_msgSend)(
        native->ns_window,
        sel_registerName("setFrame:display:"),
        frame,
        1
    );
}

void platform_center_window(app_window_t* window) {
    if (!window || !window->native_window) return;
    
    platform_native_window_t* native = (platform_native_window_t*)window->native_window;
    ((void (*)(id, SEL))objc_msgSend)(native->ns_window, sel_registerName("center"));
}

void platform_run_app(app_window_t* window) {
    if (!window || !window->native_window) return;
    
    platform_native_window_t* native = (platform_native_window_t*)window->native_window;
    
    if (window->config->development.debug_mode) {
        printf("Starting macOS application event loop...\n");
        printf("Press Cmd+Q or close the window to quit.\n");
    }
    
    // Run the application event loop
    ((void (*)(id, SEL))objc_msgSend)(native->ns_app, sel_registerName("run"));
}

void platform_quit_app(void) {
    if (g_app) {
        ((void (*)(id, SEL, id))objc_msgSend)(g_app, sel_registerName("terminate:"), g_app);
    }
}

void platform_destroy_window(app_window_t* window) {
    if (!window) return;
    
    if (window->native_window) {
        platform_native_window_t* native = (platform_native_window_t*)window->native_window;
        
        // Close and release native window
        if (native->ns_window) {
            ((void (*)(id, SEL))objc_msgSend)(native->ns_window, sel_registerName("close"));
        }
        
        free(window->native_window);
    }
    
    free(window);
    
    if (window->config && window->config->development.debug_mode) {
        printf("Window destroyed\n");
    }
}

void platform_handle_menu_action(const char* action) {
    if (!action) return;
    
    printf("Menu action triggered: %s\n", action);
    
    // Handle common menu actions
    if (strcmp(action, "new") == 0) {
        printf("Creating new document...\n");
    } else if (strcmp(action, "open") == 0) {
        printf("Opening file dialog...\n");
    } else if (strcmp(action, "save") == 0) {
        printf("Saving document...\n");
    } else if (strcmp(action, "close_window") == 0) {
        printf("Closing window...\n");
        // TODO: Close current window
    } else if (strcmp(action, "undo") == 0) {
        printf("Undoing last action...\n");
    } else if (strcmp(action, "redo") == 0) {
        printf("Redoing action...\n");
    } else if (strcmp(action, "cut") == 0) {
        printf("Cutting selection...\n");
    } else if (strcmp(action, "copy") == 0) {
        printf("Copying selection...\n");
    } else if (strcmp(action, "paste") == 0) {
        printf("Pasting from clipboard...\n");
    } else if (strcmp(action, "zoom_in") == 0) {
        printf("Zooming in...\n");
    } else if (strcmp(action, "zoom_out") == 0) {
        printf("Zooming out...\n");
    } else if (strcmp(action, "zoom_reset") == 0) {
        printf("Resetting zoom...\n");
    } else if (strcmp(action, "minimize") == 0) {
        printf("Minimizing window...\n");
    } else if (strcmp(action, "zoom_window") == 0) {
        printf("Zooming window...\n");
    } else if (strcmp(action, "show_help") == 0) {
        printf("Showing help documentation...\n");
    } else {
        printf("Unknown menu action: %s\n", action);
    }
}

// Menu action callback - this gets called when menu items are selected
void menu_action_callback(id self, SEL _cmd, id sender) {
    (void)self; (void)_cmd; // Suppress unused parameter warnings
    
    // Get the action from the menu item
    Class NSMenuItem = objc_getClass("NSMenuItem");
    if (((BOOL (*)(id, SEL, id))objc_msgSend)(sender, sel_registerName("isKindOfClass:"), (id)NSMenuItem)) {
        id represented_object = ((id (*)(id, SEL))objc_msgSend)(sender, sel_registerName("representedObject"));
        if (represented_object) {
            const char* action = ((const char* (*)(id, SEL))objc_msgSend)(represented_object, sel_registerName("UTF8String"));
            platform_handle_menu_action(action);
        }
    }
}

static unsigned long parse_key_equivalent(const char* shortcut) {
    if (!shortcut || strlen(shortcut) == 0) return 0;
    
    unsigned long mask = 0;
    
    if (strstr(shortcut, "cmd+")) mask |= 0x100000; // NSCommandKeyMask
    if (strstr(shortcut, "shift+")) mask |= 0x20000; // NSShiftKeyMask
    if (strstr(shortcut, "alt+")) mask |= 0x80000; // NSAlternateKeyMask
    if (strstr(shortcut, "ctrl+")) mask |= 0x40000; // NSControlKeyMask
    
    return mask;
}

static const char* get_key_equivalent_string(const char* shortcut) {
    if (!shortcut || strlen(shortcut) == 0) return "";
    
    // Extract the key after the last '+'
    char* plus_pos = strrchr(shortcut, '+');
    if (!plus_pos) return "";
    
    // Handle special keys
    if (strcmp(plus_pos + 1, "plus") == 0) return "+";
    if (strcmp(plus_pos + 1, "minus") == 0) return "-";
    
    // Return single character as string
    static char key_str[2];
    key_str[0] = *(plus_pos + 1);
    key_str[1] = '\0';
    return key_str;
}

static id create_menu_item(const menu_item_config_t* item_config) {
    Class NSMenuItem = objc_getClass("NSMenuItem");
    Class NSString = objc_getClass("NSString");
    
    // Create menu item title
    id title = ((id (*)(id, SEL, const char*))objc_msgSend)(
        (id)NSString,
        sel_registerName("stringWithUTF8String:"),
        item_config->title
    );
    
    // Create key equivalent string
    const char* keyEquivalent = get_key_equivalent_string(item_config->shortcut);
    
    // Create menu item - first alloc, then init
    id menuItem = ((id (*)(id, SEL))objc_msgSend)((id)NSMenuItem, sel_registerName("alloc"));
    menuItem = ((id (*)(id, SEL, id, SEL, id))objc_msgSend)(
        menuItem,
        sel_registerName("initWithTitle:action:keyEquivalent:"),
        title,
        sel_registerName("menuAction:"),
        ((id (*)(id, SEL, const char*))objc_msgSend)((id)NSString, sel_registerName("stringWithUTF8String:"), keyEquivalent)
    );
    
    // Set the target to the app so our callback gets called
    Class NSApplication = objc_getClass("NSApplication");
    id app = ((id (*)(id, SEL))objc_msgSend)((id)NSApplication, sel_registerName("sharedApplication"));
    ((void (*)(id, SEL, id))objc_msgSend)(menuItem, sel_registerName("setTarget:"), app);
    
    // Set key equivalent mask
    unsigned long mask = parse_key_equivalent(item_config->shortcut);
    if (mask != 0) {
        ((void (*)(id, SEL, unsigned long))objc_msgSend)(menuItem, sel_registerName("setKeyEquivalentModifierMask:"), mask);
    }
    
    // Set enabled state
    ((void (*)(id, SEL, BOOL))objc_msgSend)(menuItem, sel_registerName("setEnabled:"), item_config->enabled);
    
    // Set action string as represented object
    id actionString = ((id (*)(id, SEL, const char*))objc_msgSend)(
        (id)NSString,
        sel_registerName("stringWithUTF8String:"),
        item_config->action
    );
    ((void (*)(id, SEL, id))objc_msgSend)(menuItem, sel_registerName("setRepresentedObject:"), actionString);
    
    return menuItem;
}

static id create_menu(const menu_config_t* menu_config) {
    if (!menu_config->enabled) return nil;
    
    Class NSMenu = objc_getClass("NSMenu");
    Class NSString = objc_getClass("NSString");
    Class NSMenuItem = objc_getClass("NSMenuItem");
    
    // Create menu title
    id title = ((id (*)(id, SEL, const char*))objc_msgSend)(
        (id)NSString,
        sel_registerName("stringWithUTF8String:"),
        menu_config->title
    );
    
    // Create menu
    id menu = ((id (*)(id, SEL, id))objc_msgSend)(
        ((id (*)(id, SEL))objc_msgSend)((id)NSMenu, sel_registerName("alloc")),
        sel_registerName("initWithTitle:"),
        title
    );
    
    // Add menu items
    for (int i = 0; i < menu_config->item_count; i++) {
        id menuItem = create_menu_item(&menu_config->items[i]);
        if (menuItem) {
            ((void (*)(id, SEL, id))objc_msgSend)(menu, sel_registerName("addItem:"), menuItem);
            
            // Add separator if requested
            if (menu_config->items[i].separator_after) {
                id separator = ((id (*)(id, SEL))objc_msgSend)((id)NSMenuItem, sel_registerName("separatorItem"));
                ((void (*)(id, SEL, id))objc_msgSend)(menu, sel_registerName("addItem:"), separator);
            }
        }
    }
    
    return menu;
}

void platform_setup_menubar(app_window_t* window) {
    if (!window || !window->config || !window->config->menubar.enabled) return;
    
    const menubar_config_t* menubar = &window->config->menubar;
    
    Class NSMenu = objc_getClass("NSMenu");
    Class NSMenuItem = objc_getClass("NSMenuItem");
    Class NSString = objc_getClass("NSString");
    
    // Use global g_app instead of creating a local one
    if (!g_app) {
        printf("Error: NSApplication not initialized\n");
        return;
    }
    
    // Create main menu
    id mainMenu = ((id (*)(id, SEL))objc_msgSend)(
        ((id (*)(id, SEL))objc_msgSend)((id)NSMenu, sel_registerName("alloc")),
        sel_registerName("init")
    );
    
    // Create application menu (first menu)
    id appMenuItem = ((id (*)(id, SEL))objc_msgSend)((id)NSMenuItem, sel_registerName("alloc"));
    appMenuItem = ((id (*)(id, SEL, id, SEL, id))objc_msgSend)(
        appMenuItem,
        sel_registerName("initWithTitle:action:keyEquivalent:"),
        ((id (*)(id, SEL, const char*))objc_msgSend)((id)NSString, sel_registerName("stringWithUTF8String:"), ""),
        nil,
        ((id (*)(id, SEL, const char*))objc_msgSend)((id)NSString, sel_registerName("stringWithUTF8String:"), "")
    );
    
    id appMenu = ((id (*)(id, SEL))objc_msgSend)(
        ((id (*)(id, SEL))objc_msgSend)((id)NSMenu, sel_registerName("alloc")),
        sel_registerName("init")
    );
    
    // Add About item if enabled
    if (menubar->show_about_item) {
        id aboutTitle = ((id (*)(id, SEL, const char*))objc_msgSend)(
            (id)NSString,
            sel_registerName("stringWithUTF8String:"),
            "About"
        );
        id aboutItem = ((id (*)(id, SEL, id, SEL, id))objc_msgSend)(
            ((id (*)(id, SEL))objc_msgSend)((id)NSMenuItem, sel_registerName("alloc")),
            sel_registerName("initWithTitle:action:keyEquivalent:"),
            aboutTitle,
            sel_registerName("orderFrontStandardAboutPanel:"),
            ((id (*)(id, SEL, const char*))objc_msgSend)((id)NSString, sel_registerName("stringWithUTF8String:"), "")
        );
        ((void (*)(id, SEL, id))objc_msgSend)(appMenu, sel_registerName("addItem:"), aboutItem);
        
        // Add separator
        id separator = ((id (*)(id, SEL))objc_msgSend)((id)NSMenuItem, sel_registerName("separatorItem"));
        ((void (*)(id, SEL, id))objc_msgSend)(appMenu, sel_registerName("addItem:"), separator);
    }
    
    // Add Quit item
    id quitTitle = ((id (*)(id, SEL, const char*))objc_msgSend)(
        (id)NSString,
        sel_registerName("stringWithUTF8String:"),
        "Quit"
    );
    id quitItem = ((id (*)(id, SEL, id, SEL, id))objc_msgSend)(
        ((id (*)(id, SEL))objc_msgSend)((id)NSMenuItem, sel_registerName("alloc")),
        sel_registerName("initWithTitle:action:keyEquivalent:"),
        quitTitle,
        sel_registerName("terminate:"),
        ((id (*)(id, SEL, const char*))objc_msgSend)((id)NSString, sel_registerName("stringWithUTF8String:"), "q")
    );
    ((void (*)(id, SEL, unsigned long))objc_msgSend)(quitItem, sel_registerName("setKeyEquivalentModifierMask:"), 0x100000); // Cmd+Q
    ((void (*)(id, SEL, id))objc_msgSend)(appMenu, sel_registerName("addItem:"), quitItem);
    
    ((void (*)(id, SEL, id))objc_msgSend)(appMenuItem, sel_registerName("setSubmenu:"), appMenu);
    ((void (*)(id, SEL, id))objc_msgSend)(mainMenu, sel_registerName("addItem:"), appMenuItem);
    
    // Add configured menus
    const menu_config_t* menus[] = {
        &menubar->file_menu,
        &menubar->edit_menu,
        &menubar->view_menu,
        &menubar->window_menu,
        &menubar->help_menu
    };
    
    for (int i = 0; i < 5; i++) {
        if (menus[i]->enabled) {
            if (window->config->development.debug_mode) {
                printf("Creating menu: %s with %d items\n", menus[i]->title, menus[i]->item_count);
            }
            
            id menu = create_menu(menus[i]);
            if (menu) {
                // Create the top-level menu item with the proper title
                id menuTitle = ((id (*)(id, SEL, const char*))objc_msgSend)(
                    (id)NSString,
                    sel_registerName("stringWithUTF8String:"),
                    menus[i]->title
                );
                
                id menuItem = ((id (*)(id, SEL))objc_msgSend)((id)NSMenuItem, sel_registerName("alloc"));
                menuItem = ((id (*)(id, SEL, id, SEL, id))objc_msgSend)(
                    menuItem,
                    sel_registerName("initWithTitle:action:keyEquivalent:"),
                    menuTitle,
                    nil,
                    ((id (*)(id, SEL, const char*))objc_msgSend)((id)NSString, sel_registerName("stringWithUTF8String:"), "")
                );
                ((void (*)(id, SEL, id))objc_msgSend)(menuItem, sel_registerName("setSubmenu:"), menu);
                ((void (*)(id, SEL, id))objc_msgSend)(mainMenu, sel_registerName("addItem:"), menuItem);
            }
        }
    }
    
    // Set the main menu using global g_app
    ((void (*)(id, SEL, id))objc_msgSend)(g_app, sel_registerName("setMainMenu:"), mainMenu);
    
    if (window->config->development.debug_mode) {
        printf("macOS menubar configured with %d menus\n", 
               menubar->file_menu.enabled + menubar->edit_menu.enabled + 
               menubar->view_menu.enabled + menubar->window_menu.enabled + 
               menubar->help_menu.enabled);
    }
}

void platform_setup_webview(app_window_t* window) {
    if (!window || !window->native_window || !window->config->webview.enabled) return;
    
    platform_native_window_t* native = (platform_native_window_t*)window->native_window;
    
    // Get required classes
    Class WKWebView = objc_getClass("WKWebView");
    Class WKWebViewConfiguration = objc_getClass("WKWebViewConfiguration");
    Class WKPreferences = objc_getClass("WKPreferences");
    Class WKUserContentController = objc_getClass("WKUserContentController");
    
    if (!WKWebView || !WKWebViewConfiguration) {
        printf("Error: WebKit framework not available\n");
        return;
    }
    
    // Create WKWebViewConfiguration
    id config = ((id (*)(id, SEL))objc_msgSend)(
        ((id (*)(id, SEL))objc_msgSend)((id)WKWebViewConfiguration, sel_registerName("alloc")),
        sel_registerName("init")
    );
    
    // Create preferences
    id preferences = ((id (*)(id, SEL))objc_msgSend)(
        ((id (*)(id, SEL))objc_msgSend)((id)WKPreferences, sel_registerName("alloc")),
        sel_registerName("init")
    );
    
    // Enable JavaScript
    if (window->config->webview.javascript_enabled) {
        ((void (*)(id, SEL, BOOL))objc_msgSend)(preferences, sel_registerName("setJavaScriptEnabled:"), YES);
    }
    
    // Set preferences on configuration
    ((void (*)(id, SEL, id))objc_msgSend)(config, sel_registerName("setPreferences:"), preferences);
    
    // Create user content controller for message handling
    id userContentController = ((id (*)(id, SEL))objc_msgSend)(
        ((id (*)(id, SEL))objc_msgSend)((id)WKUserContentController, sel_registerName("alloc")),
        sel_registerName("init")
    );
    
    // Set user content controller on configuration
    ((void (*)(id, SEL, id))objc_msgSend)(config, sel_registerName("setUserContentController:"), userContentController);
    
    // Create script message handler
    native->script_handler = create_script_message_handler(window);
    if (native->script_handler) {
        // Create NSString for handler name
        Class NSString = objc_getClass("NSString");
        id handlerName = ((id (*)(id, SEL, const char*))objc_msgSend)(
            (id)NSString,
            sel_registerName("stringWithUTF8String:"),
            "bridge"
        );
        
        // Add script message handler
        ((void (*)(id, SEL, id, id))objc_msgSend)(
            userContentController,
            sel_registerName("addScriptMessageHandler:name:"),
            native->script_handler,
            handlerName
        );
        
        if (window->config->development.debug_mode) {
            printf("Bridge script message handler registered\n");
        }
    }
    
    // Enable developer extras on the configuration (newer method)
    if (window->config->webview.developer_extras) {
        // Check if the method exists before calling it
        SEL developerExtrasSelector = sel_registerName("_setDeveloperExtrasEnabled:");
        if (((BOOL (*)(id, SEL, SEL))objc_msgSend)(config, sel_registerName("respondsToSelector:"), developerExtrasSelector)) {
            ((void (*)(id, SEL, BOOL))objc_msgSend)(config, sel_registerName("_setDeveloperExtrasEnabled:"), YES);
        } else {
            // Fallback: try the old preferences method with safety check
            SEL prefsDevExtrasSelector = sel_registerName("setDeveloperExtrasEnabled:");
            if (((BOOL (*)(id, SEL, SEL))objc_msgSend)(preferences, sel_registerName("respondsToSelector:"), prefsDevExtrasSelector)) {
                ((void (*)(id, SEL, BOOL))objc_msgSend)(preferences, sel_registerName("setDeveloperExtrasEnabled:"), YES);
            } else {
                if (window->config->development.debug_mode) {
                    printf("Developer extras not available on this WebKit version\n");
                }
            }
        }
    }
    
    // Create WebView frame (same as window content view)
    CGRect frame = ((CGRect (*)(id, SEL))objc_msgSend)(native->ns_window, sel_registerName("frame"));
    
    // Create WKWebView
    id webview = ((id (*)(id, SEL))objc_msgSend)((id)WKWebView, sel_registerName("alloc"));
    webview = ((id (*)(id, SEL, CGRect, id))objc_msgSend)(
        webview,
        sel_registerName("initWithFrame:configuration:"),
        frame,
        config
    );
    
    // Set as content view
    ((void (*)(id, SEL, id))objc_msgSend)(
        native->ns_window,
        sel_registerName("setContentView:"),
        webview
    );
    
    // Store WebView in native window structure
    native->webview = webview;
    native->webview_config = config;
    
    // Load initial URL if available
    const char* url = get_webview_url(&window->config->webview.framework);
    if (url && strlen(url) > 0) {
        platform_webview_load_url(window, url);
    }
    
    if (window->config->development.debug_mode) {
        printf("WebView initialized successfully\n");
    }
}

void platform_webview_load_url(app_window_t* window, const char* url) {
    if (!window || !window->native_window || !url) return;
    
    platform_native_window_t* native = (platform_native_window_t*)window->native_window;
    if (!native->webview) return;
    
    // Get required classes
    Class NSString = objc_getClass("NSString");
    Class NSURL = objc_getClass("NSURL");
    
    // Create NSString from URL
    id urlString = ((id (*)(id, SEL, const char*))objc_msgSend)(
        (id)NSString,
        sel_registerName("stringWithUTF8String:"),
        url
    );
    
    // Create NSURL from string
    id nsurl = ((id (*)(id, SEL, id))objc_msgSend)(
        ((id (*)(id, SEL))objc_msgSend)((id)NSURL, sel_registerName("alloc")),
        sel_registerName("initWithString:"),
        urlString
    );
    
    // Create NSURLRequest
    id request = ((id (*)(id, SEL, id))objc_msgSend)(
        ((id (*)(id, SEL))objc_msgSend)((id)objc_getClass("NSURLRequest"), sel_registerName("alloc")),
        sel_registerName("initWithURL:"),
        nsurl
    );
    
    // Load request in WebView
    ((void (*)(id, SEL, id))objc_msgSend)(
        native->webview,
        sel_registerName("loadRequest:"),
        request
    );
    
    if (window->config->development.debug_mode) {
        printf("Loading URL: %s\n", url);
    }
}

void platform_webview_load_html(app_window_t* window, const char* html) {
    if (!window || !window->native_window || !html) return;
    
    platform_native_window_t* native = (platform_native_window_t*)window->native_window;
    if (!native->webview) return;
    
    Class NSString = objc_getClass("NSString");
    
    // Create HTML string
    id htmlString = ((id (*)(id, SEL, const char*))objc_msgSend)(
        (id)NSString,
        sel_registerName("stringWithUTF8String:"),
        html
    );
    
    // Create base URL for relative paths
    id baseURL = NULL;
    const char* url = get_webview_url(&window->config->webview.framework);
    if (url && strlen(url) > 0) {
        Class NSURL = objc_getClass("NSURL");
        id urlString = ((id (*)(id, SEL, const char*))objc_msgSend)(
            (id)NSString,
            sel_registerName("stringWithUTF8String:"),
            url
        );
        baseURL = ((id (*)(id, SEL, id))objc_msgSend)(
            (id)NSURL,
            sel_registerName("URLWithString:"),
            urlString
        );
    }
    
    // Load HTML
    ((void (*)(id, SEL, id, id))objc_msgSend)(
        native->webview,
        sel_registerName("loadHTMLString:baseURL:"),
        htmlString,
        baseURL
    );
    
    if (window->config->development.debug_mode) {
        printf("Loading HTML content\n");
    }
}

void platform_webview_evaluate_javascript(app_window_t* window, const char* script) {
    if (!window || !window->native_window || !script) return;
    
    platform_native_window_t* native = (platform_native_window_t*)window->native_window;
    if (!native->webview) return;
    
    Class NSString = objc_getClass("NSString");
    
    // Create script string
    id scriptString = ((id (*)(id, SEL, const char*))objc_msgSend)(
        (id)NSString,
        sel_registerName("stringWithUTF8String:"),
        script
    );
    
    // Evaluate JavaScript
    ((void (*)(id, SEL, id, id))objc_msgSend)(
        native->webview,
        sel_registerName("evaluateJavaScript:completionHandler:"),
        scriptString,
        NULL
    );
    
    if (window->config->development.debug_mode) {
        printf("Evaluating JavaScript: %s\n", script);
    }
}

void platform_webview_navigate(app_window_t* window) {
    if (!window || !window->native_window) return;
    
    platform_native_window_t* native = (platform_native_window_t*)window->native_window;
    if (!native->webview) return;
    
    // Get the URL from the framework configuration
    const char* url = get_webview_url(&window->config->webview.framework);
    if (url && strlen(url) > 0) {
        platform_webview_load_url(window, url);
        
        if (window->config->development.debug_mode) {
            printf("Navigating to URL: %s\n", url);
        }
    }
}

void platform_close_window(app_window_t* window) {
    if (!window || !window->native_window) return;
    
    platform_native_window_t* native = (platform_native_window_t*)window->native_window;
    
    // Hide window
    ((void (*)(id, SEL, id))objc_msgSend)(native->ns_window, sel_registerName("orderOut:"), native->ns_window);
    
    // Close window
    ((void (*)(id, SEL))objc_msgSend)(native->ns_window, sel_registerName("close"));
}

void platform_run_event_loop(void) {
    // Run the event loop
    ((void (*)(id, SEL))objc_msgSend)(g_app, sel_registerName("run"));
}

// Script Message Handler Implementation

// Structure to hold window reference for message handler
typedef struct {
    Class class;
    app_window_t* window;
} BridgeMessageHandler;

// Message handler callback function
void bridge_message_handler_callback(id self, SEL _cmd, id userContentController, id message) {
    // Get the window reference from the handler structure
    BridgeMessageHandler* handler = (BridgeMessageHandler*)self;
    app_window_t* window = handler->window;
    
    if (!window) {
        printf("Bridge message handler: No window reference\n");
        return;
    }
    
    // Get message body from the WKScriptMessage (second parameter)
    id body = ((id (*)(id, SEL))objc_msgSend)(message, sel_registerName("body"));
    if (!body) {
        printf("Bridge message handler: No message body\n");
        return;
    }
    
    // Convert message body to C string
    const char* messageString = ((const char* (*)(id, SEL))objc_msgSend)(body, sel_registerName("UTF8String"));
    if (!messageString) {
        printf("Bridge message handler: Failed to get message string\n");
        return;
    }
    
    if (window->config->development.debug_mode) {
        printf("Bridge received WebKit message: %s\n", messageString);
    }
    
    // Forward to bridge system
    bridge_handle_message(messageString, window);
}

// Create script message handler
id create_script_message_handler(app_window_t* window) {
    // Create a new class for the message handler
    Class handlerClass = objc_allocateClassPair(objc_getClass("NSObject"), "BridgeMessageHandler", sizeof(BridgeMessageHandler) - sizeof(Class));
    if (!handlerClass) {
        printf("Failed to create script message handler class\n");
        return nil;
    }
    
    // Add the userContentController:didReceiveScriptMessage: method
    SEL messageSelector = sel_registerName("userContentController:didReceiveScriptMessage:");
    if (!class_addMethod(handlerClass, messageSelector, (IMP)bridge_message_handler_callback, "v@:@@")) {
        printf("Failed to add message handler method\n");
        objc_disposeClassPair(handlerClass);
        return nil;
    }
    
    // Register the class
    objc_registerClassPair(handlerClass);
    
    // Create an instance
    id handler = ((id (*)(id, SEL))objc_msgSend)((id)handlerClass, sel_registerName("alloc"));
    handler = ((id (*)(id, SEL))objc_msgSend)(handler, sel_registerName("init"));
    
    // Store the window reference in the handler
    BridgeMessageHandler* bridgeHandler = (BridgeMessageHandler*)handler;
    bridgeHandler->window = window;
    
    if (window->config->development.debug_mode) {
        printf("Script message handler created successfully\n");
    }
    
    return handler;
}

#endif // __APPLE__ 