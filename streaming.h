#ifndef STREAMING_H
#define STREAMING_H

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/socket.h>
#include "config.h"
#include "platform.h"

// Stream connection structure
typedef struct stream_connection {
    int socket;
    bool active;
    pthread_t thread;
    char client_ip[16];
    uint16_t client_port;
    struct stream_connection* next;
} stream_connection_t;

// Streaming server structure
typedef struct streaming_server {
    int server_socket;
    bool running;
    pthread_t server_thread;
    stream_connection_t* connections;
    pthread_mutex_t connections_mutex;
    int connection_count;
    int max_connections;
    const streaming_config_t* config;
    app_window_t* window;
} streaming_server_t;

// Stream handler function type
typedef void (*stream_handler_t)(const char* stream_name, char* output_buffer, size_t buffer_size);

// Stream function registry entry
typedef struct {
    char name[64];
    char endpoint[128];
    int interval_ms;
    bool enabled;
    stream_handler_t handler;
    char description[256];
} stream_function_entry_t;

// Streaming server management
bool streaming_init(const streaming_config_t* config, app_window_t* window);
void streaming_cleanup(void);
bool streaming_start_server(void);
void streaming_stop_server(void);

// Stream function registration
void streaming_register_function(const char* name, const char* endpoint, 
                                int interval_ms, stream_handler_t handler, 
                                const char* description);

// Built-in and custom handler registration (defined in separate files)
void streaming_register_builtin_handlers(void);
void streaming_register_config_streams(const streaming_config_t* config);
void streaming_register_custom_handlers(void);

// Custom handler registration (for user-defined functions)
bool streaming_register_custom_handler(const char* name, stream_handler_t handler);
void streaming_cleanup_custom_handlers(void);

// HTTP response utilities
void streaming_send_http_response(int client_socket, const char* status, 
                                 const char* content_type, const char* body);
void streaming_send_sse_event(int client_socket, const char* event_name, 
                             const char* data);

// Connection management
void streaming_add_connection(stream_connection_t* conn);
void streaming_remove_connection(stream_connection_t* conn);
void streaming_cleanup_connections(void);

#endif // STREAMING_H 