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
#include <math.h>
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
static app_window_t* g_current_window = NULL; // Store current window for menu actions

// Function declarations
static id create_script_message_handler(app_window_t* window);

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

// Forward declarations
static void cleanup_main_window(void);
static void signal_handler(int sig);
static Class create_window_delegate_class(void);
static void setup_modern_toolbar(app_window_t* window, platform_native_window_t* native);

// Toolbar delegate methods
static id toolbar_item_for_identifier(id self, SEL _cmd, id toolbar, id itemIdentifier);
static id toolbar_default_item_identifiers(id self, SEL _cmd, id toolbar);
static id toolbar_allowed_item_identifiers(id self, SEL _cmd, id toolbar);
static Class create_toolbar_delegate_class(void);

// Toolbar helper functions
static id create_toolbar_button_with_symbol(const char* symbolName, const char* action_name, const char* tooltip, app_window_t* window);
static id create_toolbar_button_from_config(const toolbar_button_config_t* config, app_window_t* window);

// Toolbar callback functions

// NEW: Universal toolbar callback that dispatches to bridge system
static void universal_toolbar_callback(id self, SEL _cmd, id sender) {
  (void)self;
  (void)_cmd; // Suppress unused parameter warnings

  // Get the window from the sender's represented object
  id represented_object = ((id(*)(id, SEL))objc_msgSend)(
      sender, sel_registerName("representedObject"));
  if (represented_object) {
    app_window_t *window = (app_window_t *)((void *(*)(id, SEL))objc_msgSend)(
        represented_object, sel_registerName("pointerValue"));
    if (window) {
      // Get the action name from the button's identifier or title
      id identifier = ((id(*)(id, SEL))objc_msgSend)(
          sender, sel_registerName("identifier"));
      const char *action_name = NULL;

      if (identifier) {
        action_name = ((const char *(*)(id, SEL))objc_msgSend)(
            identifier, sel_registerName("UTF8String"));
      }

      if (action_name) {
        // Use the bridge system to handle the action
        bridge_handle_toolbar_action(action_name, window);
      } else {
        printf("Warning: Toolbar button clicked but no action name found\n");
      }
    }
  }
}

// Outline view data source implementation
static Class create_toolbar_delegate_class(void) {
    static Class ToolbarDelegateClass = nil;
    if (ToolbarDelegateClass == nil) {
        ToolbarDelegateClass = objc_allocateClassPair(objc_getClass("NSObject"), "ToolbarDelegate", 0);
        
        // Add instance variable to store window pointer
        class_addIvar(ToolbarDelegateClass, "window", sizeof(void*), log2(sizeof(void*)), "^v");
        
        // Add toolbar delegate methods
        class_addMethod(ToolbarDelegateClass, sel_registerName("toolbar:itemForItemIdentifier:willBeInsertedIntoToolbar:"),
                       (IMP)toolbar_item_for_identifier, "@@:@@c");
        class_addMethod(ToolbarDelegateClass, sel_registerName("toolbarDefaultItemIdentifiers:"),
                       (IMP)toolbar_default_item_identifiers, "@@:@");
        class_addMethod(ToolbarDelegateClass, sel_registerName("toolbarAllowedItemIdentifiers:"),
                       (IMP)toolbar_allowed_item_identifiers, "@@:@");
        
        objc_registerClassPair(ToolbarDelegateClass);
    }
    return ToolbarDelegateClass;
}


// ============================================================================
// PLATFORM INITIALIZATION AND WINDOW MANAGEMENT
// ============================================================================

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
    
    // Add menu action method to NSApplication class for menu handling
    Class appClass = object_getClass(g_app);
    SEL menuActionSelector = sel_registerName("menuAction:");
    if (!class_addMethod(appClass, menuActionSelector, (IMP)menu_action_callback, "v@:@")) {
        // Method might already exist, try to replace it
        method_setImplementation(class_getInstanceMethod(appClass, menuActionSelector), (IMP)menu_action_callback);
        if (app_config->development.debug_mode) {
            printf("Menu action method replaced on NSApplication\n");
        }
    } else {
        if (app_config->development.debug_mode) {
            printf("Menu action method added to NSApplication\n");
        }
    }
    
    // Set activation policy to regular app (shows in dock)
    ((void (*)(id, SEL, long))objc_msgSend)(g_app, sel_registerName("setActivationPolicy:"), 0); // NSApplicationActivationPolicyRegular
    
    printf("Modern macOS platform initialized\n");
    
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
    printf("Modern macOS platform cleaned up\n");
}

