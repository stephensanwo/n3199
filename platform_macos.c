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

// Function declarations
static id create_script_message_handler(app_window_t* window);
static void platform_send_sidebar_state_to_webview(app_window_t* window, bool visible);

// Forward declarations
static void cleanup_main_window(void);
static void signal_handler(int sig);
static Class create_window_delegate_class(void);
static void setup_modern_toolbar(app_window_t* window, platform_native_window_t* native);

// Forward declarations for delegate methods
static NSInteger outline_view_number_of_children(id self, SEL _cmd, id outlineView, id item);
static id outline_view_child_of_item(id self, SEL _cmd, id outlineView, NSInteger index, id item);
static BOOL outline_view_is_item_expandable(id self, SEL _cmd, id outlineView, id item);
static id outline_view_view_for_table_column(id self, SEL _cmd, id outlineView, id tableColumn, id item);
static void outline_view_selection_did_change(id self, SEL _cmd, id notification);
static BOOL split_view_can_collapse_subview(id self, SEL _cmd, id splitView, id subview);
static CGFloat split_view_constrain_min_coordinate(id self, SEL _cmd, id splitView, CGFloat proposedMin, NSInteger dividerIndex);
static CGFloat split_view_constrain_max_coordinate(id self, SEL _cmd, id splitView, CGFloat proposedMax, NSInteger dividerIndex);
static BOOL split_view_should_adjust_size_of_subview(id self, SEL _cmd, id splitView, id subview);

// Toolbar delegate methods
static id toolbar_item_for_identifier(id self, SEL _cmd, id toolbar, id identifier, BOOL willBeInserted);
static id toolbar_default_item_identifiers(id self, SEL _cmd, id toolbar);
static id toolbar_allowed_item_identifiers(id self, SEL _cmd, id toolbar);
static Class create_toolbar_delegate_class(void);

// Outline view data source implementation
static NSInteger outline_view_number_of_children(id self, SEL _cmd, id outlineView, id item) {
    (void)self; (void)_cmd; (void)outlineView; // Suppress unused warnings
    
    // Get window from stored data
    id windowValue = objc_getAssociatedObject(self, "window");
    app_window_t* window = NULL;
    if (windowValue) {
        window = (app_window_t*)((void* (*)(id, SEL))objc_msgSend)(windowValue, sel_registerName("pointerValue"));
    }
    if (!window || item) {
        // Root level items only for now
        return window ? window->config->macos.sidebar.item_count : 0;
    }
    
    return 0; // No children for items (flat structure)
}

static id outline_view_child_of_item(id self, SEL _cmd, id outlineView, NSInteger index, id item) {
    (void)self; (void)_cmd; (void)outlineView; // Suppress unused warnings
    
    // Get window from stored data
    id windowValue = objc_getAssociatedObject(self, "window");
    app_window_t* window = NULL;
    if (windowValue) {
        window = (app_window_t*)((void* (*)(id, SEL))objc_msgSend)(windowValue, sel_registerName("pointerValue"));
    }
    if (!window || item) return nil; // Only handle root level items
    
    if (index >= 0 && index < window->config->macos.sidebar.item_count) {
        // Create a string representing the item
        Class NSString = objc_getClass("NSString");
        return ((id (*)(id, SEL, const char*))objc_msgSend)(
            (id)NSString,
            sel_registerName("stringWithUTF8String:"),
            window->config->macos.sidebar.items[index].title
        );
    }
    
    return nil;
}

static BOOL outline_view_is_item_expandable(id self, SEL _cmd, id outlineView, id item) {
    (void)self; (void)_cmd; (void)outlineView; (void)item; // Suppress unused warnings
    return NO; // Flat structure for now
}

