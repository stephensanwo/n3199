#ifndef WEBVIEW_FRAMEWORK_H
#define WEBVIEW_FRAMEWORK_H

#include <stddef.h>
#include "config.h"
#include <stdbool.h>

// Core webview framework functions
bool run_command(const char* command, char* output, size_t output_size);

// Build and development functions
bool run_build_command(const webview_framework_config_t* config);
bool start_dev_server(const webview_framework_config_t* config);
void stop_dev_server(void);

// Utility functions
const char* get_webview_url(const webview_framework_config_t* config);

#endif // WEBVIEW_FRAMEWORK_H 