bool platform_create_window(app_window_t* window) {
    if (!window) return false;
    
    // Store global window reference for menu actions
    g_current_window = window;
    
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
    
    // Create window style mask based on config
    unsigned long style_mask = 0;
    style_mask |= (1 << 0);  // NSWindowStyleMaskTitled
    style_mask |= (1 << 1);  // NSWindowStyleMaskClosable
    style_mask |= (1 << 2);  // NSWindowStyleMaskMiniaturizable
    style_mask |= (1 << 3);  // NSWindowStyleMaskResizable
    
    // Add unified toolbar and full size content view for modern appearance
    style_mask |= (1 << 15); // NSWindowStyleMaskUnifiedTitleAndToolbar
    style_mask |= (1 << 13); // NSWindowStyleMaskFullSizeContentView
    
    printf("Creating modern window with style mask: 0x%lx\n", style_mask);
    
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
    
    // Set window properties for modern appearance
    ((void (*)(id, SEL, BOOL))objc_msgSend)(ns_window, sel_registerName("setReleasedWhenClosed:"), NO);
    
    // Create and set window delegate for proper close handling
    Class WindowDelegateClass = create_window_delegate_class();
    id delegate = ((id (*)(id, SEL))objc_msgSend)((id)WindowDelegateClass, sel_registerName("alloc"));
    delegate = ((id (*)(id, SEL))objc_msgSend)(delegate, sel_registerName("init"));
    ((void (*)(id, SEL, id))objc_msgSend)(ns_window, sel_registerName("setDelegate:"), delegate);
    
    // Configure title bar based on config
    if (!window->config->macos.show_title_bar) {
        // Hide title bar completely and make it transparent
        ((void (*)(id, SEL, BOOL))objc_msgSend)(ns_window, sel_registerName("setTitlebarAppearsTransparent:"), YES);
        ((void (*)(id, SEL, long))objc_msgSend)(ns_window, sel_registerName("setTitleVisibility:"), 1); // NSWindowTitleHidden
        
        // Enable full-size content view and make toolbar area draggable
        ((void (*)(id, SEL, BOOL))objc_msgSend)(ns_window, sel_registerName("setMovableByWindowBackground:"), YES);
        
        // Additional settings for proper toolbar dragging
        ((void (*)(id, SEL, BOOL))objc_msgSend)(ns_window, sel_registerName("setMovable:"), YES);
        
        // Make the entire toolbar area draggable by setting the style mask properly
        unsigned long current_mask = ((unsigned long (*)(id, SEL))objc_msgSend)(ns_window, sel_registerName("styleMask"));
        current_mask |= (1 << 15); // Ensure NSWindowStyleMaskUnifiedTitleAndToolbar is set
        ((void (*)(id, SEL, unsigned long))objc_msgSend)(ns_window, sel_registerName("setStyleMask:"), current_mask);
        
        if (window->config->development.debug_mode) {
            printf("Title bar hidden, window draggable by background and toolbar (mask: 0x%lx)\n", current_mask);
        }
        } else {
        // Show normal title bar
        ((void (*)(id, SEL, BOOL))objc_msgSend)(ns_window, sel_registerName("setTitlebarAppearsTransparent:"), NO);
        ((void (*)(id, SEL, long))objc_msgSend)(ns_window, sel_registerName("setTitleVisibility:"), 0); // NSWindowTitleVisible
        
        if (window->config->development.debug_mode) {
            printf("Title bar visible\n");
        }
    }
    
    // Set window title
    Class NSString = objc_getClass("NSString");
    id title = ((id (*)(id, SEL, const char*))objc_msgSend)(
            (id)NSString,
            sel_registerName("stringWithUTF8String:"),
        window->config->window.title
    );
    ((void (*)(id, SEL, id))objc_msgSend)(ns_window, sel_registerName("setTitle:"), title);
    
    // Store window in native structure
    native->ns_window = ns_window;
    
    printf("Modern macOS window created successfully\n");
    return true;
}