static id outline_view_view_for_table_column(id self, SEL _cmd, id outlineView, id tableColumn, id item) {
    (void)self; (void)_cmd; (void)tableColumn; // Suppress unused warnings
    
    Class NSTableCellView = objc_getClass("NSTableCellView");
    Class NSTextField = objc_getClass("NSTextField");
    Class NSColor = objc_getClass("NSColor");
    Class NSString = objc_getClass("NSString");
    
    // Try to reuse existing cell
    id cellIdentifier = ((id (*)(id, SEL, const char*))objc_msgSend)(
        (id)NSString,
        sel_registerName("stringWithUTF8String:"),
        "DataCell"
    );
    
    id cellView = ((id (*)(id, SEL, id, id))objc_msgSend)(
        outlineView,
        sel_registerName("makeViewWithIdentifier:owner:"),
        cellIdentifier,
        self
    );
    
    if (!cellView) {
        // Create new cell view
        cellView = ((id (*)(id, SEL))objc_msgSend)(
            ((id (*)(id, SEL))objc_msgSend)((id)NSTableCellView, sel_registerName("alloc")),
            sel_registerName("init")
        );
        
        // Set identifier
        ((void (*)(id, SEL, id))objc_msgSend)(cellView, sel_registerName("setIdentifier:"), cellIdentifier);
        
        // Create text field
        id textField = ((id (*)(id, SEL))objc_msgSend)(
            ((id (*)(id, SEL))objc_msgSend)((id)NSTextField, sel_registerName("alloc")),
            sel_registerName("init")
        );
        
        // Configure text field
        ((void (*)(id, SEL, BOOL))objc_msgSend)(textField, sel_registerName("setTranslatesAutoresizingMaskIntoConstraints:"), NO);
        ((void (*)(id, SEL, BOOL))objc_msgSend)(textField, sel_registerName("setBordered:"), NO);
        ((void (*)(id, SEL, BOOL))objc_msgSend)(textField, sel_registerName("setEditable:"), NO);
        ((void (*)(id, SEL, BOOL))objc_msgSend)(textField, sel_registerName("setSelectable:"), NO);
        
        // Set background color to clear
        id clearColor = ((id (*)(id, SEL))objc_msgSend)((id)NSColor, sel_registerName("clearColor"));
        ((void (*)(id, SEL, id))objc_msgSend)(textField, sel_registerName("setBackgroundColor:"), clearColor);
        
        // Add to cell view
        ((void (*)(id, SEL, id))objc_msgSend)(cellView, sel_registerName("addSubview:"), textField);
        ((void (*)(id, SEL, id))objc_msgSend)(cellView, sel_registerName("setTextField:"), textField);
        
        // Set up constraints (simplified - just center the text)
        Class NSLayoutConstraint = objc_getClass("NSLayoutConstraint");
        if (NSLayoutConstraint) {
            // Leading constraint
            id leadingAnchor = ((id (*)(id, SEL))objc_msgSend)(textField, sel_registerName("leadingAnchor"));
            id cellLeadingAnchor = ((id (*)(id, SEL))objc_msgSend)(cellView, sel_registerName("leadingAnchor"));
            id leadingConstraint = ((id (*)(id, SEL, id, double))objc_msgSend)(
                leadingAnchor,
                sel_registerName("constraintEqualToAnchor:constant:"),
                cellLeadingAnchor,
                8.0
            );
            ((void (*)(id, SEL, BOOL))objc_msgSend)(leadingConstraint, sel_registerName("setActive:"), YES);
            
            // Center Y constraint
            id centerYAnchor = ((id (*)(id, SEL))objc_msgSend)(textField, sel_registerName("centerYAnchor"));
            id cellCenterYAnchor = ((id (*)(id, SEL))objc_msgSend)(cellView, sel_registerName("centerYAnchor"));
            id centerYConstraint = ((id (*)(id, SEL, id))objc_msgSend)(
                centerYAnchor,
                sel_registerName("constraintEqualToAnchor:"),
                cellCenterYAnchor
            );
            ((void (*)(id, SEL, BOOL))objc_msgSend)(centerYConstraint, sel_registerName("setActive:"), YES);
        }
    }
    
    // Set the text content
    id textField = ((id (*)(id, SEL))objc_msgSend)(cellView, sel_registerName("textField"));
    if (textField && item) {
        const char* itemText = ((const char* (*)(id, SEL))objc_msgSend)(item, sel_registerName("UTF8String"));
        if (itemText) {
            ((void (*)(id, SEL, id))objc_msgSend)(textField, sel_registerName("setStringValue:"), item);
        }
    }
    
    return cellView;
}

static void outline_view_selection_did_change(id self, SEL _cmd, id notification) {
    (void)self; (void)_cmd; // Suppress unused warnings
    
    // Get outline view from notification
    id outlineView = ((id (*)(id, SEL))objc_msgSend)(notification, sel_registerName("object"));
    if (!outlineView) return;
    
    // Get selected row
    NSInteger selectedRow = ((NSInteger (*)(id, SEL))objc_msgSend)(outlineView, sel_registerName("selectedRow"));
    if (selectedRow >= 0) {
        // Get window from stored data
        id windowValue = objc_getAssociatedObject(self, "window");
        app_window_t* window = NULL;
        if (windowValue) {
            window = (app_window_t*)((void* (*)(id, SEL))objc_msgSend)(windowValue, sel_registerName("pointerValue"));
        }
        if (window && window->config->development.debug_mode) {
            printf("Native sidebar item selected at row: %ld\n", (long)selectedRow);
        }
        
        // Handle selection
        if (window && selectedRow < window->config->macos.sidebar.item_count) {
            const char* action = window->config->macos.sidebar.items[selectedRow].action;
            if (action && strlen(action) > 0) {
                printf("Executing sidebar action: %s\n", action);
            }
        }
    }
}

// Split view delegate implementation
static BOOL split_view_can_collapse_subview(id self, SEL _cmd, id splitView, id subview) {
    (void)self; (void)_cmd; // Suppress unused warnings
    
    // Allow the sidebar (first subview) to be collapsed
    id subviews = ((id (*)(id, SEL))objc_msgSend)(splitView, sel_registerName("subviews"));
    NSUInteger count = ((NSUInteger (*)(id, SEL))objc_msgSend)(subviews, sel_registerName("count"));
    
    if (count > 0) {
        id firstSubview = ((id (*)(id, SEL, NSUInteger))objc_msgSend)(subviews, sel_registerName("objectAtIndex:"), 0);
        return (subview == firstSubview);
    }
    
    return NO;
}

