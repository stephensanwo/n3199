#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Simple JSON parsing helpers
static char* read_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: Could not open config file '%s'\n", filename);
        return NULL;
    }
    
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* content = malloc(length + 1);
    if (!content) {
        fclose(file);
        return NULL;
    }
    
    fread(content, 1, length, file);
    content[length] = '\0';
    fclose(file);
    
    return content;
}

static char* find_json_value(const char* json, const char* key) {
    char search_key[512];
    snprintf(search_key, sizeof(search_key), "\"%s\"", key);
    
    char* key_pos = strstr(json, search_key);
    if (!key_pos) return NULL;
    
    char* colon = strchr(key_pos, ':');
    if (!colon) return NULL;
    
    // Skip whitespace after colon
    colon++;
    while (*colon && isspace(*colon)) colon++;
    
    char* value_start = colon;
    char* value_end = value_start;
    
    if (*value_start == '"') {
        // String value
        value_start++;
        value_end = strchr(value_start, '"');
    } else if (isdigit(*value_start) || *value_start == '-') {
        // Number value
        while (*value_end && (isdigit(*value_end) || *value_end == '.' || *value_end == '-')) {
            value_end++;
        }
    } else if (strncmp(value_start, "true", 4) == 0) {
        value_end = value_start + 4;
    } else if (strncmp(value_start, "false", 5) == 0) {
        value_end = value_start + 5;
    }
    
    if (value_end <= value_start) return NULL;
    
    int length = value_end - value_start;
    char* result = malloc(length + 1);
    strncpy(result, value_start, length);
    result[length] = '\0';
    
    return result;
}

static char* find_nested_json_value(const char* json, const char* section, const char* key) {
    char section_key[512];
    snprintf(section_key, sizeof(section_key), "\"%s\"", section);
    
    char* section_pos = strstr(json, section_key);
    if (!section_pos) return NULL;
    
    char* brace = strchr(section_pos, '{');
    if (!brace) return NULL;
    
    // Find the end of this section
    int brace_count = 1;
    char* section_end = brace + 1;
    while (*section_end && brace_count > 0) {
        if (*section_end == '{') brace_count++;
        if (*section_end == '}') brace_count--;
        section_end++;
    }
    
    // Create a substring for this section
    int section_length = section_end - brace;
    char* section_content = malloc(section_length + 1);
    strncpy(section_content, brace, section_length);
    section_content[section_length] = '\0';
    
    char* result = find_json_value(section_content, key);
    free(section_content);
    
    return result;
}

static void parse_string(const char* json, const char* key, char* dest, size_t dest_size) {
    char* value = find_json_value(json, key);
    if (value) {
        strncpy(dest, value, dest_size - 1);
        dest[dest_size - 1] = '\0';
        free(value);
    }
}

static void parse_nested_string(const char* json, const char* section, const char* key, char* dest, size_t dest_size) {
    char* value = find_nested_json_value(json, section, key);
    if (value) {
        strncpy(dest, value, dest_size - 1);
        dest[dest_size - 1] = '\0';
        free(value);
    }
}

static int parse_int(const char* json, const char* key, int default_value) {
    char* value = find_json_value(json, key);
    if (value) {
        int result = atoi(value);
        free(value);
        return result;
    }
    return default_value;
}

static bool parse_bool(const char* json, const char* key, bool default_value) {
    char* value = find_json_value(json, key);
    if (value) {
        bool result = (strcmp(value, "true") == 0);
        free(value);
        return result;
    }
    return default_value;
}

static bool parse_nested_bool(const char* json, const char* section, const char* key, bool default_value) {
    char* value = find_nested_json_value(json, section, key);
    if (value) {
        bool result = (strcmp(value, "true") == 0);
        free(value);
        return result;
    }
    return default_value;
}

static double parse_double(const char* json, const char* key, double default_value) {
    char* value = find_json_value(json, key);
    if (value) {
        double result = atof(value);
        free(value);
        return result;
    }
    return default_value;
}

static double parse_nested_double(const char* json, const char* section, const char* key, double default_value) {
    char* value = find_nested_json_value(json, section, key);
    if (value) {
        double result = atof(value);
        free(value);
        return result;
    }
    return default_value;
}

