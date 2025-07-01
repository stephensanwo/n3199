#include <stdio.h>
#include <stdlib.h>

#ifdef __APPLE__
#include <objc/objc.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <CoreGraphics/CoreGraphics.h>

// Create and show a simple macOS window
void create_window() {
    printf("Creating macOS window...\n");
    
    // Get NSApplication class
    Class NSApp = objc_getClass("NSApplication");
    id app = ((id (*)(id, SEL))objc_msgSend)((id)NSApp, sel_registerName("sharedApplication"));
    
    // Set activation policy to regular app (NSApplicationActivationPolicyRegular = 0)
    ((void (*)(id, SEL, long))objc_msgSend)(app, sel_registerName("setActivationPolicy:"), 0);
    
    // Create window
    Class NSWindow = objc_getClass("NSWindow");
    Class NSString = objc_getClass("NSString");
    
    // Create window frame (x, y, width, height)
    CGRect frame = CGRectMake(100, 100, 800, 600);
    
    // Window style mask: titled(1), closable(2), miniaturizable(4), resizable(8)
    unsigned long styleMask = 1 | 2 | 4 | 8;
    
    // Allocate window
    id window = ((id (*)(id, SEL))objc_msgSend)((id)NSWindow, sel_registerName("alloc"));
    
    // Initialize window
    window = ((id (*)(id, SEL, CGRect, unsigned long, unsigned long, int))objc_msgSend)(
        window, 
        sel_registerName("initWithContentRect:styleMask:backing:defer:"), 
        frame, 
        styleMask, 
        2, // NSBackingStoreBuffered
        0  // defer = NO
    );
    
    // Set window title
    id title = ((id (*)(id, SEL, const char*))objc_msgSend)(
        (id)NSString, 
        sel_registerName("stringWithUTF8String:"), 
        "My First C Desktop App"
    );
    ((void (*)(id, SEL, id))objc_msgSend)(window, sel_registerName("setTitle:"), title);
    
    // Center and show window
    ((void (*)(id, SEL))objc_msgSend)(window, sel_registerName("center"));
    ((void (*)(id, SEL, id))objc_msgSend)(window, sel_registerName("makeKeyAndOrderFront:"), window);
    
    // Activate app
    ((void (*)(id, SEL, int))objc_msgSend)(app, sel_registerName("activateIgnoringOtherApps:"), 1);
    
    printf("Window created and displayed!\n");
    printf("Press Ctrl+C to quit.\n");
    
    // Run the application event loop
    ((void (*)(id, SEL))objc_msgSend)(app, sel_registerName("run"));
}

#else
void create_window() {
    printf("This application is designed for macOS only.\n");
    printf("For other platforms, different window libraries would be needed.\n");
}
#endif

int main() {
    printf("Starting C Desktop Application...\n");
    printf("Platform: macOS\n");
    
    create_window();
    
    return 0;
} 