static CGFloat split_view_constrain_min_coordinate(id self, SEL _cmd, id splitView, CGFloat proposedMin, NSInteger dividerIndex) {
    (void)self; (void)_cmd; (void)splitView; (void)dividerIndex; // Suppress unused warnings
    return fmax(proposedMin, 0.0); // Allow complete collapse to 0
}

static CGFloat split_view_constrain_max_coordinate(id self, SEL _cmd, id splitView, CGFloat proposedMax, NSInteger dividerIndex) {
    (void)self; (void)_cmd; (void)dividerIndex; // Suppress unused warnings
    
    CGRect frame = ((CGRect (*)(id, SEL))objc_msgSend)(splitView, sel_registerName("frame"));
    return fmin(proposedMax, frame.size.width / 3.0); // Maximum sidebar width (1/3 of total)
}

static BOOL split_view_should_adjust_size_of_subview(id self, SEL _cmd, id splitView, id subview) {
    (void)self; (void)_cmd; // Suppress unused warnings
    
    // Don't auto-resize the sidebar when window resizes
    id subviews = ((id (*)(id, SEL))objc_msgSend)(splitView, sel_registerName("subviews"));
    NSUInteger count = ((NSUInteger (*)(id, SEL))objc_msgSend)(subviews, sel_registerName("count"));
    
    if (count > 0) {
        id firstSubview = ((id (*)(id, SEL, NSUInteger))objc_msgSend)(subviews, sel_registerName("objectAtIndex:"), 0);
        return (subview != firstSubview);
    }
    
    return YES;
}

// Create outline view delegate/data source class
static Class create_outline_view_delegate_class(void) {
    static Class OutlineViewDelegateClass = nil;
    if (OutlineViewDelegateClass == nil) {
        OutlineViewDelegateClass = objc_allocateClassPair(objc_getClass("NSObject"), "OutlineViewDelegate", 0);
        
        // Add data source methods
        class_addMethod(OutlineViewDelegateClass, sel_registerName("outlineView:numberOfChildrenOfItem:"),
                       (IMP)outline_view_number_of_children, "l@:@@");
        class_addMethod(OutlineViewDelegateClass, sel_registerName("outlineView:child:ofItem:"),
                       (IMP)outline_view_child_of_item, "@@:@l@");
        class_addMethod(OutlineViewDelegateClass, sel_registerName("outlineView:isItemExpandable:"),
                       (IMP)outline_view_is_item_expandable, "c@:@@");
        
        // Add delegate methods
        class_addMethod(OutlineViewDelegateClass, sel_registerName("outlineView:viewForTableColumn:item:"),
                       (IMP)outline_view_view_for_table_column, "@@:@@@");
        class_addMethod(OutlineViewDelegateClass, sel_registerName("outlineViewSelectionDidChange:"),
                       (IMP)outline_view_selection_did_change, "v@:@");
        
        objc_registerClassPair(OutlineViewDelegateClass);
    }
    return OutlineViewDelegateClass;
}

// Create split view delegate class
static Class create_split_view_delegate_class(void) {
    static Class SplitViewDelegateClass = nil;
    if (SplitViewDelegateClass == nil) {
        SplitViewDelegateClass = objc_allocateClassPair(objc_getClass("NSObject"), "SplitViewDelegate", 0);
        
        // Add split view delegate methods
        class_addMethod(SplitViewDelegateClass, sel_registerName("splitView:canCollapseSubview:"),
                       (IMP)split_view_can_collapse_subview, "c@:@@");
        class_addMethod(SplitViewDelegateClass, sel_registerName("splitView:constrainMinCoordinate:ofSubviewAt:"),
                       (IMP)split_view_constrain_min_coordinate, "f@:@fi");
        class_addMethod(SplitViewDelegateClass, sel_registerName("splitView:constrainMaxCoordinate:ofSubviewAt:"),
                       (IMP)split_view_constrain_max_coordinate, "f@:@fi");
        class_addMethod(SplitViewDelegateClass, sel_registerName("splitView:shouldAdjustSizeOfSubview:"),
                       (IMP)split_view_should_adjust_size_of_subview, "c@:@@");
        
        objc_registerClassPair(SplitViewDelegateClass);
    }
    return SplitViewDelegateClass;
}

// Create toolbar delegate class
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

