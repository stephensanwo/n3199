#include "streaming.h"
#include "bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>

#ifdef __APPLE__
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/vm_statistics.h>
#endif

#define MAX_STREAM_FUNCTIONS 32
#define BUFFER_SIZE 4096

// Global streaming state
static streaming_server_t* g_streaming_server = NULL;
static stream_function_entry_t g_stream_functions[MAX_STREAM_FUNCTIONS];
static int g_stream_function_count = 0;
static pthread_mutex_t g_functions_mutex = PTHREAD_MUTEX_INITIALIZER;

// Forward declarations
static void* server_thread_func(void* arg);
static void* connection_thread_func(void* arg);
static void handle_http_request(int client_socket, const char* request);
static stream_function_entry_t* find_stream_function(const char* endpoint);

// Initialize streaming system
bool streaming_init(const streaming_config_t* config, app_window_t* window) {
    if (!config || !window) {
        printf("Streaming init failed: Invalid parameters\n");
        return false;
    }

    printf("Initializing streaming system...\n");

    // Allocate and initialize the streaming server
    g_streaming_server = malloc(sizeof(streaming_server_t));
    if (!g_streaming_server) {
        printf("Streaming init failed: Memory allocation failed\n");
        return false;
    }

    memset(g_streaming_server, 0, sizeof(streaming_server_t));

    // Initialize the server configuration
    g_streaming_server->config = config;
    g_streaming_server->window = window;
    g_streaming_server->server_socket = -1;
    g_streaming_server->running = false;
    g_streaming_server->connections = NULL;
    g_streaming_server->connection_count = 0;
    g_streaming_server->max_connections = config->server.max_connections;

    // Initialize the connections mutex
    if (pthread_mutex_init(&g_streaming_server->connections_mutex, NULL) != 0) {
        printf("Streaming init failed: Mutex initialization failed\n");
        free(g_streaming_server);
        g_streaming_server = NULL;
        return false;
    }

    // Register built-in stream handlers
    streaming_register_builtin_handlers();
    
    // Register custom handlers (user-provided)
    streaming_register_custom_handlers();
    
    // Register streams from configuration (this reads handlers from the registry)
    streaming_register_config_streams(config);

    printf("Streaming system initialized successfully!\n");
    return true;
}

// Cleanup streaming system
void streaming_cleanup(void) {
    if (!g_streaming_server) return;
    
    printf("Cleaning up streaming system...\n");
    
    // Stop server
    streaming_stop_server();
    
    // Cleanup connections
    streaming_cleanup_connections();
    
    // Destroy mutex
    pthread_mutex_destroy(&g_streaming_server->connections_mutex);
    
    // Free server structure
    free(g_streaming_server);
    g_streaming_server = NULL;
    
    // Clear stream functions
    g_stream_function_count = 0;
    
    printf("Streaming system cleaned up\n");
}

// Start streaming server
bool streaming_start_server(void) {
    if (!g_streaming_server) {
        printf("Streaming server not initialized\n");
        return false;
    }

    if (g_streaming_server->running) {
        printf("Streaming server already running\n");
        return true;
    }

    printf("Starting streaming server on %s:%d...\n",
           g_streaming_server->config->server.host, g_streaming_server->config->server.port);

    // Create server thread
    if (pthread_create(&g_streaming_server->server_thread, NULL, server_thread_func, g_streaming_server) != 0) {
        printf("Failed to create streaming server thread\n");
        return false;
    }

    g_streaming_server->running = true;
    printf("Streaming server started successfully!\n");
    return true;
}

// Stop streaming server
void streaming_stop_server(void) {
    if (!g_streaming_server || !g_streaming_server->running) {
        return;
    }
    
    printf("Stopping streaming server...\n");
    
    // Set running flag to false
    g_streaming_server->running = false;
    
    // Close server socket
    if (g_streaming_server->server_socket >= 0) {
        close(g_streaming_server->server_socket);
        g_streaming_server->server_socket = -1;
    }
    
    // Wait for server thread to finish
    pthread_join(g_streaming_server->server_thread, NULL);
    
    // Cleanup all connections
    streaming_cleanup_connections();
    
    printf("Streaming server stopped\n");
}

