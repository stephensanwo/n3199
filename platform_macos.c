#ifdef __APPLE__

#include <objc/objc.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <CoreGraphics/CoreGraphics.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform.h"
#include "config.h"

// Forward declarations
void menu_action_callback(id self, SEL _cmd, id sender);
void webview_navigation_callback(id self, SEL _cmd, id notification);
void webview_load_finished_callback(id self, SEL _cmd, id notification);

// macOS-specific window structure
struct platform_window {
    id ns_window;
    id ns_app;
    id ns_toolbar;
    id window_delegate;
    id webview;
    id webview_config;
    id script_handler;
};

// Global variables
static id g_app = NULL;

int platform_init(void) {
    if (g_app != NULL) return 0; // Already initialized - success
    
    Class NSApplication = objc_getClass("NSApplication");
    
    // Create shared application
    id app = ((id (*)(id, SEL))objc_msgSend)((id)NSApplication, sel_registerName("sharedApplication"));
    
    if (!app) {
        printf("Failed to initialize macOS platform\n");
        return -1; // Error
    }
    
    // Register menu action method
    Class appClass = object_getClass(app);
    class_addMethod(appClass, sel_registerName("menuAction:"), (IMP)menu_action_callback, "v@:@");
    
    g_app = app;
    
    // Set activation policy to regular app
    ((void (*)(id, SEL, long))objc_msgSend)(g_app, sel_registerName("setActivationPolicy:"), 0);
    
    printf("macOS platform initialized\n");
    return 0; // Success
}

void platform_cleanup(void) {
    g_app = NULL;
    printf("macOS platform cleaned up\n");
}

app_window_t* platform_create_window(const app_configuration_t* config) {
    if (!config) return NULL;
    
    // Ensure platform is initialized (returns 0 on success, -1 on error)
    if (platform_init() < 0) return NULL;  // Only fail on error (-1)
    
    app_window_t* window = malloc(sizeof(app_window_t));
    platform_window_t* native = malloc(sizeof(platform_window_t));
    
    if (!window || !native) {
        free(window);
        free(native);
        return NULL;
    }
    
    window->native_window = (void*)native;
    window->config = config;
    window->is_visible = false;
    window->is_focused = false;
    
    native->ns_app = g_app;
    native->ns_toolbar = NULL;
    native->window_delegate = NULL;
    native->webview = NULL;
    native->webview_config = NULL;
    native->script_handler = NULL;
    
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
    
    // Setup WebView if enabled
    if (config->webview.enabled) {
        platform_setup_webview(window);
    }
    
    if (config->development.debug_mode) {
        printf("macOS window created successfully\n");
    }
    
    return window;
}

void platform_macos_setup_toolbar(app_window_t* window) {
    if (!window || !window->native_window) return;
    
    platform_window_t* native = (platform_window_t*)window->native_window;
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
    
    platform_window_t* native = (platform_window_t*)window->native_window;
    if (!native->ns_toolbar) return;
    
    // This is a simplified version - in a full implementation, you'd need to
    // set up a proper NSToolbarDelegate to handle toolbar items
    if (window->config->development.debug_mode) {
        printf("Adding toolbar item: %s (%s)\n", identifier, title);
    }
}

void platform_show_window(app_window_t* window) {
    if (!window || !window->native_window) return;
    
    platform_window_t* native = (platform_window_t*)window->native_window;
    
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
    
    platform_window_t* native = (platform_window_t*)window->native_window;
    ((void (*)(id, SEL, id))objc_msgSend)(native->ns_window, sel_registerName("orderOut:"), native->ns_window);
    window->is_visible = false;
}

void platform_set_window_title(app_window_t* window, const char* title) {
    if (!window || !window->native_window || !title) return;
    
    platform_window_t* native = (platform_window_t*)window->native_window;
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
    
    platform_window_t* native = (platform_window_t*)window->native_window;
    
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
    
    platform_window_t* native = (platform_window_t*)window->native_window;
    ((void (*)(id, SEL))objc_msgSend)(native->ns_window, sel_registerName("center"));
}