// ============================================================================
// WINDOW MANAGEMENT FUNCTIONS
// ============================================================================

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
        printf("Starting modern macOS application event loop...\n");
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
    
    // First, check if this action is a registered bridge function
    if (g_current_window && bridge_function_exists(action)) {
        printf("Menu action '%s' found in bridge system - forwarding to bridge\n", action);
        bridge_handle_toolbar_action(action, g_current_window);
        return;
    }
    
    // Handle common menu actions (legacy fallback)
    if (strcmp(action, "new") == 0) {
        printf("Creating new document...\n");
    } else if (strcmp(action, "open") == 0) {
        printf("Opening file dialog...\n");
    } else if (strcmp(action, "save") == 0) {
        printf("Saving document...\n");
    } else if (strcmp(action, "close_window") == 0) {
        printf("Closing window...\n");
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

// ============================================================================
// MENU SYSTEM (keeping existing implementation)
// ============================================================================

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

// ============================================================================
// WEBVIEW SETUP AND MANAGEMENT (Modern WebKit implementation)
// ============================================================================

void platform_setup_webview(app_window_t* window) {
    if (!window || !window->native_window || !window->config->webview.enabled) return;
    
    platform_native_window_t* native = (platform_native_window_t*)window->native_window;
    
    // Get required classes
    Class WKWebView = objc_getClass("WKWebView");
    Class WKWebViewConfiguration = objc_getClass("WKWebViewConfiguration");
    Class WKPreferences = objc_getClass("WKPreferences");
    Class WKUserContentController = objc_getClass("WKUserContentController");
    Class NSView = objc_getClass("NSView");
    
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
    
    // Create user content controller for bridge communication
    id userContentController = ((id (*)(id, SEL))objc_msgSend)(
        ((id (*)(id, SEL))objc_msgSend)((id)WKUserContentController, sel_registerName("alloc")),
        sel_registerName("init")
    );
    
    // Create script message handler for bridge
    id scriptHandler = create_script_message_handler(window);
    if (scriptHandler) {
        Class NSString = objc_getClass("NSString");
        id bridgeName = ((id (*)(id, SEL, const char*))objc_msgSend)(
            (id)NSString,
            sel_registerName("stringWithUTF8String:"),
            "bridge"
        );
        
        ((void (*)(id, SEL, id, id))objc_msgSend)(
            userContentController,
            sel_registerName("addScriptMessageHandler:name:"),
            scriptHandler,
            bridgeName
        );
        
        // Store reference for cleanup
        native->script_handler = scriptHandler;
    }
    
    // Set user content controller
    ((void (*)(id, SEL, id))objc_msgSend)(config, sel_registerName("setUserContentController:"), userContentController);
    
    // Enable developer extras if requested
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
    
    // Get window content frame
    CGRect windowFrame = ((CGRect (*)(id, SEL))objc_msgSend)(native->ns_window, sel_registerName("frame"));
    
    // Create container view to hold the webview (this will be the content view)
    id containerView = ((id (*)(id, SEL))objc_msgSend)(
        ((id (*)(id, SEL))objc_msgSend)((id)NSView, sel_registerName("alloc")),
        sel_registerName("init")
    );
    
    // Set container as content view first
    ((void (*)(id, SEL, id))objc_msgSend)(native->ns_window, sel_registerName("setContentView:"), containerView);
    
    // Setup modern toolbar (this needs to be done BEFORE calculating webview frame)
    setup_modern_toolbar(window, native);
    
    // Calculate proper webview frame accounting for toolbar
    CGRect containerFrame = ((CGRect (*)(id, SEL))objc_msgSend)(containerView, sel_registerName("bounds"));
    
    // Adjust for toolbar height if toolbar is enabled
    CGFloat toolbarHeight = 0;
    if (window->config->macos.toolbar.enabled && native->toolbar) {
        // Get toolbar height - typically around 40-50px for macOS
        toolbarHeight = 52; // Standard macOS toolbar height
        
        if (window->config->development.debug_mode) {
            printf("Adjusting webview frame for toolbar height: %.0f px\n", toolbarHeight);
        }
    }
    
    // Create webview frame below the toolbar
    CGRect webviewFrame = CGRectMake(
        0,                                    // x
        0,                                    // y (starts at bottom of container)
        containerFrame.size.width,            // width (full width)
        containerFrame.size.height           // height (full height - toolbar is handled by window)
    );
    
    // Create WKWebView
    id webview = ((id (*)(id, SEL))objc_msgSend)((id)WKWebView, sel_registerName("alloc"));
    webview = ((id (*)(id, SEL, CGRect, id))objc_msgSend)(
        webview,
        sel_registerName("initWithFrame:configuration:"),
        webviewFrame,
        config
    );
    
    // Store WebView in native window structure
    native->webview = webview;
    native->webview_config = config;

    // Add webview to container
    ((void (*)(id, SEL, id))objc_msgSend)(containerView, sel_registerName("addSubview:"), webview);
    
    // Set up autoresizing so webview fills the container
    ((void (*)(id, SEL, unsigned long))objc_msgSend)(webview, sel_registerName("setAutoresizingMask:"), 
        (1 << 1) | (1 << 4)); // NSViewWidthSizable | NSViewHeightSizable
    
    // Load initial URL if available
    const char* url = get_webview_url(&window->config->webview.framework);
    if (url && strlen(url) > 0) {
        platform_webview_load_url(window, url);
    }
    
    printf("Native WebView initialized successfully with proper toolbar separation\n");
    
    if (window->config->development.debug_mode) {
        printf("WebView setup completed:\n");
        printf("- Container view set as content view\n");
        printf("- Webview frame: {%.0f, %.0f, %.0f, %.0f}\n", 
               webviewFrame.origin.x, webviewFrame.origin.y, 
               webviewFrame.size.width, webviewFrame.size.height);
        printf("- Toolbar height accounted for: %.0f px\n", toolbarHeight);
        printf("- Modern WKWebView instance created\n");
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

// ============================================================================
// SCRIPT MESSAGE HANDLER IMPLEMENTATION
// ============================================================================

// Structure to hold window reference for message handler
typedef struct {
    Class class;
    app_window_t* window;
} BridgeMessageHandler;

// Message handler callback function
void bridge_message_handler_callback(id self, SEL _cmd, id userContentController, id message) {
    (void)userContentController;  // Suppress unused parameter warning
    (void)_cmd;  // Suppress unused parameter warning
    
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
static id create_script_message_handler(app_window_t* window) {
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

// ============================================================================
// WINDOW DELEGATE IMPLEMENTATION
// ============================================================================

// Window delegate callback for window close
static BOOL window_should_close_callback(id self, SEL _cmd, id sender) {
    (void)self; (void)_cmd; (void)sender; // Suppress unused parameter warnings
    
    printf("Window close requested - cleaning up and terminating application\n");
    
    // Stop dev server first to prevent hanging processes
    stop_dev_server();
    
    // Clean up bridge system
    bridge_cleanup();
    
    // Terminate the application when window is closed
    if (g_app) {
        // Use performSelector to terminate on next run loop cycle
        ((void (*)(id, SEL, SEL, id, double))objc_msgSend)(
            g_app, 
            sel_registerName("performSelector:withObject:afterDelay:"),
            sel_registerName("terminate:"),
            g_app,
            0.1
        );
    }
    
    return YES; // Allow window to close
}

// Create window delegate class
static Class create_window_delegate_class(void) {
    static Class WindowDelegateClass = nil;
    if (WindowDelegateClass == nil) {
        WindowDelegateClass = objc_allocateClassPair(objc_getClass("NSObject"), "WindowDelegate", 0);
        
        // Add window delegate method
        class_addMethod(WindowDelegateClass, sel_registerName("windowShouldClose:"),
                       (IMP)window_should_close_callback, "c@:@");
        
        objc_registerClassPair(WindowDelegateClass);
    }
    return WindowDelegateClass;
}

// ============================================================================
// TOOLBAR SETUP
// ============================================================================

static void setup_modern_toolbar(app_window_t* window, platform_native_window_t* native) {
    if (!window->config->macos.toolbar.enabled) {
        if (window->config->development.debug_mode) {
            printf("Toolbar disabled in configuration\n");
        }
        return;
    }
    
    Class NSToolbar = objc_getClass("NSToolbar");
    Class NSString = objc_getClass("NSString");
    Class NSTitlebarAccessoryViewController = objc_getClass("NSTitlebarAccessoryViewController");
    Class NSButton = objc_getClass("NSButton");
    Class NSImage = objc_getClass("NSImage");
    Class NSValue = objc_getClass("NSValue");
    
    if (!NSToolbar) {
        printf("NSToolbar class not available\n");
        return;
    }
    
    printf("Setting up modern macOS toolbar with NSToolbar\n");
    
    // Create toolbar with identifier
    id toolbarIdentifier = ((id (*)(id, SEL, const char*))objc_msgSend)(
        (id)NSString,
        sel_registerName("stringWithUTF8String:"),
        "MainToolbar"
    );
    
    id toolbar = ((id (*)(id, SEL, id))objc_msgSend)(
        ((id (*)(id, SEL))objc_msgSend)((id)NSToolbar, sel_registerName("alloc")),
        sel_registerName("initWithIdentifier:"),
        toolbarIdentifier
    );
    
    // Create and set toolbar delegate
    Class ToolbarDelegateClass = create_toolbar_delegate_class();
    id delegate = ((id (*)(id, SEL))objc_msgSend)(
        ((id (*)(id, SEL))objc_msgSend)((id)ToolbarDelegateClass, sel_registerName("alloc")),
        sel_registerName("init")
    );
    
    // Store window pointer in delegate
    object_setInstanceVariable(delegate, "window", window);
    
    // Set delegate
    ((void (*)(id, SEL, id))objc_msgSend)(toolbar, sel_registerName("setDelegate:"), delegate);
    
    // Configure toolbar for proper visual separation
    ((void (*)(id, SEL, BOOL))objc_msgSend)(toolbar, sel_registerName("setAllowsUserCustomization:"), YES);
    ((void (*)(id, SEL, BOOL))objc_msgSend)(toolbar, sel_registerName("setAutosavesConfiguration:"), YES);
    
    // IMPORTANT: Enable baseline separator for visual separation from webview
    ((void (*)(id, SEL, BOOL))objc_msgSend)(toolbar, sel_registerName("setShowsBaselineSeparator:"), YES);
    
    // Set toolbar display mode
    ((void (*)(id, SEL, long))objc_msgSend)(toolbar, sel_registerName("setDisplayMode:"), 1); // NSToolbarDisplayModeIconOnly
    
    // Set size mode
    ((void (*)(id, SEL, long))objc_msgSend)(toolbar, sel_registerName("setSizeMode:"), 0); // NSToolbarSizeModeDefault
    
    // Set toolbar on window
    ((void (*)(id, SEL, id))objc_msgSend)(native->ns_window, sel_registerName("setToolbar:"), toolbar);
    
    // Configure window for proper toolbar appearance
    if (!window->config->macos.show_title_bar) {
        // For hidden title bar, ensure the toolbar has proper background
        ((void (*)(id, SEL, BOOL))objc_msgSend)(native->ns_window, sel_registerName("setTitlebarAppearsTransparent:"), NO);
        
        // Temporarily show titlebar for toolbar background, then hide title text
        ((void (*)(id, SEL, long))objc_msgSend)(native->ns_window, sel_registerName("setTitleVisibility:"), 1); // NSWindowTitleHidden
        
        // Enable toolbar background while keeping title hidden
        ((void (*)(id, SEL, BOOL))objc_msgSend)(native->ns_window, sel_registerName("setMovableByWindowBackground:"), YES);
        
        if (window->config->development.debug_mode) {
            printf("Toolbar configured with proper background and hidden title\n");
        }
    }
    
    // Store reference
    native->toolbar = toolbar;
    
    if (window->config->development.debug_mode) {
        printf("Modern macOS toolbar setup completed with proper visual separation\n");
    }
}

// ============================================================================
// TOOLBAR DELEGATE CLASS (Updated for three-group layout)
// ============================================================================

// Dynamic toolbar item identifiers from configuration
static id toolbar_default_item_identifiers(id self, SEL _cmd, id toolbar) {
    (void)_cmd; (void)toolbar; // Suppress unused warnings
    
    // Get window from delegate
    app_window_t* window = NULL;
    object_getInstanceVariable(self, "window", (void**)&window);
    if (!window || !window->config) {
        return nil;
    }
    
    Class NSArray = objc_getClass("NSArray");
    Class NSString = objc_getClass("NSString");
    Class NSMutableArray = objc_getClass("NSMutableArray");
    
    id mutableArray = ((id (*)(id, SEL))objc_msgSend)(
        ((id (*)(id, SEL))objc_msgSend)((id)NSMutableArray, sel_registerName("alloc")),
        sel_registerName("init")
    );
    
    // Add LEFT group buttons
    const macos_toolbar_config_t* toolbarConfig = &window->config->macos.toolbar;
    for (int i = 0; i < toolbarConfig->left.button_count; i++) {
        const toolbar_button_config_t* button = &toolbarConfig->left.buttons[i];
        if (button->enabled && strlen(button->action) > 0) {
            id identifier = ((id (*)(id, SEL, const char*))objc_msgSend)(
                (id)NSString,
                sel_registerName("stringWithUTF8String:"),
                button->action
            );
            ((void (*)(id, SEL, id))objc_msgSend)(mutableArray, sel_registerName("addObject:"), identifier);
        }
    }
    
    // Add flexible space to push middle group to center
    if (toolbarConfig->middle.button_count > 0) {
        id flexibleSpaceIdentifier = ((id (*)(id, SEL, const char*))objc_msgSend)(
            (id)NSString,
            sel_registerName("stringWithUTF8String:"),
            "NSToolbarFlexibleSpaceItem"
        );
        ((void (*)(id, SEL, id))objc_msgSend)(mutableArray, sel_registerName("addObject:"), flexibleSpaceIdentifier);
        
        // Add MIDDLE group buttons
        for (int i = 0; i < toolbarConfig->middle.button_count; i++) {
            const toolbar_button_config_t* button = &toolbarConfig->middle.buttons[i];
            if (button->enabled && strlen(button->action) > 0) {
                id identifier = ((id (*)(id, SEL, const char*))objc_msgSend)(
                    (id)NSString,
                    sel_registerName("stringWithUTF8String:"),
                    button->action
                );
                ((void (*)(id, SEL, id))objc_msgSend)(mutableArray, sel_registerName("addObject:"), identifier);
            }
        }
        
        // Add flexible space to push right group to the right
        ((void (*)(id, SEL, id))objc_msgSend)(mutableArray, sel_registerName("addObject:"), flexibleSpaceIdentifier);
    } else if (toolbarConfig->right.button_count > 0) {
        // If no middle group, just add one flexible space before right group
        id flexibleSpaceIdentifier = ((id (*)(id, SEL, const char*))objc_msgSend)(
            (id)NSString,
            sel_registerName("stringWithUTF8String:"),
            "NSToolbarFlexibleSpaceItem"
        );
        ((void (*)(id, SEL, id))objc_msgSend)(mutableArray, sel_registerName("addObject:"), flexibleSpaceIdentifier);
    }
    
    // Add RIGHT group buttons
    for (int i = 0; i < toolbarConfig->right.button_count; i++) {
        const toolbar_button_config_t* button = &toolbarConfig->right.buttons[i];
        if (button->enabled && strlen(button->action) > 0) {
            id identifier = ((id (*)(id, SEL, const char*))objc_msgSend)(
                (id)NSString,
                sel_registerName("stringWithUTF8String:"),
                button->action
            );
            ((void (*)(id, SEL, id))objc_msgSend)(mutableArray, sel_registerName("addObject:"), identifier);
        }
    }
    
    // Convert to immutable array
    id result = ((id (*)(id, SEL, id))objc_msgSend)(
        (id)NSArray,
        sel_registerName("arrayWithArray:"),
        mutableArray
    );
    
    return result;
}

// Dynamic allowed toolbar item identifiers from configuration
static id toolbar_allowed_item_identifiers(id self, SEL _cmd, id toolbar) {
    (void)_cmd; (void)toolbar; // Suppress unused warnings
    
    // Get window from delegate
    app_window_t* window = NULL;
    object_getInstanceVariable(self, "window", (void**)&window);
    if (!window || !window->config) {
        return nil;
    }
    
    Class NSArray = objc_getClass("NSArray");
    Class NSString = objc_getClass("NSString");
    Class NSMutableArray = objc_getClass("NSMutableArray");
    
    id mutableArray = ((id (*)(id, SEL))objc_msgSend)(
        ((id (*)(id, SEL))objc_msgSend)((id)NSMutableArray, sel_registerName("alloc")),
        sel_registerName("init")
    );
    
    // Add all button identifiers from all groups
    const macos_toolbar_config_t* toolbarConfig = &window->config->macos.toolbar;
    
    // LEFT group
    for (int i = 0; i < toolbarConfig->left.button_count; i++) {
        const toolbar_button_config_t* button = &toolbarConfig->left.buttons[i];
        if (strlen(button->action) > 0) {
            id identifier = ((id (*)(id, SEL, const char*))objc_msgSend)(
                (id)NSString,
                sel_registerName("stringWithUTF8String:"),
                button->action
            );
            ((void (*)(id, SEL, id))objc_msgSend)(mutableArray, sel_registerName("addObject:"), identifier);
        }
    }
    
    // MIDDLE group
    for (int i = 0; i < toolbarConfig->middle.button_count; i++) {
        const toolbar_button_config_t* button = &toolbarConfig->middle.buttons[i];
        if (strlen(button->action) > 0) {
            id identifier = ((id (*)(id, SEL, const char*))objc_msgSend)(
                (id)NSString,
                sel_registerName("stringWithUTF8String:"),
                button->action
            );
            ((void (*)(id, SEL, id))objc_msgSend)(mutableArray, sel_registerName("addObject:"), identifier);
        }
    }
    
    // RIGHT group
    for (int i = 0; i < toolbarConfig->right.button_count; i++) {
        const toolbar_button_config_t* button = &toolbarConfig->right.buttons[i];
        if (strlen(button->action) > 0) {
            id identifier = ((id (*)(id, SEL, const char*))objc_msgSend)(
                (id)NSString,
                sel_registerName("stringWithUTF8String:"),
                button->action
            );
            ((void (*)(id, SEL, id))objc_msgSend)(mutableArray, sel_registerName("addObject:"), identifier);
        }
    }
    
    // Add standard toolbar items
    id flexibleSpaceIdentifier = ((id (*)(id, SEL, const char*))objc_msgSend)(
        (id)NSString,
        sel_registerName("stringWithUTF8String:"),
        "NSToolbarFlexibleSpaceItem"
    );
    ((void (*)(id, SEL, id))objc_msgSend)(mutableArray, sel_registerName("addObject:"), flexibleSpaceIdentifier);
    
    id spaceIdentifier = ((id (*)(id, SEL, const char*))objc_msgSend)(
        (id)NSString,
        sel_registerName("stringWithUTF8String:"),
        "NSToolbarSpaceItem"
    );
    ((void (*)(id, SEL, id))objc_msgSend)(mutableArray, sel_registerName("addObject:"), spaceIdentifier);
    
    // Convert to immutable array
    id result = ((id (*)(id, SEL, id))objc_msgSend)(
        (id)NSArray,
        sel_registerName("arrayWithArray:"),
        mutableArray
    );
    
    return result;
}

// Dynamic toolbar item creation from configuration
static id toolbar_item_for_identifier(id self, SEL _cmd, id toolbar, id itemIdentifier) {
    (void)_cmd; (void)toolbar; // Suppress unused warnings
    
    // Get window from delegate
    app_window_t* window = NULL;
    object_getInstanceVariable(self, "window", (void**)&window);
    if (!window || !window->config) {
        return nil;
    }
    
    Class NSString = objc_getClass("NSString");
    
    // Get identifier string
    const char* identifier = ((const char* (*)(id, SEL))objc_msgSend)(
        itemIdentifier,
        sel_registerName("UTF8String")
    );
    
    // Search all groups for matching button configuration
    const macos_toolbar_config_t* toolbarConfig = &window->config->macos.toolbar;
    
    // Search LEFT group
    for (int i = 0; i < toolbarConfig->left.button_count; i++) {
        const toolbar_button_config_t* button = &toolbarConfig->left.buttons[i];
        if (strcmp(button->action, identifier) == 0) {
            return create_toolbar_button_from_config(button, window);
        }
    }
    
    // Search MIDDLE group
    for (int i = 0; i < toolbarConfig->middle.button_count; i++) {
        const toolbar_button_config_t* button = &toolbarConfig->middle.buttons[i];
        if (strcmp(button->action, identifier) == 0) {
            return create_toolbar_button_from_config(button, window);
        }
    }
    
    // Search RIGHT group
    for (int i = 0; i < toolbarConfig->right.button_count; i++) {
        const toolbar_button_config_t* button = &toolbarConfig->right.buttons[i];
        if (strcmp(button->action, identifier) == 0) {
            return create_toolbar_button_from_config(button, window);
        }
    }
    
    // Handle standard toolbar items
    if (strcmp(identifier, "NSToolbarFlexibleSpaceItem") == 0 ||
        strcmp(identifier, "NSToolbarSpaceItem") == 0 ||
        strcmp(identifier, "NSToolbarSeparatorItem") == 0) {
        
        Class NSToolbarItem = objc_getClass("NSToolbarItem");
        id toolbarItem = ((id (*)(id, SEL, id))objc_msgSend)(
            ((id (*)(id, SEL))objc_msgSend)((id)NSToolbarItem, sel_registerName("alloc")),
            sel_registerName("initWithItemIdentifier:"),
            itemIdentifier
        );
        return toolbarItem;
    }
    
    // Unknown identifier
    printf("Warning: Unknown toolbar item identifier: %s\n", identifier);
    return nil;
}

// Add callback methods for the new buttons

// ============================================================================
// TOOLBAR HELPER FUNCTIONS
// ============================================================================

// Helper function to create a toolbar button with native NSButton
static id create_toolbar_button_with_symbol(const char* symbolName, const char* action_name, const char* tooltip, app_window_t* window) {
    Class NSToolbarItem = objc_getClass("NSToolbarItem");
    Class NSButton = objc_getClass("NSButton");
    Class NSImage = objc_getClass("NSImage");
    Class NSString = objc_getClass("NSString");
    Class NSValue = objc_getClass("NSValue");
    
    // Create identifier from action name
    id identifier = ((id (*)(id, SEL, const char*))objc_msgSend)(
        (id)NSString,
        sel_registerName("stringWithUTF8String:"),
        action_name
    );
    
    // Create toolbar item
    id toolbarItem = ((id (*)(id, SEL, id))objc_msgSend)(
        ((id (*)(id, SEL))objc_msgSend)((id)NSToolbarItem, sel_registerName("alloc")),
        sel_registerName("initWithItemIdentifier:"),
        identifier
    );
    
    // Create SF Symbol image first
    id symbolString = ((id (*)(id, SEL, const char*))objc_msgSend)(
        (id)NSString,
        sel_registerName("stringWithUTF8String:"),
        symbolName
    );
    
    id icon = nil;
    
    // Try to create system symbol first
    if (NSImage) {
        icon = ((id (*)(id, SEL, id, id))objc_msgSend)(
            (id)NSImage,
            sel_registerName("imageWithSystemSymbolName:accessibilityDescription:"),
            symbolString,
            nil
        );
    }
    
    if (!icon) {
        // Fallback to system image
        icon = ((id (*)(id, SEL, id))objc_msgSend)(
            (id)NSImage,
            sel_registerName("imageNamed:"),
            symbolString
        );
    }
    
    if (icon) {
        // Set proper icon size for toolbar buttons
        CGSize iconSize = CGSizeMake(24, 24);
        ((void (*)(id, SEL, CGSize))objc_msgSend)(icon, sel_registerName("setSize:"), iconSize);
    }

    // Use universal callback instead of looking up specific callbacks
    SEL action = sel_registerName("universalToolbarAction:");

    // Create native NSButton using the proper initializer: NSButton(image:target:action:)
    id button = ((id (*)(id, SEL, id, id, SEL))objc_msgSend)(
        (id)NSButton,
        sel_registerName("buttonWithImage:target:action:"),
        icon,
        nil,  // target will be set later
        action
    );
    
    if (button) {
        // Configure the native button for toolbar use
        ((void (*)(id, SEL, long))objc_msgSend)(button, sel_registerName("setButtonType:"), 7); // NSMomentaryChangeButton for proper hover behavior
        ((void (*)(id, SEL, BOOL))objc_msgSend)(button, sel_registerName("setBordered:"), NO); // No border by default
        ((void (*)(id, SEL, long))objc_msgSend)(button, sel_registerName("setBezelStyle:"), 4); // NSBezelStyleRegularSquare for clean look
        
        // Enable hover highlighting using NSButtonCell methods
        id buttonCell = ((id (*)(id, SEL))objc_msgSend)(button, sel_registerName("cell"));
        if (buttonCell) {
            ((void (*)(id, SEL, long))objc_msgSend)(buttonCell, sel_registerName("setHighlightsBy:"), 1); // NSContentsCellMask - content highlighting
            ((void (*)(id, SEL, long))objc_msgSend)(buttonCell, sel_registerName("setShowsStateBy:"), 0); // NSNoCellMask - no state indication by default
        }
        
        // Configure for toolbar appearance - transparent by default, subtle highlight on hover
        ((void (*)(id, SEL, BOOL))objc_msgSend)(button, sel_registerName("setTransparent:"), NO);
        ((void (*)(id, SEL, BOOL))objc_msgSend)(button, sel_registerName("setShowsBorderOnlyWhileMouseInside:"), YES);
        
        // Set proper button size
        CGRect buttonFrame = CGRectMake(0, 0, 24, 24);
        ((void (*)(id, SEL, CGRect))objc_msgSend)(button, sel_registerName("setFrame:"), buttonFrame);
        
        // Set target to the button itself for action callbacks
        ((void (*)(id, SEL, id))objc_msgSend)(button, sel_registerName("setTarget:"), button);
        
        // Store window pointer for action callbacks
        id windowValue = ((id (*)(id, SEL, void*))objc_msgSend)(
            (id)NSValue,
            sel_registerName("valueWithPointer:"),
            window
        );
        ((void (*)(id, SEL, id))objc_msgSend)(button, sel_registerName("setRepresentedObject:"), windowValue);

        // Set the identifier as well so we can retrieve the action name
        ((void (*)(id, SEL, id))objc_msgSend)(
            button, sel_registerName("setIdentifier:"), identifier);

        // Add universal action method to button class dynamically
        Class buttonClass = object_getClass(button);
        class_addMethod(buttonClass, action, (IMP)universal_toolbar_callback,
                        "v@:@");
    }
    
    // Set tooltip on toolbar item
    id tooltipStr = ((id (*)(id, SEL, const char*))objc_msgSend)(
        (id)NSString,
        sel_registerName("stringWithUTF8String:"),
        tooltip
    );
    ((void (*)(id, SEL, id))objc_msgSend)(toolbarItem, sel_registerName("setToolTip:"), tooltipStr);
    
    // Set button as the view for the toolbar item
    ((void (*)(id, SEL, id))objc_msgSend)(toolbarItem, sel_registerName("setView:"), button);
    
    return toolbarItem;
}

// Create toolbar button from configuration structure
static id create_toolbar_button_from_config(const toolbar_button_config_t* config, app_window_t* window) {
    // Validate input
    if (!config || !config->enabled || strlen(config->name) == 0) {
        return nil;
    }
    
    return create_toolbar_button_with_symbol(config->icon, config->action, config->tooltip, window);
}

// ============================================================================
// PLATFORM-SPECIFIC UI FUNCTIONS
// ============================================================================

// Flexible alert function with parameters
bool platform_show_alert_with_params(app_window_t* window, const char* title, const char* message, const char* ok_button, const char* cancel_button) {
    if (!window) {
        printf("Failed to show alert: No window provided\n");
        return false;
    }
    
    // Use defaults if parameters are null or empty
    const char* alert_title = (title && strlen(title) > 0) ? title : "Alert";
    const char* alert_message = (message && strlen(message) > 0) ? message : "This is a native alert dialog.";
    const char* ok_text = (ok_button && strlen(ok_button) > 0) ? ok_button : "OK";
    const char* cancel_text = (cancel_button && strlen(cancel_button) > 0) ? cancel_button : "Cancel";
    
    printf("Opening native macOS alert dialog: %s\n", alert_title);
    
    // Create NSAlert for alert dialog
    Class NSAlert = objc_getClass("NSAlert");
    Class NSString = objc_getClass("NSString");
    
    if (!NSAlert || !NSString) {
        printf("Failed to get required classes for alert dialog\n");
        return false;
    }
    
    // Create alert instance
    id alert = ((id (*)(id, SEL))objc_msgSend)(
        ((id (*)(id, SEL))objc_msgSend)((id)NSAlert, sel_registerName("alloc")),
        sel_registerName("init")
    );
    
    if (!alert) {
        printf("Failed to create NSAlert instance\n");
        return false;
    }
    
    // Set dialog properties
    id messageText = ((id (*)(id, SEL, const char*))objc_msgSend)(
        (id)NSString,
        sel_registerName("stringWithUTF8String:"),
        alert_title
    );
    ((void (*)(id, SEL, id))objc_msgSend)(alert, sel_registerName("setMessageText:"), messageText);
    
    id informativeText = ((id (*)(id, SEL, const char*))objc_msgSend)(
        (id)NSString,
        sel_registerName("stringWithUTF8String:"),
        alert_message
    );
    ((void (*)(id, SEL, id))objc_msgSend)(alert, sel_registerName("setInformativeText:"), informativeText);
    
    // Set alert style to informational
    ((void (*)(id, SEL, long))objc_msgSend)(alert, sel_registerName("setAlertStyle:"), 1); // NSAlertStyleInformational
    
    // Add buttons
    id okButtonText = ((id (*)(id, SEL, const char*))objc_msgSend)(
        (id)NSString,
        sel_registerName("stringWithUTF8String:"),
        ok_text
    );
    ((void (*)(id, SEL, id))objc_msgSend)(alert, sel_registerName("addButtonWithTitle:"), okButtonText);
    
    // Only add cancel button if cancel_button is not null (allows single-button alerts)
    if (cancel_button != NULL) {
        id cancelButtonText = ((id (*)(id, SEL, const char*))objc_msgSend)(
            (id)NSString,
            sel_registerName("stringWithUTF8String:"),
            cancel_text
        );
        ((void (*)(id, SEL, id))objc_msgSend)(alert, sel_registerName("addButtonWithTitle:"), cancelButtonText);
    }
    
    // Show the dialog modally
    long response = ((long (*)(id, SEL))objc_msgSend)(alert, sel_registerName("runModal"));
    printf("Native alert dialog closed with response: %ld\n", response);
    
    // Return true if OK was pressed (response 1000), false if Cancel (response 1001)
    return (response == 1000);
}

// Direct C function for native calls (no bridge involved)
bool platform_show_alert_direct(app_window_t* window, const char* title, const char* message, const char* ok_button, const char* cancel_button) {
    // This is a direct wrapper around the flexible alert function
    // It can be called from C code without involving the bridge system
    return platform_show_alert_with_params(window, title, message, ok_button, cancel_button);
}

#endif // __APPLE__ 