// Server thread function
static void* server_thread_func(void* arg) {
    streaming_server_t* server = (streaming_server_t*)arg;
    
    printf("Server thread started\n");
    
    // Create socket
    server->server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server->server_socket < 0) {
        printf("Failed to create server socket: %s\n", strerror(errno));
        return NULL;
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(server->server_socket, SOL_SOCKET, SO_REUSEADDR, 
                   &opt, sizeof(opt)) < 0) {
        printf("Failed to set socket options: %s\n", strerror(errno));
        close(server->server_socket);
        return NULL;
    }
    
    // Bind socket
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server->config->server.host);
    server_addr.sin_port = htons(server->config->server.port);
    
    if (bind(server->server_socket, (struct sockaddr*)&server_addr, 
             sizeof(server_addr)) < 0) {
        printf("Failed to bind server socket: %s\n", strerror(errno));
        close(server->server_socket);
        return NULL;
    }
    
    // Listen for connections
    if (listen(server->server_socket, server->config->server.max_connections) < 0) {
        printf("Failed to listen on server socket: %s\n", strerror(errno));
        close(server->server_socket);
        return NULL;
    }

    printf("Server listening on %s:%d\n", server->config->server.host, server->config->server.port);
    
    while (server->running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        // Accept client connection
        int client_socket = accept(server->server_socket, 
                                  (struct sockaddr*)&client_addr, &client_len);
        
        if (client_socket < 0) {
            if (server->running) {
                printf("Failed to accept client connection: %s\n", strerror(errno));
            }
            continue;
        }
        
        printf("Client connected from %s:%d\n", 
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        // Create connection structure
        stream_connection_t* conn = malloc(sizeof(stream_connection_t));
        if (!conn) {
            printf("Failed to allocate connection structure\n");
            close(client_socket);
            continue;
        }
        
        conn->socket = client_socket;
        conn->active = true;
        conn->next = NULL;
        
        // Get client IP and port
        inet_ntop(AF_INET, &client_addr.sin_addr, conn->client_ip, sizeof(conn->client_ip));
        conn->client_port = ntohs(client_addr.sin_port);

        // Add to connections list
        streaming_add_connection(conn);
        
        // Create thread to handle this connection
        if (pthread_create(&conn->thread, NULL, connection_thread_func, conn) != 0) {
            printf("Failed to create connection thread\n");
            streaming_remove_connection(conn);
            close(client_socket);
            free(conn);
        }
    }
    
    printf("Server thread stopping\n");
    return NULL;
}

// Connection thread function
static void* connection_thread_func(void* arg) {
    stream_connection_t* conn = (stream_connection_t*)arg;
    char buffer[BUFFER_SIZE];
    
    // Read HTTP request
    ssize_t bytes_read = recv(conn->socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        handle_http_request(conn->socket, buffer);
    }
    
    // Connection cleanup will be handled by the main cleanup function
    return NULL;
}

// Handle HTTP request
static void handle_http_request(int client_socket, const char* request) {
    printf("HTTP Request: %s\n", request);
    
    // Parse request line
    char method[16], path[256], version[16];
    if (sscanf(request, "%15s %255s %15s", method, path, version) != 3) {
        streaming_send_http_response(client_socket, "400 Bad Request", 
                                   "text/plain", "Bad Request");
        return;
    }
    
    // Only handle GET requests
    if (strcmp(method, "GET") != 0) {
        streaming_send_http_response(client_socket, "405 Method Not Allowed", 
                                   "text/plain", "Method Not Allowed");
        return;
    }
    
    // Find stream function for this endpoint
    stream_function_entry_t* stream_func = find_stream_function(path);
    if (!stream_func) {
        streaming_send_http_response(client_socket, "404 Not Found", 
                                   "text/plain", "Stream not found");
        return;
    }
    
    if (!stream_func->enabled) {
        streaming_send_http_response(client_socket, "503 Service Unavailable", 
                                   "text/plain", "Stream disabled");
        return;
    }
    
    // Send SSE headers
    const char* sse_headers = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n";
    
    send(client_socket, sse_headers, strlen(sse_headers), 0);
    
    // Stream data
    char data_buffer[1024];
    int data_count = 0;
    while (true) {
        // Call stream handler
        stream_func->handler(stream_func->name, data_buffer, sizeof(data_buffer));
        
        printf("Streaming data #%d: %s\n", ++data_count, data_buffer);
        
        // Send SSE event
        streaming_send_sse_event(client_socket, "data", data_buffer);
        
        // Wait for next interval
        usleep(stream_func->interval_ms * 1000);
        
        // Check if connection is still active (simple check)
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(client_socket, SOL_SOCKET, SO_ERROR, &error, &len) != 0 || error != 0) {
            printf("Connection closed, stopping stream\n");
            break;
        }
    }
    
    close(client_socket);
}

