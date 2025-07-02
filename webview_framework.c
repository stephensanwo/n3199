#include "webview_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_COMMAND_OUTPUT 1024

// Global variable to track dev server process
static pid_t dev_server_pid = 0;

bool run_command(const char* command, char* output, size_t output_size) {
    FILE* pipe = popen(command, "r");
    if (!pipe) {
        printf("Error executing command: %s\n", command);
        return false;
    }

    // Always read the output to prevent EPIPE errors
    char buffer[1024];
    size_t total_read = 0;
    
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        if (output && output_size > 0 && total_read < output_size - 1) {
            size_t len = strlen(buffer);
            size_t copy_len = (total_read + len < output_size - 1) ? len : output_size - 1 - total_read;
            strncpy(output + total_read, buffer, copy_len);
            total_read += copy_len;
        }
        // Always print output for debugging
        printf("%s", buffer);
    }
    
    if (output && output_size > 0) {
        output[total_read] = '\0';
    }

    int status = pclose(pipe);
    return WEXITSTATUS(status) == 0;
}

bool run_build_command(const webview_framework_config_t* config) {
    if (!config) return false;
    
    char build_cmd[512];
    snprintf(build_cmd, sizeof(build_cmd), "cd webview && %s", config->build_command);
    return run_command(build_cmd, NULL, 0);
}

bool check_server_ready(const char* url) {
    char curl_cmd[512];
    snprintf(curl_cmd, sizeof(curl_cmd), "curl -s -o /dev/null -w '%%{http_code}' %s", url);
    
    // Try up to 30 times (15 seconds total)
    for (int i = 0; i < 30; i++) {
        char output[16] = {0};
        if (run_command(curl_cmd, output, sizeof(output))) {
            int status_code = atoi(output);
            if (status_code >= 200 && status_code < 400) {
                printf("Dev server is ready (HTTP %d)\n", status_code);
                return true;
            }
        }
        usleep(500000); // Wait 500ms between attempts
    }
    
    printf("Dev server failed to respond\n");
    return false;
}

bool start_dev_server(const webview_framework_config_t* config) {
    if (!config || !config->dev_url) return false;
    
    printf("Starting development server...\n");
    
    // Fork a child process to run the dev server
    dev_server_pid = fork();
    
    if (dev_server_pid < 0) {
        // Fork failed
        printf("Failed to fork process for dev server\n");
        return false;
    } else if (dev_server_pid == 0) {
        // Child process - execute dev server command
        printf("Starting dev server with command: %s\n", config->dev_command);
        
        // Change to webview directory and execute the command
        chdir("webview");
        char* args[] = {"/bin/sh", "-c", (char*)config->dev_command, NULL};
        execv("/bin/sh", args);
        
        // If execv returns, there was an error
        printf("Failed to execute dev server command\n");
        exit(1);
    }
    
    // Parent process - wait for server to be ready
    printf("Waiting for dev server to be ready...\n");
    if (!check_server_ready(config->dev_url)) {
        printf("Dev server failed to start\n");
        stop_dev_server();
        return false;
    }
    
    printf("Dev server started successfully (PID: %d)\n", dev_server_pid);
    return true;
}

void stop_dev_server(void) {
    if (dev_server_pid > 0) {
        printf("Stopping dev server (PID: %d)...\n", dev_server_pid);
        kill(dev_server_pid, SIGTERM);
        dev_server_pid = 0;
    }
}

const char* get_webview_url(const webview_framework_config_t* config) {
    if (!config) return NULL;
    
    if (config->dev_mode) {
        return config->dev_url;
    } else {
        // TODO: Return URL for built files
        return NULL;
    }
} 