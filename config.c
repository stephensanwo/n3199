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
    } else if (*value_start == '[') {
        // Array value - need to find the matching closing bracket
        int bracket_count = 1;
        value_end = value_start + 1;
        while (*value_end && bracket_count > 0) {
            if (*value_end == '[') bracket_count++;
            else if (*value_end == ']') bracket_count--;
            value_end++;
        }
        // Include the entire array including brackets
        value_start = value_start;  // Keep the opening bracket
    } else if (*value_start == '{') {
        // Object value - need to find the matching closing brace
        int brace_count = 1;
        value_end = value_start + 1;
        while (*value_end && brace_count > 0) {
            if (*value_end == '{') brace_count++;
            else if (*value_end == '}') brace_count--;
            value_end++;
        }
        // Include the entire object including braces
        value_start = value_start;  // Keep the opening brace
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
    if (!section_pos) {
        return NULL;
    }
    
    char* brace = strchr(section_pos, '{');
    if (!brace) {
        return NULL;
    }
    
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

static void parse_menu_item(const char* json, const char* menu_section, int item_index, menu_item_config_t* item) {
    char item_path[128];
    snprintf(item_path, sizeof(item_path), "%s\": { \"items\": [ %d ]", menu_section, item_index);
    
    // Find the specific item in the array
    char* menu_start = strstr(json, menu_section);
    if (!menu_start) return;
    
    char* items_start = strstr(menu_start, "\"items\":");
    if (!items_start) return;
    
    char* array_start = strchr(items_start, '[');
    if (!array_start) return;
    
    // Navigate to the specific item
    char* current = array_start + 1;
    for (int i = 0; i < item_index && current; i++) {
        current = strchr(current, '{');
        if (current) current++;
        current = strchr(current, '}');
        if (current) current++;
    }
    
    if (!current) return;
    
    char* item_start = strchr(current, '{');
    char* item_end = strchr(item_start, '}');
    if (!item_start || !item_end) return;
    
    // Extract item JSON
    size_t item_len = item_end - item_start + 1;
    char* item_json = malloc(item_len + 1);
    strncpy(item_json, item_start, item_len);
    item_json[item_len] = '\0';
    
    // Parse item fields using find_json_value directly
    char* title = find_json_value(item_json, "title");
    if (title) {
        strncpy(item->title, title, sizeof(item->title) - 1);
        item->title[sizeof(item->title) - 1] = '\0';
        free(title);
    }
    
    char* shortcut = find_json_value(item_json, "shortcut");
    if (shortcut) {
        strncpy(item->shortcut, shortcut, sizeof(item->shortcut) - 1);
        item->shortcut[sizeof(item->shortcut) - 1] = '\0';
        free(shortcut);
    }
    
    char* action = find_json_value(item_json, "action");
    if (action) {
        strncpy(item->action, action, sizeof(item->action) - 1);
        item->action[sizeof(item->action) - 1] = '\0';
        free(action);
    }
    
    char* enabled = find_json_value(item_json, "enabled");
    if (enabled) {
        item->enabled = (strcmp(enabled, "true") == 0);
        free(enabled);
    } else {
        item->enabled = true; // default
    }
    
    char* separator_after = find_json_value(item_json, "separator_after");
    if (separator_after) {
        item->separator_after = (strcmp(separator_after, "true") == 0);
        free(separator_after);
    } else {
        item->separator_after = false; // default
    }
    
    free(item_json);
}

static void parse_menu_config(const char* json, const char* menu_name, menu_config_t* menu) {
    char menu_section[64];
    snprintf(menu_section, sizeof(menu_section), "\"%s\"", menu_name);
    
    menu->enabled = parse_nested_bool(json, menu_name, "enabled", true);
    parse_nested_string(json, menu_name, "title", menu->title, sizeof(menu->title));
    
    // Count items (simplified - assumes well-formed JSON)
    char* menu_start = strstr(json, menu_section);
    if (menu_start) {
        char* items_start = strstr(menu_start, "\"items\":");
        if (items_start) {
            char* array_start = strchr(items_start, '[');
            char* array_end = strchr(array_start, ']');
            if (array_start && array_end) {
                // Count objects in array
                menu->item_count = 0;
                char* current = array_start;
                while (current < array_end && menu->item_count < 16) {
                    current = strchr(current + 1, '{');
                    if (current && current < array_end) {
                        parse_menu_item(json, menu_name, menu->item_count, &menu->items[menu->item_count]);
                        menu->item_count++;
                    } else {
                        break;
                    }
                }
            }
        }
    }
}

static void parse_webview_framework_config(const char* json, webview_framework_config_t* config) {
    // Set defaults first
    strcpy(config->build_command, "pnpm run build");
    strcpy(config->dev_command, "pnpm run dev");
    strcpy(config->dev_url, "http://localhost:5174");
    strcpy(config->build_dir, "dist");
    config->dev_mode = true;

    // Find the webview section first
    char* webview_content = find_nested_json_value(json, "webview", "framework");
    if (!webview_content) {
        return; // Use defaults
    }

    // Now parse the framework fields from the framework content
    char* value;
    
    value = find_json_value(webview_content, "build_command");
    if (value) {
        strncpy(config->build_command, value, sizeof(config->build_command) - 1);
        config->build_command[sizeof(config->build_command) - 1] = '\0';
        free(value);
    }
    
    value = find_json_value(webview_content, "dev_command");
    if (value) {
        strncpy(config->dev_command, value, sizeof(config->dev_command) - 1);
        config->dev_command[sizeof(config->dev_command) - 1] = '\0';
        free(value);
    }
    
    value = find_json_value(webview_content, "dev_url");
    if (value) {
        strncpy(config->dev_url, value, sizeof(config->dev_url) - 1);
        config->dev_url[sizeof(config->dev_url) - 1] = '\0';
        free(value);
    }
    
    value = find_json_value(webview_content, "build_dir");
    if (value) {
        strncpy(config->build_dir, value, sizeof(config->build_dir) - 1);
        config->build_dir[sizeof(config->build_dir) - 1] = '\0';
        free(value);
    }
    
    value = find_json_value(webview_content, "dev_mode");
    if (value) {
        config->dev_mode = (strcmp(value, "true") == 0);
        free(value);
    }
    
    free(webview_content);
}

// NEW: Parse streaming configuration
static void parse_streaming_config(const char* json, streaming_config_t* config) {
    // Set defaults first
    config->enabled = false;
    strcpy(config->server.host, "127.0.0.1");
    config->server.port = 8080;
    config->server.max_connections = 10;
    config->stream_count = 0;

    // Parse streaming enabled flag
    config->enabled = parse_nested_bool(json, "streaming", "enabled", false);
    
    printf("DEBUG: Streaming enabled: %s\n", config->enabled ? "true" : "false");
    
    if (!config->enabled) {
        return; // Skip parsing if streaming is disabled
    }

    // Parse server configuration
    char* server_content = find_nested_json_value(json, "streaming", "server");
    if (server_content) {
        char* value;
        
        value = find_json_value(server_content, "host");
        if (value) {
            strncpy(config->server.host, value, sizeof(config->server.host) - 1);
            config->server.host[sizeof(config->server.host) - 1] = '\0';
            free(value);
        }
        
        value = find_json_value(server_content, "port");
        if (value) {
            config->server.port = atoi(value);
            free(value);
        }
        
        value = find_json_value(server_content, "max_connections");
        if (value) {
            config->server.max_connections = atoi(value);
            free(value);
        }
        
        free(server_content);
    }

    // Parse streams array
    printf("DEBUG: Looking for streams array...\n");
    char* streams_content = find_nested_json_value(json, "streaming", "streams");
    if (streams_content) {
        printf("DEBUG: Found streams content: %.200s...\n", streams_content);
        
        // Find the array start
        char* array_start = strchr(streams_content, '[');
        if (array_start) {
            printf("DEBUG: Found array start\n");
            char* current = array_start + 1;
            int stream_index = 0;
            
            // Parse each stream object
            while (current && *current && stream_index < 16) {
                // Find next object start
                char* obj_start = strchr(current, '{');
                if (!obj_start) break;
                
                printf("DEBUG: Parsing stream object %d\n", stream_index);
                
                // Find matching closing brace
                int brace_count = 1;
                char* obj_end = obj_start + 1;
                while (*obj_end && brace_count > 0) {
                    if (*obj_end == '{') brace_count++;
                    else if (*obj_end == '}') brace_count--;
                    obj_end++;
                }
                
                if (brace_count == 0) {
                    // Extract the object
                    size_t obj_len = obj_end - obj_start;
                    char* obj_content = malloc(obj_len + 1);
                    strncpy(obj_content, obj_start, obj_len);
                    obj_content[obj_len] = '\0';
                    
                    printf("DEBUG: Stream object content: %s\n", obj_content);
                    
                    // Parse stream object
                    stream_function_config_t* stream = &config->streams[stream_index];
                    
                    char* value;
                    value = find_json_value(obj_content, "name");
                    if (value) {
                        strncpy(stream->name, value, sizeof(stream->name) - 1);
                        stream->name[sizeof(stream->name) - 1] = '\0';
                        printf("DEBUG: Stream name: %s\n", stream->name);
                        free(value);
                    }
                    
                    value = find_json_value(obj_content, "endpoint");
                    if (value) {
                        strncpy(stream->endpoint, value, sizeof(stream->endpoint) - 1);
                        stream->endpoint[sizeof(stream->endpoint) - 1] = '\0';
                        printf("DEBUG: Stream endpoint: %s\n", stream->endpoint);
                        free(value);
                    }
                    
                    value = find_json_value(obj_content, "handler");
                    if (value) {
                        strncpy(stream->handler, value, sizeof(stream->handler) - 1);
                        stream->handler[sizeof(stream->handler) - 1] = '\0';
                        printf("DEBUG: Stream handler: %s\n", stream->handler);
                        free(value);
                    }
                    
                    value = find_json_value(obj_content, "interval_ms");
                    if (value) {
                        stream->interval_ms = atoi(value);
                        printf("DEBUG: Stream interval: %d\n", stream->interval_ms);
                        free(value);
                    }
                    
                    value = find_json_value(obj_content, "enabled");
                    if (value) {
                        stream->enabled = (strcmp(value, "true") == 0);
                        printf("DEBUG: Stream enabled: %s\n", stream->enabled ? "true" : "false");
                        free(value);
                    }
                    
                    value = find_json_value(obj_content, "description");
                    if (value) {
                        strncpy(stream->description, value, sizeof(stream->description) - 1);
                        stream->description[sizeof(stream->description) - 1] = '\0';
                        free(value);
                    }
                    
                    free(obj_content);
                    stream_index++;
                }
                
                current = obj_end;
            }
            
            config->stream_count = stream_index;
            printf("DEBUG: Final stream count: %d\n", config->stream_count);
        } else {
            printf("DEBUG: No array start found in streams content\n");
        }
        
        free(streams_content);
    } else {
        printf("DEBUG: No streams content found\n");
    }
}

static void parse_toolbar_button(const char* json, const char* group_section, int button_index, toolbar_button_config_t* button) {
    // Initialize button with empty values
    memset(button, 0, sizeof(toolbar_button_config_t));
    
    // Find the group section (left, middle, or right)
    char* group_start = strstr(json, group_section);
    if (!group_start) return;
    
    // Find the buttons array within the group
    char* buttons_start = strstr(group_start, "\"buttons\":");
    if (!buttons_start) return;
    
    char* array_start = strchr(buttons_start, '[');
    if (!array_start) return;
    
    // Navigate to the specific button
    char* current = array_start + 1;
    for (int i = 0; i < button_index && current; i++) {
        current = strchr(current, '{');
        if (current) current++;
        
        // Find the end of this button object
        int brace_count = 1;
        char* button_end = current;
        while (*button_end && brace_count > 0) {
            if (*button_end == '{') brace_count++;
            if (*button_end == '}') brace_count--;
            button_end++;
        }
        current = button_end;
    }
    
    if (!current) return;
    
    // Find the start of this button object
    current = strchr(current - 1, '{');
    if (!current) return;
    
    // Find the end of this button object
    int brace_count = 1;
    char* button_end = current + 1;
    while (*button_end && brace_count > 0) {
        if (*button_end == '{') brace_count++;
        if (*button_end == '}') brace_count--;
        button_end++;
    }
    
    // Create a substring for this button
    int button_length = button_end - current;
    char* button_content = malloc(button_length + 1);
    strncpy(button_content, current, button_length);
    button_content[button_length] = '\0';
    
    // Parse button properties
    char* name_val = find_json_value(button_content, "name");
    if (name_val) {
        strncpy(button->name, name_val, sizeof(button->name) - 1);
        button->name[sizeof(button->name) - 1] = '\0';
        free(name_val);
    }
    
    char* icon_val = find_json_value(button_content, "icon");
    if (icon_val) {
        strncpy(button->icon, icon_val, sizeof(button->icon) - 1);
        button->icon[sizeof(button->icon) - 1] = '\0';
        free(icon_val);
    }
    
    char* action_val = find_json_value(button_content, "action");
    if (action_val) {
        strncpy(button->action, action_val, sizeof(button->action) - 1);
        button->action[sizeof(button->action) - 1] = '\0';
        free(action_val);
    }
    
    char* tooltip_val = find_json_value(button_content, "tooltip");
    if (tooltip_val) {
        strncpy(button->tooltip, tooltip_val, sizeof(button->tooltip) - 1);
        button->tooltip[sizeof(button->tooltip) - 1] = '\0';
        free(tooltip_val);
    }
    
    char* enabled_val = find_json_value(button_content, "enabled");
    if (enabled_val) {
        button->enabled = (strcmp(enabled_val, "true") == 0);
        free(enabled_val);
    } else {
        button->enabled = true;
    }
    
    free(button_content);
}

// Parse a toolbar group (left, middle, or right)
static void parse_toolbar_group(const char* json, const char* group_name, toolbar_group_config_t* group) {
    group->button_count = 0;
    
    // Parse buttons in this group
    for (int i = 0; i < 8; i++) {
        parse_toolbar_button(json, group_name, i, &group->buttons[i]);
        if (strlen(group->buttons[i].name) > 0) {
            group->button_count++;
        } else {
            break;
        }
    }
}

// Parse complete toolbar configuration
static void parse_toolbar_config(const char* json, macos_toolbar_config_t* toolbar) {
    // Find the macOS section first
    char* macos_section = strstr(json, "\"macos\"");
    if (!macos_section) {
        toolbar->enabled = false;
        return;
    }
    
    // Find the toolbar section within macOS
    char* toolbar_section = strstr(macos_section, "\"toolbar\"");
    if (!toolbar_section) {
        toolbar->enabled = false;
        return;
    }
    
    // Parse basic toolbar properties
    char* enabled_val = find_json_value(toolbar_section, "enabled");
    if (enabled_val) {
        toolbar->enabled = (strcmp(enabled_val, "true") == 0);
        free(enabled_val);
    } else {
        toolbar->enabled = false;
    }
    
    // Parse toolbar groups
    parse_toolbar_group(toolbar_section, "\"left\"", &toolbar->left);
    parse_toolbar_group(toolbar_section, "\"middle\"", &toolbar->middle);
    parse_toolbar_group(toolbar_section, "\"right\"", &toolbar->right);
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
    
    // Use a simpler approach to parse nested macOS configurations
    char* macos_section = strstr(json_content, "\"macos\"");
    if (macos_section) {
        // Parse toolbar configuration
        parse_toolbar_config(json_content, &config->macos.toolbar);
        
        // Parse show_title_bar within macOS section
        char* title_bar_val = find_json_value(macos_section, "show_title_bar");
        if (title_bar_val) {
            config->macos.show_title_bar = (strcmp(title_bar_val, "true") == 0);
            free(title_bar_val);
        } else {
            config->macos.show_title_bar = false;  // Default to false for modern appearance
        }
    } else {
        config->macos.toolbar.enabled = false;
        config->macos.show_title_bar = false;  // Default to false for modern appearance
    }
    
    // Parse development section
    config->development.debug_mode = parse_nested_bool(json_content, "development", "debug_mode", false);
    config->development.console_logging = parse_nested_bool(json_content, "development", "console_logging", true);
    
    // Parse menubar configuration
    config->menubar.enabled = parse_nested_bool(json_content, "menubar", "enabled", true);
    config->menubar.show_about_item = parse_nested_bool(json_content, "menubar", "show_about_item", true);
    config->menubar.show_preferences_item = parse_nested_bool(json_content, "menubar", "show_preferences_item", true);
    config->menubar.show_services_menu = parse_nested_bool(json_content, "menubar", "show_services_menu", false);
    
    // Parse individual menus
    parse_menu_config(json_content, "file_menu", &config->menubar.file_menu);
    parse_menu_config(json_content, "edit_menu", &config->menubar.edit_menu);
    parse_menu_config(json_content, "view_menu", &config->menubar.view_menu);
    parse_menu_config(json_content, "window_menu", &config->menubar.window_menu);
    parse_menu_config(json_content, "help_menu", &config->menubar.help_menu);
    
    // Parse WebView configuration
    config->webview.enabled = parse_nested_bool(json_content, "webview", "enabled", false);
    config->webview.developer_extras = parse_nested_bool(json_content, "webview", "developer_extras", false);
    config->webview.javascript_enabled = parse_nested_bool(json_content, "webview", "javascript_enabled", true);
    
    // Parse webview framework config
    parse_webview_framework_config(json_content, &config->webview.framework);
    
    // NEW: Parse streaming configuration
    parse_streaming_config(json_content, &config->streaming);
    
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
    printf("macOS Title Bar: %s\n", config->macos.show_title_bar ? "Visible" : "Hidden");
    #endif
    
    printf("Debug Mode: %s\n", config->development.debug_mode ? "On" : "Off");
    
    // Debug webview framework configuration
    if (config->webview.enabled) {
        printf("\n=== WebView Framework Configuration ===\n");
        printf("Build Command: '%s'\n", config->webview.framework.build_command);
        printf("Dev Command: '%s'\n", config->webview.framework.dev_command);
        printf("Dev URL: %s\n", config->webview.framework.dev_url);
        printf("Build Directory: %s\n", config->webview.framework.build_dir);
        printf("Dev Mode: %s\n", config->webview.framework.dev_mode ? "Yes" : "No");
        printf("==========================================\n");
    }
    
    // NEW: Debug streaming configuration
    if (config->streaming.enabled) {
        printf("\n=== Streaming Configuration ===\n");
        printf("Streaming Server: %s:%d\n", config->streaming.server.host, config->streaming.server.port);
        printf("Max Connections: %d\n", config->streaming.server.max_connections);
        printf("Configured Streams: %d\n", config->streaming.stream_count);
        
        for (int i = 0; i < config->streaming.stream_count; i++) {
            const stream_function_config_t* stream = &config->streaming.streams[i];
            printf("  Stream %d: %s\n", i + 1, stream->name);
            printf("    Endpoint: %s\n", stream->endpoint);
            printf("    Handler: %s\n", stream->handler);
            printf("    Interval: %d ms\n", stream->interval_ms);
            printf("    Enabled: %s\n", stream->enabled ? "Yes" : "No");
            printf("    Description: %s\n", stream->description);
        }
        printf("===============================\n");
    }
    
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