// Find stream function by endpoint
static stream_function_entry_t* find_stream_function(const char* endpoint) {
    pthread_mutex_lock(&g_functions_mutex);
    
    for (int i = 0; i < g_stream_function_count; i++) {
        if (strcmp(g_stream_functions[i].endpoint, endpoint) == 0) {
            pthread_mutex_unlock(&g_functions_mutex);
            return &g_stream_functions[i];
        }
    }
    
    pthread_mutex_unlock(&g_functions_mutex);
    return NULL;
}

// Register stream function
void streaming_register_function(const char* name, const char* endpoint, 
                                int interval_ms, stream_handler_t handler, 
                                const char* description) {
    if (!name || !endpoint || !handler || !description) {
        printf("Invalid parameters for stream function registration\n");
        return;
    }
    
    pthread_mutex_lock(&g_functions_mutex);
    
    if (g_stream_function_count >= MAX_STREAM_FUNCTIONS) {
        printf("Maximum stream functions reached\n");
        pthread_mutex_unlock(&g_functions_mutex);
        return;
    }
    
    stream_function_entry_t* entry = &g_stream_functions[g_stream_function_count];
    
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    
    strncpy(entry->endpoint, endpoint, sizeof(entry->endpoint) - 1);
    entry->endpoint[sizeof(entry->endpoint) - 1] = '\0';
    
    entry->interval_ms = interval_ms;
    entry->enabled = true;
    entry->handler = handler;
    
    strncpy(entry->description, description, sizeof(entry->description) - 1);
    entry->description[sizeof(entry->description) - 1] = '\0';
    
    g_stream_function_count++;
    
    printf("Registered stream function: %s -> %s (%d ms)\n", name, endpoint, interval_ms);
    
    pthread_mutex_unlock(&g_functions_mutex);
}

// Send HTTP response
void streaming_send_http_response(int client_socket, const char* status, 
                                 const char* content_type, const char* body) {
    char response[2048];
    snprintf(response, sizeof(response),
            "HTTP/1.1 %s\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            status, content_type, strlen(body), body);
    
    send(client_socket, response, strlen(response), 0);
    close(client_socket);
}

// Send SSE event
void streaming_send_sse_event(int client_socket, const char* event_name, 
                             const char* data) {
    char event[2048];
    snprintf(event, sizeof(event),
            "event: %s\n"
            "data: %s\n"
            "\n",
            event_name, data);
    
    ssize_t sent = send(client_socket, event, strlen(event), 0);
    printf("Sent SSE event: %zd bytes\n", sent);
    if (sent < 0) {
        printf("Failed to send SSE event: %s\n", strerror(errno));
    }
}

// Add connection to list
void streaming_add_connection(stream_connection_t* conn) {
    if (!g_streaming_server || !conn) return;
    
    pthread_mutex_lock(&g_streaming_server->connections_mutex);
    
    conn->next = g_streaming_server->connections;
    g_streaming_server->connections = conn;
    
    pthread_mutex_unlock(&g_streaming_server->connections_mutex);
}

// Remove connection from list
void streaming_remove_connection(stream_connection_t* conn) {
    if (!g_streaming_server || !conn) return;
    
    pthread_mutex_lock(&g_streaming_server->connections_mutex);
    
    stream_connection_t** current = &g_streaming_server->connections;
    while (*current) {
        if (*current == conn) {
            *current = conn->next;
            break;
        }
        current = &(*current)->next;
    }
    
    pthread_mutex_unlock(&g_streaming_server->connections_mutex);
}

// Cleanup all connections
void streaming_cleanup_connections(void) {
    if (!g_streaming_server) return;
    
    pthread_mutex_lock(&g_streaming_server->connections_mutex);
    
    stream_connection_t* current = g_streaming_server->connections;
    while (current) {
        stream_connection_t* next = current->next;
        
        // Close socket
        if (current->socket >= 0) {
            close(current->socket);
        }
        
        // Wait for thread to finish
        if (current->active) {
            pthread_join(current->thread, NULL);
        }
        
        free(current);
        current = next;
    }
    
    g_streaming_server->connections = NULL;
    
    pthread_mutex_unlock(&g_streaming_server->connections_mutex);
} 