void platform_run_app(app_window_t* window) {
    if (!window || !window->native_window) return;
    
    platform_window_t* native = (platform_window_t*)window->native_window;
    
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
        platform_window_t* native = (platform_window_t*)window->native_window;
        
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
    
    Class NSApplication = objc_getClass("NSApplication");
    Class NSMenu = objc_getClass("NSMenu");
    Class NSMenuItem = objc_getClass("NSMenuItem");
    Class NSString = objc_getClass("NSString");
    
    // Get shared application
    id app = ((id (*)(id, SEL))objc_msgSend)((id)NSApplication, sel_registerName("sharedApplication"));
    
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
    
    // Set the main menu
    ((void (*)(id, SEL, id))objc_msgSend)(app, sel_registerName("setMainMenu:"), mainMenu);
    
    if (window->config->development.debug_mode) {
        printf("macOS menubar configured with %d menus\n", 
               menubar->file_menu.enabled + menubar->edit_menu.enabled + 
               menubar->view_menu.enabled + menubar->window_menu.enabled + 
               menubar->help_menu.enabled);
    }
}

void platform_setup_webview(app_window_t* window) {
    if (!window || !window->native_window || !window->config->webview.enabled) return;
    
    platform_window_t* native = (platform_window_t*)window->native_window;
    
    // Get required classes
    Class WKWebView = objc_getClass("WKWebView");
    Class WKWebViewConfiguration = objc_getClass("WKWebViewConfiguration");
    Class WKPreferences = objc_getClass("WKPreferences");
    Class NSString = objc_getClass("NSString");
    Class NSURL = objc_getClass("NSURL");
    Class NSNumber = objc_getClass("NSNumber");
    
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
        id jsKey = ((id (*)(id, SEL, const char*))objc_msgSend)(
            (id)NSString,
            sel_registerName("stringWithUTF8String:"),
            "javaScriptEnabled"
        );
        id jsValue = ((id (*)(id, SEL, BOOL))objc_msgSend)(
            (id)NSNumber,
            sel_registerName("numberWithBool:"),
            YES
        );
        ((void (*)(id, SEL, id, id))objc_msgSend)(
            preferences,
            sel_registerName("setValue:forKey:"),
            jsValue,
            jsKey
        );
    }
    
    // Enable developer extras if requested
    if (window->config->webview.developer_extras) {
        id devKey = ((id (*)(id, SEL, const char*))objc_msgSend)(
            (id)NSString,
            sel_registerName("stringWithUTF8String:"),
            "developerExtrasEnabled"
        );
        id devValue = ((id (*)(id, SEL, BOOL))objc_msgSend)(
            (id)NSNumber,
            sel_registerName("numberWithBool:"),
            YES
        );
        ((void (*)(id, SEL, id, id))objc_msgSend)(
            preferences,
            sel_registerName("setValue:forKey:"),
            devValue,
            devKey
        );
    }
    
    // Set preferences on configuration
    ((void (*)(id, SEL, id))objc_msgSend)(config, sel_registerName("setPreferences:"), preferences);
    
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
    
    // Load initial URL if specified
    if (strlen(window->config->webview.url) > 0) {
        platform_webview_load_url(window, window->config->webview.url);
    }
    
    if (window->config->development.debug_mode) {
        printf("WebView initialized successfully\n");
    }
}

void platform_webview_load_url(app_window_t* window, const char* url) {
    if (!window || !window->native_window || !url) return;
    
    platform_window_t* native = (platform_window_t*)window->native_window;
    if (!native->webview) return;
    
    Class NSString = objc_getClass("NSString");
    Class NSURL = objc_getClass("NSURL");
    
    // Create URL string
    id urlString = ((id (*)(id, SEL, const char*))objc_msgSend)(
        (id)NSString,
        sel_registerName("stringWithUTF8String:"),
        url
    );
    
    // Create NSURL
    id nsurl = ((id (*)(id, SEL, id))objc_msgSend)(
        (id)NSURL,
        sel_registerName("URLWithString:"),
        urlString
    );
    
    // Create NSURLRequest
    Class NSURLRequest = objc_getClass("NSURLRequest");
    id request = ((id (*)(id, SEL, id))objc_msgSend)(
        (id)NSURLRequest,
        sel_registerName("requestWithURL:"),
        nsurl
    );
    
    // Load request
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
    
    platform_window_t* native = (platform_window_t*)window->native_window;
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
    if (strlen(window->config->webview.url) > 0) {
        Class NSURL = objc_getClass("NSURL");
        id urlString = ((id (*)(id, SEL, const char*))objc_msgSend)(
            (id)NSString,
            sel_registerName("stringWithUTF8String:"),
            window->config->webview.url
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
    
    platform_window_t* native = (platform_window_t*)window->native_window;
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

#endif // __APPLE__ 