// Setup native split view with sidebar
static void setup_native_sidebar_split_view(app_window_t* window, platform_native_window_t* native) {
    Class NSSplitView = objc_getClass("NSSplitView");
    Class NSScrollView = objc_getClass("NSScrollView");
    Class NSOutlineView = objc_getClass("NSOutlineView");
    Class NSTableColumn = objc_getClass("NSTableColumn");
    Class NSString = objc_getClass("NSString");
    
    printf("Setting up native macOS sidebar with NSSplitView and NSOutlineView\n");
    
    // Create the main split view
    id splitView = ((id (*)(id, SEL))objc_msgSend)(
        ((id (*)(id, SEL))objc_msgSend)((id)NSSplitView, sel_registerName("alloc")),
        sel_registerName("init")
    );
    
    // Configure split view for sidebar behavior
    ((void (*)(id, SEL, BOOL))objc_msgSend)(splitView, sel_registerName("setTranslatesAutoresizingMaskIntoConstraints:"), NO);
    ((void (*)(id, SEL, BOOL))objc_msgSend)(splitView, sel_registerName("setVertical:"), YES); // Side by side
    ((void (*)(id, SEL, long))objc_msgSend)(splitView, sel_registerName("setDividerStyle:"), 0); // NO divider (was 1)
    
    // Hide the divider completely
    ((void (*)(id, SEL, BOOL))objc_msgSend)(splitView, sel_registerName("setArrangesAllSubviews:"), YES);
    
    // Create and set split view delegate
    Class SplitViewDelegateClass = create_split_view_delegate_class();
    id splitViewDelegate = ((id (*)(id, SEL))objc_msgSend)(
        ((id (*)(id, SEL))objc_msgSend)((id)SplitViewDelegateClass, sel_registerName("alloc")),
        sel_registerName("init")
    );
    ((void (*)(id, SEL, id))objc_msgSend)(splitView, sel_registerName("setDelegate:"), splitViewDelegate);
    
    // Create scroll view for the sidebar
    id scrollView = ((id (*)(id, SEL))objc_msgSend)(
        ((id (*)(id, SEL))objc_msgSend)((id)NSScrollView, sel_registerName("alloc")),
        sel_registerName("init")
    );
    
    // Configure scroll view
    ((void (*)(id, SEL, BOOL))objc_msgSend)(scrollView, sel_registerName("setTranslatesAutoresizingMaskIntoConstraints:"), NO);
    ((void (*)(id, SEL, BOOL))objc_msgSend)(scrollView, sel_registerName("setHasVerticalScroller:"), YES);
    ((void (*)(id, SEL, BOOL))objc_msgSend)(scrollView, sel_registerName("setHasHorizontalScroller:"), NO);
    ((void (*)(id, SEL, BOOL))objc_msgSend)(scrollView, sel_registerName("setAutohidesScrollers:"), YES);
    ((void (*)(id, SEL, long))objc_msgSend)(scrollView, sel_registerName("setBorderType:"), 0); // No border
    
    // Create outline view
    id outlineView = ((id (*)(id, SEL))objc_msgSend)(
        ((id (*)(id, SEL))objc_msgSend)((id)NSOutlineView, sel_registerName("alloc")),
        sel_registerName("init")
    );
    
    // Configure outline view for native sidebar appearance
    ((void (*)(id, SEL, BOOL))objc_msgSend)(outlineView, sel_registerName("setTranslatesAutoresizingMaskIntoConstraints:"), NO);
    ((void (*)(id, SEL, id))objc_msgSend)(outlineView, sel_registerName("setHeaderView:"), nil); // Hide headers
    ((void (*)(id, SEL, long))objc_msgSend)(outlineView, sel_registerName("setSelectionHighlightStyle:"), 1); // Source list style
    ((void (*)(id, SEL, BOOL))objc_msgSend)(outlineView, sel_registerName("setFloatsGroupRows:"), NO);
    
    // Create table column
    id columnIdentifier = ((id (*)(id, SEL, const char*))objc_msgSend)(
        (id)NSString,
        sel_registerName("stringWithUTF8String:"),
        "MainColumn"
    );
    
    id column = ((id (*)(id, SEL, id))objc_msgSend)(
        ((id (*)(id, SEL))objc_msgSend)((id)NSTableColumn, sel_registerName("alloc")),
        sel_registerName("initWithIdentifier:"),
        columnIdentifier
    );
    
    id columnTitle = ((id (*)(id, SEL, const char*))objc_msgSend)(
        (id)NSString,
        sel_registerName("stringWithUTF8String:"),
        "Sidebar"
    );
    ((void (*)(id, SEL, id))objc_msgSend)(column, sel_registerName("setTitle:"), columnTitle);
    
    // Add column to outline view
    ((void (*)(id, SEL, id))objc_msgSend)(outlineView, sel_registerName("addTableColumn:"), column);
    ((void (*)(id, SEL, id))objc_msgSend)(outlineView, sel_registerName("setOutlineTableColumn:"), column);
    
    // Create and set outline view data source/delegate
    Class OutlineViewDelegateClass = create_outline_view_delegate_class();
    id outlineViewDelegate = ((id (*)(id, SEL))objc_msgSend)(
        ((id (*)(id, SEL))objc_msgSend)((id)OutlineViewDelegateClass, sel_registerName("alloc")),
        sel_registerName("init")
    );
    
    // Store the window pointer in the delegate for access
    Class NSValue = objc_getClass("NSValue");
    id windowValue = ((id (*)(id, SEL, void*))objc_msgSend)(
        (id)NSValue,
        sel_registerName("valueWithPointer:"),
        window
    );
    objc_setAssociatedObject(outlineViewDelegate, "window", windowValue, OBJC_ASSOCIATION_RETAIN);
    
    // Set data source and delegate
    ((void (*)(id, SEL, id))objc_msgSend)(outlineView, sel_registerName("setDataSource:"), outlineViewDelegate);
    ((void (*)(id, SEL, id))objc_msgSend)(outlineView, sel_registerName("setDelegate:"), outlineViewDelegate);
    
    // Set outline view as document view of scroll view
    ((void (*)(id, SEL, id))objc_msgSend)(scrollView, sel_registerName("setDocumentView:"), outlineView);
    
    // Configure webview for split view
    ((void (*)(id, SEL, BOOL))objc_msgSend)(native->webview, sel_registerName("setTranslatesAutoresizingMaskIntoConstraints:"), NO);
    
    // Add subviews to split view in correct order (sidebar first, then webview)
    ((void (*)(id, SEL, id))objc_msgSend)(splitView, sel_registerName("addSubview:"), scrollView);
    ((void (*)(id, SEL, id))objc_msgSend)(splitView, sel_registerName("addSubview:"), native->webview);
    
    // Set split view as the window's content view
    ((void (*)(id, SEL, id))objc_msgSend)(native->ns_window, sel_registerName("setContentView:"), splitView);
    
    // Configure content hugging priorities
    Class NSView = objc_getClass("NSView");
    if (((BOOL (*)(id, SEL, id))objc_msgSend)(scrollView, sel_registerName("isKindOfClass:"), (id)NSView)) {
        // Sidebar should maintain its width (high hugging priority)
        ((void (*)(id, SEL, float, long))objc_msgSend)(scrollView, sel_registerName("setContentHuggingPriority:forOrientation:"), 251.0, 0);
    }
    if (((BOOL (*)(id, SEL, id))objc_msgSend)(native->webview, sel_registerName("isKindOfClass:"), (id)NSView)) {
        // WebView should expand to fill remaining space (low hugging priority)
        ((void (*)(id, SEL, float, long))objc_msgSend)(native->webview, sel_registerName("setContentHuggingPriority:forOrientation:"), 249.0, 0);
    }
    
    // Set initial sidebar width (200 points)
    ((void (*)(id, SEL, CGFloat, NSInteger))objc_msgSend)(splitView, sel_registerName("setPosition:ofDividerAtIndex:"), 200.0, 0);
    
    // Store references for later use
    native->split_view_controller = splitView; // Reusing the field but it's actually NSSplitView now
    native->sidebar_view_controller = scrollView; // Store the sidebar's scroll view
    
    // Set initial visibility state
    native->sidebar_visible = !window->config->macos.sidebar.start_collapsed;
    
    // Reload outline view data
    ((void (*)(id, SEL))objc_msgSend)(outlineView, sel_registerName("reloadData"));
    
    // Handle initial collapsed state
    if (window->config->macos.sidebar.start_collapsed) {
        // Properly collapse the sidebar initially
        ((void (*)(id, SEL, CGFloat, NSInteger))objc_msgSend)(splitView, sel_registerName("setPosition:ofDividerAtIndex:"), 0.0, 0);
        native->sidebar_visible = false;
        printf("Sidebar initially collapsed as configured\n");
    }
    
    if (window->config->development.debug_mode) {
        printf("Native macOS sidebar setup completed\n");
        printf("- Using NSSplitView with NSOutlineView\n");
        printf("- Sidebar %s, width: %s\n", native->sidebar_visible ? "visible" : "collapsed", native->sidebar_visible ? "200pt" : "0pt");
        printf("- %d navigation items configured\n", window->config->macos.sidebar.item_count);
        printf("- No divider line or resize handle\n");
    }
}