app_configuration_t* load_config(const char* config_file) {
    char* json_content = read_file(config_file);
    if (!json_content) {
        printf("Warning: Could not load config file, using defaults\n");
        // Return default configuration
        app_configuration_t* config = malloc(sizeof(app_configuration_t));
        memset(config, 0, sizeof(app_configuration_t));
        strcpy(config->app.name, "Desktop App");
        strcpy(config->window.title, "My Desktop App");
        config->window.width = 800;
        config->window.height = 600;
        config->window.resizable = true;
        config->window.center = true;
        return config;
    }
    
    app_configuration_t* config = malloc(sizeof(app_configuration_t));
    memset(config, 0, sizeof(app_configuration_t));
    
    // Parse app section
    parse_nested_string(json_content, "app", "name", config->app.name, sizeof(config->app.name));
    parse_nested_string(json_content, "app", "version", config->app.version, sizeof(config->app.version));
    parse_nested_string(json_content, "app", "bundle_id", config->app.bundle_id, sizeof(config->app.bundle_id));
    
    // Parse window section
    parse_nested_string(json_content, "window", "title", config->window.title, sizeof(config->window.title));
    config->window.width = parse_int(find_nested_json_value(json_content, "window", "width") ?: "800", "width", 800);
    config->window.height = parse_int(find_nested_json_value(json_content, "window", "height") ?: "600", "height", 600);
    config->window.min_width = parse_int(find_nested_json_value(json_content, "window", "min_width") ?: "400", "min_width", 400);
    config->window.min_height = parse_int(find_nested_json_value(json_content, "window", "min_height") ?: "300", "min_height", 300);
    config->window.center = parse_nested_bool(json_content, "window", "center", true);
    config->window.resizable = parse_nested_bool(json_content, "window", "resizable", true);
    config->window.minimizable = parse_nested_bool(json_content, "window", "minimizable", true);
    config->window.maximizable = parse_nested_bool(json_content, "window", "maximizable", true);
    config->window.closable = parse_nested_bool(json_content, "window", "closable", true);
    
    // Parse macOS-specific configuration
    #ifdef PLATFORM_MACOS
    char* macos_section = strstr(json_content, "\"macos\"");
    if (macos_section) {
        config->macos.toolbar.enabled = parse_nested_bool(json_content, "toolbar", "enabled", false);
    }
    #endif
    
    // Parse development section
    config->development.debug_mode = parse_nested_bool(json_content, "development", "debug_mode", false);
    config->development.console_logging = parse_nested_bool(json_content, "development", "console_logging", true);
    
    free(json_content);
    return config;
}

void free_config(app_configuration_t* config) {
    if (config) {
        free(config);
    }
}

void print_config(const app_configuration_t* config) {
    if (!config) return;
    
    printf("=== Application Configuration ===\n");
    printf("Platform: %s\n", PLATFORM_NAME);
    printf("App Name: %s\n", config->app.name);
    printf("Window Title: %s\n", config->window.title);
    printf("Window Size: %dx%d\n", config->window.width, config->window.height);
    printf("Resizable: %s\n", config->window.resizable ? "Yes" : "No");
    
    #ifdef PLATFORM_MACOS
    printf("macOS Toolbar: %s\n", config->macos.toolbar.enabled ? "Enabled" : "Disabled");
    #endif
    
    printf("Debug Mode: %s\n", config->development.debug_mode ? "On" : "Off");
    printf("=================================\n\n");
}

unsigned long get_window_style_mask(const app_configuration_t* config) {
    unsigned long style_mask = 0;
    
    // Basic window with title bar
    style_mask |= 1; // NSTitledWindowMask
    
    if (config->window.closable) {
        style_mask |= 2; // NSClosableWindowMask
    }
    
    if (config->window.minimizable) {
        style_mask |= 4; // NSMiniaturizableWindowMask
    }
    
    if (config->window.resizable) {
        style_mask |= 8; // NSResizableWindowMask
    }
    
    return style_mask;
} 