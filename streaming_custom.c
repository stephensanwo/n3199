#include "streaming.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __APPLE__
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/vm_statistics.h>
#endif

// Custom stream handler registry
typedef struct custom_stream_handler {
    char name[64];
    stream_handler_t handler;
    struct custom_stream_handler* next;
} custom_stream_handler_t;

static custom_stream_handler_t* g_custom_handlers = NULL;

// Forward declarations
static stream_handler_t find_custom_handler(const char* name);
static void default_custom_handler(const char* stream_name, char* output_buffer, size_t buffer_size);

// Custom handler implementations (user-provided via config)
void stream_system_memory(const char* stream_name, char* output_buffer, size_t buffer_size);
void stream_network_tcpdump(const char* stream_name, char* output_buffer, size_t buffer_size);

// Register stream functions from configuration
void streaming_register_config_streams(const streaming_config_t* config) {
    if (!config) {
        printf("No streaming config provided for stream registration\n");
        return;
    }
    
    printf("Registering %d streams from config...\n", config->stream_count);
    
    for (int i = 0; i < config->stream_count; i++) {
        const stream_function_config_t* stream_config = &config->streams[i];
        
        if (!stream_config->enabled) {
            printf("Stream '%s' is disabled in config\n", stream_config->name);
            continue;
        }
        
        // Map handler name from config to actual handler function
        stream_handler_t handler = find_custom_handler(stream_config->handler);
        if (!handler) {
            printf("No handler found for '%s', using default handler\n", stream_config->handler);
            handler = default_custom_handler;
        }
        
        streaming_register_function(
            stream_config->name,
            stream_config->endpoint,
            stream_config->interval_ms,
            handler,
            stream_config->description
        );
        
        printf("Registered: %s -> %s (handler: %s)\n", 
               stream_config->endpoint, stream_config->name, stream_config->handler);
    }
    
    printf("Config streams registered successfully\n");
}

// Register user-provided custom handlers
void streaming_register_custom_handlers(void) {
    printf("Registering user-provided custom handlers...\n");
    
    // Register the handlers that users provide via config
    streaming_register_custom_handler("stream_system_memory", stream_system_memory);
    streaming_register_custom_handler("stream_network_tcpdump", stream_network_tcpdump);
    
    printf("Custom handlers registered successfully\n");
}

// Register a custom stream handler (for user-defined functions)
bool streaming_register_custom_handler(const char* name, stream_handler_t handler) {
    if (!name || !handler) {
        printf("Invalid parameters for custom handler registration\n");
        return false;
    }
    
    // Create new handler entry
    custom_stream_handler_t* new_handler = malloc(sizeof(custom_stream_handler_t));
    if (!new_handler) {
        printf("Failed to allocate memory for custom handler\n");
        return false;
    }
    
    strncpy(new_handler->name, name, sizeof(new_handler->name) - 1);
    new_handler->name[sizeof(new_handler->name) - 1] = '\0';
    new_handler->handler = handler;
    new_handler->next = g_custom_handlers;
    
    g_custom_handlers = new_handler;
    
    printf("Registered custom handler for stream: %s\n", name);
    return true;
}

// Find a custom handler by name
static stream_handler_t find_custom_handler(const char* name) {
    custom_stream_handler_t* current = g_custom_handlers;
    while (current) {
        if (strcmp(current->name, name) == 0) {
            return current->handler;
        }
        current = current->next;
    }
    return NULL;
}

// Default handler for custom streams that don't have specific implementations
static void default_custom_handler(const char* stream_name, char* output_buffer, size_t buffer_size) {
    time_t now = time(NULL);
    
    // Generate a default response indicating this is a placeholder
    snprintf(output_buffer, buffer_size,
            "{\"timestamp\":%ld,\"stream\":\"%s\",\"status\":\"placeholder\",\"message\":\"This is a default handler. Implement a custom handler for this stream.\"}",
            now, stream_name);
}

// Cleanup custom handlers
void streaming_cleanup_custom_handlers(void) {
    custom_stream_handler_t* current = g_custom_handlers;
    while (current) {
        custom_stream_handler_t* next = current->next;
        free(current);
        current = next;
    }
    g_custom_handlers = NULL;
}