// ============================================================================
// SIDEBAR MANAGEMENT FUNCTIONS (Updated for native NSSplitView)
// ============================================================================

void platform_macos_toggle_sidebar(app_window_t* window) {
    if (!window || !window->native_window) {
        printf("Error: Invalid window or native window\n");
        return;
    }
    
    platform_native_window_t* native = (platform_native_window_t*)window->native_window;
    
    printf("Toggling native sidebar...\n");
    
    // Get the split view (stored in split_view_controller field)
    id splitView = native->split_view_controller;
    if (!splitView) {
        printf("Error: No split view found\n");
        return;
    }
    
    // Check current position to determine state
    CGFloat currentPosition = ((CGFloat (*)(id, SEL, NSInteger))objc_msgSend)(splitView, sel_registerName("positionOfDividerAtIndex:"), 0);
    printf("Current divider position: %.1f\n", currentPosition);
    
    // Toggle based on current position
    if (currentPosition <= 1.0) {
        printf("Showing native sidebar...\n");
        platform_macos_show_sidebar(window);
    } else {
        printf("Hiding native sidebar...\n");
        platform_macos_hide_sidebar(window);
    }
}

void platform_macos_show_sidebar(app_window_t* window) {
    if (!window || !window->native_window) return;
    
    platform_native_window_t* native = (platform_native_window_t*)window->native_window;
    
    printf("Showing native sidebar...\n");
    
    // Get the split view
    id splitView = native->split_view_controller;
    if (!splitView) {
        printf("Error: No split view found\n");
        return;
    }
    
    // Simply set the divider position without animation to avoid block syntax issues
    ((void (*)(id, SEL, CGFloat, NSInteger))objc_msgSend)(splitView, sel_registerName("setPosition:ofDividerAtIndex:"), 200.0, 0);
    
    // Update state
    native->sidebar_visible = true;
    
    // Send state to webview
    platform_send_sidebar_state_to_webview(window, true);
    
    if (window->config->development.debug_mode) {
        printf("Native sidebar shown successfully (position: 200pt)\n");
    }
}

