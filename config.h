#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

// Platform detection
#ifdef __APPLE__
    #define PLATFORM_MACOS 1
    #define PLATFORM_NAME "macos"
#elif _WIN32
    #define PLATFORM_WINDOWS 1
    #define PLATFORM_NAME "windows"
#elif __linux__
    #define PLATFORM_LINUX 1
    #define PLATFORM_NAME "linux"
#else
    #define PLATFORM_UNKNOWN 1
    #define PLATFORM_NAME "unknown"
#endif

// Configuration structures
typedef struct {
    char name[256];
    char version[64];
    char bundle_id[256];
} app_config_t;

typedef struct {
    char title[256];
    int width;
    int height;
    int min_width;
    int min_height;
    bool center;
    bool resizable;
    bool minimizable;
    bool maximizable;
    bool closable;
} window_config_t;

typedef struct {
    bool enabled;
} macos_toolbar_config_t;

typedef struct {
    macos_toolbar_config_t toolbar;
} macos_config_t;

typedef struct {
    bool debug_mode;
    bool console_logging;
} development_config_t;

typedef struct {
    app_config_t app;
    window_config_t window;
    macos_config_t macos;
    development_config_t development;
} app_configuration_t;

// Function declarations
app_configuration_t* load_config(const char* config_file);
void free_config(app_configuration_t* config);
void print_config(const app_configuration_t* config);
unsigned long get_window_style_mask(const app_configuration_t* config);

#endif // CONFIG_H 