// System memory stream handler (user-provided)
void stream_system_memory(const char* stream_name, char* output_buffer, size_t buffer_size) {
    (void)stream_name; // Unused parameter
    
#ifdef __APPLE__
    // Get total physical memory using syscall
    uint64_t total_memory_bytes;
    size_t size = sizeof(total_memory_bytes);
    if (sysctlbyname("hw.memsize", &total_memory_bytes, &size, NULL, 0) != 0) {
        time_t now = time(NULL);
        snprintf(output_buffer, buffer_size,
                "{\"timestamp\":%ld,\"error\":\"Failed to get total memory size\"}",
                now);
        return;
    }
    
    // Get memory pressure (used memory)
    uint64_t memory_pressure = 0;
    size = sizeof(memory_pressure);
    sysctlbyname("vm.memory_pressure", &memory_pressure, &size, NULL, 0);
    
    // Get page size
    size_t page_size;
    size = sizeof(page_size);
    sysctlbyname("hw.pagesize", &page_size, &size, NULL, 0);
    
    // Get VM statistics for detailed breakdown
    vm_statistics64_data_t vm_stat;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, 
                         (host_info64_t)&vm_stat, &count) == KERN_SUCCESS) {
        
        // Calculate memory values in MB using actual physical memory
        uint64_t total_memory = total_memory_bytes / (1024 * 1024);
        uint64_t free_memory = vm_stat.free_count * page_size / (1024 * 1024);
        uint64_t active_memory = vm_stat.active_count * page_size / (1024 * 1024);
        uint64_t inactive_memory = vm_stat.inactive_count * page_size / (1024 * 1024);
        uint64_t wired_memory = vm_stat.wire_count * page_size / (1024 * 1024);
        uint64_t compressed_memory = vm_stat.compressor_page_count * page_size / (1024 * 1024);
        
        // Calculate used memory properly: total - free
        uint64_t used_memory = total_memory - free_memory;
        
        // Get current timestamp
        time_t now = time(NULL);
        
        // Format JSON response with all memory details
        snprintf(output_buffer, buffer_size,
                "{\"timestamp\":%ld,\"total_mb\":%llu,\"used_mb\":%llu,\"free_mb\":%llu,\"active_mb\":%llu,\"inactive_mb\":%llu,\"wired_mb\":%llu,\"compressed_mb\":%llu}",
                now, total_memory, used_memory, free_memory, active_memory, inactive_memory, wired_memory, compressed_memory);
    } else {
        // Fallback data
        time_t now = time(NULL);
        snprintf(output_buffer, buffer_size,
                "{\"timestamp\":%ld,\"error\":\"Failed to get memory statistics\"}",
                now);
    }
#else
    // Fallback for other platforms
    time_t now = time(NULL);
    snprintf(output_buffer, buffer_size,
            "{\"timestamp\":%ld,\"error\":\"Memory monitoring not implemented for this platform\"}",
            now);
#endif
}

// Network TCP dump stream handler (user-provided)
void stream_network_tcpdump(const char* stream_name, char* output_buffer, size_t buffer_size) {
    (void)stream_name; // Unused parameter
    
    static int packet_count = 0;
    static char recent_packets[5][256]; // Store last 5 packets
    static int packet_index = 0;
    
    // Simulate TCP dump data (in real implementation, this would capture actual network traffic)
    time_t now = time(NULL);
    packet_count++;
    
    // Generate mock TCP dump entry
    const char* protocols[] = {"TCP", "UDP", "ICMP", "HTTP", "HTTPS"};
    const char* sources[] = {"192.168.1.100", "10.0.0.15", "172.16.0.5", "127.0.0.1", "8.8.8.8"};
    const char* destinations[] = {"93.184.216.34", "142.250.191.14", "151.101.65.140", "192.168.1.1", "10.0.0.1"};
    
    int protocol_idx = packet_count % 5;
    int src_idx = packet_count % 5;
    int dst_idx = (packet_count + 1) % 5;
    int src_port = 1024 + (packet_count % 40000);
    int dst_port = (protocol_idx == 3) ? 80 : ((protocol_idx == 4) ? 443 : (packet_count % 65535));
    int packet_size = 64 + (packet_count % 1400);
    
    // Create packet entry
    snprintf(recent_packets[packet_index], sizeof(recent_packets[packet_index]),
            "{\"protocol\":\"%s\",\"src\":\"%s:%d\",\"dst\":\"%s:%d\",\"size\":%d,\"time\":%ld}",
            protocols[protocol_idx], sources[src_idx], src_port, 
            destinations[dst_idx], dst_port, packet_size, now);
    
    packet_index = (packet_index + 1) % 5;
    
    // Build JSON response with recent packets
    snprintf(output_buffer, buffer_size,
            "{\"timestamp\":%ld,\"packet_count\":%d,\"recent_packets\":[",
            now, packet_count);
    
    // Add recent packets (fix JSON formatting - no leading comma)
    bool first_packet = true;
    for (int i = 0; i < 5; i++) {
        int idx = (packet_index + i) % 5;
        if (strlen(recent_packets[idx]) > 0) {
            if (!first_packet) {
                strcat(output_buffer, ",");
            }
            strcat(output_buffer, recent_packets[idx]);
            first_packet = false;
        }
    }
    
    strcat(output_buffer, "]}");
} 