void platform_macos_hide_sidebar(app_window_t* window) {
    if (!window || !window->native_window) return;
    
    platform_native_window_t* native = (platform_native_window_t*)window->native_window;
    
    printf("Hiding native sidebar...\n");
    
    // Get the split view
    id splitView = native->split_view_controller;
    if (!splitView) {
        printf("Error: No split view found\n");
        return;
    }
    
    // Simply set the divider position to 0 to completely collapse the sidebar
    ((void (*)(id, SEL, CGFloat, NSInteger))objc_msgSend)(splitView, sel_registerName("setPosition:ofDividerAtIndex:"), 0.0, 0);
    
    // Update state
    native->sidebar_visible = false;
    
    // Send state to webview
    platform_send_sidebar_state_to_webview(window, false);
    
    if (window->config->development.debug_mode) {
        printf("Native sidebar hidden successfully (position: 0pt)\n");
    }
}

void platform_handle_sidebar_action(const char* action, app_window_t* window) {
    if (!action || !window) return;
    
    if (strcmp(action, "toggle") == 0) {
        platform_macos_toggle_sidebar(window);
    } else if (strcmp(action, "show") == 0) {
        platform_macos_show_sidebar(window);
    } else if (strcmp(action, "hide") == 0) {
        platform_macos_hide_sidebar(window);
    }
}

// Helper function to send sidebar state to webview
static void platform_send_sidebar_state_to_webview(app_window_t* window, bool visible) {
    if (!window || !window->native_window) return;
    
    platform_native_window_t* native = (platform_native_window_t*)window->native_window;
    
    // Create JavaScript to notify webview of sidebar state
    char js_code[256];
    snprintf(js_code, sizeof(js_code), 
             "if (window.nativeSidebar) { window.nativeSidebar.onStateChange(%s); }",
             visible ? "true" : "false");
    
    // Convert to NSString
    id js_string = ((id (*)(id, SEL, const char*))objc_msgSend)((id)objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"), js_code);
    
    // Execute JavaScript in webview
    ((void (*)(id, SEL, id, id))objc_msgSend)(native->webview, sel_registerName("evaluateJavaScript:completionHandler:"), js_string, nil);
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
    
    // Handle common menu actions
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
    } else if (strcmp(action, "toggle_sidebar") == 0) {
        printf("Toggling sidebar...\n");
        // Get the global main window and toggle sidebar
        if (g_main_window) {
            platform_macos_toggle_sidebar(g_main_window);
        }
    } else {
        printf("Unknown menu action: %s\n", action);
    }
}

// ============================================================================
// MENU SYSTEM (keeping existing implementation)
// ============================================================================

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
    
    // Store WebView in native window structure
    native->webview = webview;
    native->webview_config = config;

    // Setup native split view with sidebar (NEW IMPLEMENTATION)
    setup_native_sidebar_split_view(window, native);
    
    // Setup modern toolbar
    setup_modern_toolbar(window, native);
    
    // Load initial URL if available
    const char* url = get_webview_url(&window->config->webview.framework);
    if (url && strlen(url) > 0) {
        platform_webview_load_url(window, url);
    }
    
    if (window->config->development.debug_mode) {
        printf("Native WebView with NSSplitView and NSOutlineView sidebar initialized successfully\n");
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

// Sidebar toggle callback
static void toolbar_sidebar_toggle_callback(id self, SEL _cmd, id sender) {
    (void)self; (void)_cmd; // Suppress unused parameter warnings
    
    printf("Toolbar sidebar toggle button clicked\n");
    
    // Get the window from the sender's represented object
    id represented_object = ((id (*)(id, SEL))objc_msgSend)(sender, sel_registerName("representedObject"));
    if (represented_object) {
        app_window_t* window = (app_window_t*)((void* (*)(id, SEL))objc_msgSend)(represented_object, sel_registerName("pointerValue"));
        if (window) {
            printf("Found window pointer, calling sidebar toggle\n");
            platform_macos_toggle_sidebar(window);
        } else {
            printf("Error: Window pointer is null\n");
        }
    } else {
        printf("Error: No represented object found on button\n");
    }
}

// ============================================================================
// MODERN TOOLBAR SETUP
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
    
    if (!NSToolbar) {
        printf("NSToolbar class not available\n");
        return;
    }
    
    printf("Setting up modern macOS toolbar\n");
    
    // Create toolbar identifier
    id toolbarIdentifier = ((id (*)(id, SEL, const char*))objc_msgSend)(
        (id)NSString,
        sel_registerName("stringWithUTF8String:"),
        "MainToolbar"
    );
    
    // Create toolbar
    id toolbar = ((id (*)(id, SEL))objc_msgSend)((id)NSToolbar, sel_registerName("alloc"));
    toolbar = ((id (*)(id, SEL, id))objc_msgSend)(
        toolbar,
        sel_registerName("initWithIdentifier:"),
        toolbarIdentifier
    );
    
    // Configure toolbar
    ((void (*)(id, SEL, BOOL))objc_msgSend)(toolbar, sel_registerName("setAllowsUserCustomization:"), NO);
    ((void (*)(id, SEL, BOOL))objc_msgSend)(toolbar, sel_registerName("setAutosavesConfiguration:"), NO);
    ((void (*)(id, SEL, long))objc_msgSend)(toolbar, sel_registerName("setDisplayMode:"), 0); // NSToolbarDisplayModeDefault
    
    // Create and set toolbar delegate
    Class ToolbarDelegateClass = create_toolbar_delegate_class();
    id toolbarDelegate = ((id (*)(id, SEL))objc_msgSend)(
        ((id (*)(id, SEL))objc_msgSend)((id)ToolbarDelegateClass, sel_registerName("alloc")),
        sel_registerName("init")
    );
    
    // Store window pointer in delegate
    object_setInstanceVariable(toolbarDelegate, "window", window);
    
    // Set delegate
    ((void (*)(id, SEL, id))objc_msgSend)(toolbar, sel_registerName("setDelegate:"), toolbarDelegate);
    
    // Set toolbar on window
    ((void (*)(id, SEL, id))objc_msgSend)(native->ns_window, sel_registerName("setToolbar:"), toolbar);
    
    // Store toolbar reference
    native->toolbar = toolbar;
    
    if (window->config->development.debug_mode) {
        printf("Modern macOS toolbar setup completed\n");
    }
}

// ============================================================================
// TOOLBAR DELEGATE CLASS
// ============================================================================

// Create toolbar item for identifier
static id toolbar_item_for_identifier(id self, SEL _cmd, id toolbar, id identifier, BOOL willBeInserted) {
    (void)toolbar; (void)willBeInserted; // Suppress unused parameter warnings
    
    // Get window from delegate
    app_window_t* window;
    object_getInstanceVariable(self, "window", (void**)&window);
    
    if (!window) return nil;
    
    Class NSString = objc_getClass("NSString");
    Class NSToolbarItem = objc_getClass("NSToolbarItem");
    Class NSButton = objc_getClass("NSButton");
    Class NSImage = objc_getClass("NSImage");
    
    // Get identifier string
    const char* identifierStr = ((const char* (*)(id, SEL))objc_msgSend)(identifier, sel_registerName("UTF8String"));
    
    if (strcmp(identifierStr, "SidebarToggle") == 0) {
        // Create sidebar toggle toolbar item
        id toolbarItem = ((id (*)(id, SEL))objc_msgSend)((id)NSToolbarItem, sel_registerName("alloc"));
        toolbarItem = ((id (*)(id, SEL, id))objc_msgSend)(toolbarItem, sel_registerName("initWithItemIdentifier:"), identifier);
        
        // Create button with proper system size
        id button = ((id (*)(id, SEL))objc_msgSend)((id)NSButton, sel_registerName("alloc"));
        button = ((id (*)(id, SEL, CGRect))objc_msgSend)(button, sel_registerName("initWithFrame:"), CGRectMake(0, 0, 28, 28));
        
        // Set button type and style
        ((void (*)(id, SEL, long))objc_msgSend)(button, sel_registerName("setButtonType:"), 0); // NSMomentaryPushInButton
        ((void (*)(id, SEL, BOOL))objc_msgSend)(button, sel_registerName("setBordered:"), NO);
        ((void (*)(id, SEL, long))objc_msgSend)(button, sel_registerName("setBezelStyle:"), 15); // NSBezelStyleAccessoryBar
        
        // Use SF Symbol for sidebar icon with proper configuration
        id sidebarIcon = nil;
        Class NSImageSymbolConfiguration = objc_getClass("NSImageSymbolConfiguration");
        if (NSImageSymbolConfiguration) {
            // Create symbol configuration for medium size
            id symbolConfig = ((id (*)(id, SEL, long))objc_msgSend)(
                (id)NSImageSymbolConfiguration,
                sel_registerName("configurationWithScale:"),
                2  // NSImageSymbolScaleMedium
            );
            
            // Create the sidebar.left SF Symbol
            id symbolName = ((id (*)(id, SEL, const char*))objc_msgSend)(
                (id)NSString,
                sel_registerName("stringWithUTF8String:"),
                "sidebar.left"
            );
            
            sidebarIcon = ((id (*)(id, SEL, id, id))objc_msgSend)(
                (id)NSImage,
                sel_registerName("imageWithSystemSymbolName:accessibilityDescription:"),
                symbolName,
                nil
            );
            
            if (sidebarIcon) {
                // Apply the symbol configuration
                sidebarIcon = ((id (*)(id, SEL, id))objc_msgSend)(
                    sidebarIcon,
                    sel_registerName("imageWithSymbolConfiguration:"),
                    symbolConfig
                );
            }
        }
        
        // Fallback to text if SF Symbol not available
        if (!sidebarIcon) {
            id buttonTitle = ((id (*)(id, SEL, const char*))objc_msgSend)(
                (id)NSString,
                sel_registerName("stringWithUTF8String:"),
                "☰"
            );
            ((void (*)(id, SEL, id))objc_msgSend)(button, sel_registerName("setTitle:"), buttonTitle);
        } else {
            ((void (*)(id, SEL, id))objc_msgSend)(button, sel_registerName("setImage:"), sidebarIcon);
            ((void (*)(id, SEL, BOOL))objc_msgSend)(button, sel_registerName("setImageHugsTitle:"), YES);
        }
        
        // Store window pointer in button's represented object
        Class NSValue = objc_getClass("NSValue");
        id windowPtr = ((id (*)(id, SEL, void*))objc_msgSend)((id)NSValue, sel_registerName("valueWithPointer:"), window);
        ((void (*)(id, SEL, id))objc_msgSend)(button, sel_registerName("setRepresentedObject:"), windowPtr);
        
        // Set button action - use the button itself as target and a selector we'll add
        ((void (*)(id, SEL, id))objc_msgSend)(button, sel_registerName("setTarget:"), button);
        ((void (*)(id, SEL, SEL))objc_msgSend)(button, sel_registerName("setAction:"), sel_registerName("toggleSidebar:"));
        
        // Add the action method to the button's class dynamically
        Class buttonClass = object_getClass(button);
        if (!class_getInstanceMethod(buttonClass, sel_registerName("toggleSidebar:"))) {
            class_addMethod(buttonClass, sel_registerName("toggleSidebar:"), (IMP)toolbar_sidebar_toggle_callback, "v@:@");
            
            if (window->config->development.debug_mode) {
                printf("Added toggleSidebar: method to button class\n");
            }
        }
        
        // Set toolbar item properties
        id tooltip = ((id (*)(id, SEL, const char*))objc_msgSend)(
            (id)NSString,
            sel_registerName("stringWithUTF8String:"),
            "Toggle Sidebar"
        );
        ((void (*)(id, SEL, id))objc_msgSend)(toolbarItem, sel_registerName("setToolTip:"), tooltip);
        
        // Set button as toolbar item view
        ((void (*)(id, SEL, id))objc_msgSend)(toolbarItem, sel_registerName("setView:"), button);
        
        // Set proper size constraints for system toolbar
        CGSize minSize = CGSizeMake(28, 28);
        CGSize maxSize = CGSizeMake(28, 28);
        ((void (*)(id, SEL, CGSize))objc_msgSend)(toolbarItem, sel_registerName("setMinSize:"), minSize);
        ((void (*)(id, SEL, CGSize))objc_msgSend)(toolbarItem, sel_registerName("setMaxSize:"), maxSize);
        
        if (window->config->development.debug_mode) {
            printf("Sidebar toggle toolbar item created with action\n");
        }
        
        return toolbarItem;
    }
    
    return nil;
}

// Default toolbar item identifiers
static id toolbar_default_item_identifiers(id self, SEL _cmd, id toolbar) {
    (void)self; (void)_cmd; (void)toolbar; // Suppress unused warnings
    
    Class NSArray = objc_getClass("NSArray");
    Class NSString = objc_getClass("NSString");
    
    // Create array of default toolbar item identifiers
    id sidebarToggleIdentifier = ((id (*)(id, SEL, const char*))objc_msgSend)(
        (id)NSString,
        sel_registerName("stringWithUTF8String:"),
        "SidebarToggle"
    );
    
    id identifiers = ((id (*)(id, SEL, id, ...))objc_msgSend)(
        (id)NSArray,
        sel_registerName("arrayWithObjects:"),
        sidebarToggleIdentifier,
        nil
    );
    
    return identifiers;
}

// Allowed toolbar item identifiers
static id toolbar_allowed_item_identifiers(id self, SEL _cmd, id toolbar) {
    (void)self; (void)_cmd; (void)toolbar; // Suppress unused warnings
    
    Class NSArray = objc_getClass("NSArray");
    Class NSString = objc_getClass("NSString");
    
    // Create array of allowed toolbar item identifiers
    id sidebarToggleIdentifier = ((id (*)(id, SEL, const char*))objc_msgSend)(
        (id)NSString,
        sel_registerName("stringWithUTF8String:"),
        "SidebarToggle"
    );
    
    // Add standard toolbar items
    id flexibleSpaceIdentifier = ((id (*)(id, SEL, const char*))objc_msgSend)(
        (id)NSString,
        sel_registerName("stringWithUTF8String:"),
        "NSToolbarFlexibleSpaceItem"
    );
    
    id identifiers = ((id (*)(id, SEL, id, ...))objc_msgSend)(
        (id)NSArray,
        sel_registerName("arrayWithObjects:"),
        sidebarToggleIdentifier,
        flexibleSpaceIdentifier,
        nil
    );
    
    return identifiers;
}

#endif // __APPLE__ 