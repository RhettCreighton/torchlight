/*
 * TorchLight Core Implementation
 * Main HTTP server functionality
 */

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
#include "torchlight.h"

// Global server state
static torchlight_server_t g_server = {0};
static pthread_mutex_t g_server_mutex = PTHREAD_MUTEX_INITIALIZER;

// Default configuration
static const torchlight_config_t DEFAULT_CONFIG = {
    .document_root = "./www",
    .template_directory = "./templates",
    .static_directory = "./static",
    .enable_sessions = true,
    .enable_websockets = true,
    .enable_cors = false,
    .enable_gzip = false,
    .enable_cache = true,
    .max_connections = 100,
    .timeout_seconds = 30,
    .enable_csrf_protection = false,
    .enable_rate_limiting = false,
    .rate_limit_requests_per_minute = 60,
    .error_404_page = "",
    .error_500_page = ""
};

int torchlight_init(const torchlight_config_t* config) {
    pthread_mutex_lock(&g_server_mutex);
    
    if (g_server.initialized) {
        pthread_mutex_unlock(&g_server_mutex);
        return 0;  // Already initialized
    }
    
    printf("🔥 Initializing TorchLight Dynamic HTTP Server v%s\n", TORCHLIGHT_VERSION);
    
    // Initialize server state
    memset(&g_server, 0, sizeof(g_server));
    
    // Use provided config or defaults
    if (config) {
        g_server.config = *config;
    } else {
        g_server.config = DEFAULT_CONFIG;
    }
    
    // Initialize route and session arrays
    g_server.route_count = 0;
    g_server.session_count = 0;
    
    // Initialize statistics
    g_server.requests_served = 0;
    g_server.bytes_sent = 0;
    g_server.bytes_received = 0;
    g_server.active_connections = 0;
    g_server.error_count = 0;
    
    g_server.initialized = true;
    
    printf("✅ TorchLight initialized successfully\n");
    printf("   Document root: %s\n", g_server.config.document_root);
    printf("   Features: Sessions=%s WebSockets=%s CORS=%s\n",
           g_server.config.enable_sessions ? "ON" : "OFF",
           g_server.config.enable_websockets ? "ON" : "OFF",
           g_server.config.enable_cors ? "ON" : "OFF");
    
    pthread_mutex_unlock(&g_server_mutex);
    return 0;
}

int torchlight_start(void) {
    if (!g_server.initialized) {
        printf("❌ TorchLight not initialized - call torchlight_init() first\n");
        return -1;
    }
    
    printf("🚀 TorchLight HTTP server ready for requests\n");
    printf("   Max connections: %d\n", g_server.config.max_connections);
    printf("   Request timeout: %d seconds\n", g_server.config.timeout_seconds);
    
    return 0;
}

int torchlight_handle_request(int socket_fd) {
    if (!g_server.initialized) {
        return -1;
    }
    
    pthread_mutex_lock(&g_server_mutex);
    g_server.active_connections++;
    pthread_mutex_unlock(&g_server_mutex);
    
    printf("🌐 Processing HTTP request on socket %d\n", socket_fd);
    
    // Parse the request
    http_request_t request = {0};
    request.socket_fd = socket_fd;
    request.received_time = time(NULL);
    
    int parse_result = torchlight_parse_request(socket_fd, &request);
    if (parse_result != 0) {
        printf("❌ Failed to parse HTTP request\n");
        
        // Send error response
        http_response_t error_response = {0};
        torchlight_response_error(&error_response, HTTP_STATUS_BAD_REQUEST, "Invalid HTTP request");
        torchlight_send_response(socket_fd, &error_response);
        
        pthread_mutex_lock(&g_server_mutex);
        g_server.active_connections--;
        g_server.error_count++;
        pthread_mutex_unlock(&g_server_mutex);
        
        return -1;
    }
    
    printf("   Method: %d, Path: %s\n", request.method, request.path);
    
    // Update statistics
    pthread_mutex_lock(&g_server_mutex);
    g_server.requests_served++;
    g_server.bytes_received += request.body_length;
    pthread_mutex_unlock(&g_server_mutex);
    
    // Find matching route
    const route_t* route = torchlight_find_route(&request);
    
    http_response_t response = {0};
    
    if (route) {
        printf("   ✅ Route found: %s\n", route->description ? route->description : "No description");
        
        // Call the route handler
        int handler_result = route->handler(&request, &response);
        
        if (handler_result != 0) {
            printf("   ❌ Route handler failed\n");
            torchlight_response_error(&response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "Handler error");
        }
    } else {
        printf("   ❌ No route found for %s %s\n", 
               request.method == HTTP_METHOD_GET ? "GET" : 
               request.method == HTTP_METHOD_POST ? "POST" : "OTHER", 
               request.path);
        
        torchlight_response_error(&response, HTTP_STATUS_NOT_FOUND, "Page not found");
    }
    
    // Add security headers if enabled
    if (g_server.config.enable_csrf_protection || g_server.config.enable_cors) {
        torchlight_add_security_headers(&response);
    }
    
    // Send response
    int send_result = torchlight_send_response(socket_fd, &response);
    
    // Update statistics
    pthread_mutex_lock(&g_server_mutex);
    if (send_result == 0) {
        g_server.bytes_sent += response.body_length;
    } else {
        g_server.error_count++;
    }
    g_server.active_connections--;
    pthread_mutex_unlock(&g_server_mutex);
    
    // Cleanup
    if (request.body) {
        free(request.body);
    }
    if (response.body) {
        free(response.body);
    }
    
    // Call callback if set
    if (g_server.on_response_sent) {
        g_server.on_response_sent(&response);
    }
    
    printf("   ✅ Request completed\n");
    return 0;
}

int torchlight_stop(void) {
    printf("🛑 Stopping TorchLight HTTP server...\n");
    return 0;
}

void torchlight_shutdown(void) {
    pthread_mutex_lock(&g_server_mutex);
    
    if (!g_server.initialized) {
        pthread_mutex_unlock(&g_server_mutex);
        return;
    }
    
    printf("🔄 Shutting down TorchLight HTTP server...\n");
    
    // Cleanup sessions
    torchlight_cleanup_sessions();
    
    // Reset state
    memset(&g_server, 0, sizeof(g_server));
    
    printf("✅ TorchLight shutdown complete\n");
    
    pthread_mutex_unlock(&g_server_mutex);
}

void torchlight_get_stats(torchlight_server_t* stats_out) {
    if (!stats_out) return;
    
    pthread_mutex_lock(&g_server_mutex);
    *stats_out = g_server;
    pthread_mutex_unlock(&g_server_mutex);
}

// Built-in route handlers

// Default index handler
static int default_index_handler(const http_request_t* request, http_response_t* response) {
    (void)request;  // Unused parameter
    
    const char* html_content = 
        "<!DOCTYPE html>\n"
        "<html><head><title>TorchLight Server</title></head>\n"
        "<body>\n"
        "<h1>🔥 TorchLight Dynamic HTTP Server</h1>\n"
        "<p>Your dynamic web application is running!</p>\n"
        "<p>This page is served by the TorchLight HTTP server module.</p>\n"
        "<h2>Server Information</h2>\n"
        "<ul>\n"
        "<li>Version: " TORCHLIGHT_VERSION "</li>\n"
        "<li>Protocol: HTTP/1.1</li>\n"
        "<li>Features: Dynamic routing, Sessions, WebSockets</li>\n"
        "</ul>\n"
        "<h2>Quick Links</h2>\n"
        "<ul>\n"
        "<li><a href=\"/api/status\">API Status</a></li>\n"
        "<li><a href=\"/api/stats\">Server Statistics</a></li>\n"
        "</ul>\n"
        "</body></html>\n";
    
    return torchlight_response_html(response, html_content);
}

// API status endpoint
static int api_status_handler(const http_request_t* request, http_response_t* response) {
    (void)request;  // Unused parameter
    
    const char* json_status = 
        "{\n"
        "  \"status\": \"ok\",\n"
        "  \"server\": \"TorchLight\",\n"
        "  \"version\": \"" TORCHLIGHT_VERSION "\",\n"
        "  \"uptime\": \"running\",\n"
        "  \"features\": {\n"
        "    \"sessions\": true,\n"
        "    \"websockets\": true,\n"
        "    \"json_api\": true,\n"
        "    \"templates\": true\n"
        "  }\n"
        "}\n";
    
    return torchlight_response_json(response, json_status);
}

// API statistics endpoint
static int api_stats_handler(const http_request_t* request, http_response_t* response) {
    (void)request;  // Unused parameter
    
    char stats_json[1024];
    snprintf(stats_json, sizeof(stats_json),
        "{\n"
        "  \"requests_served\": %lu,\n"
        "  \"bytes_sent\": %lu,\n"
        "  \"bytes_received\": %lu,\n"
        "  \"active_connections\": %u,\n"
        "  \"error_count\": %u,\n"
        "  \"route_count\": %d,\n"
        "  \"session_count\": %d\n"
        "}\n",
        g_server.requests_served,
        g_server.bytes_sent, 
        g_server.bytes_received,
        g_server.active_connections,
        g_server.error_count,
        g_server.route_count,
        g_server.session_count);
    
    return torchlight_response_json(response, stats_json);
}

// Register default routes
int torchlight_register_default_routes(void) {
    int result = 0;
    
    result |= torchlight_add_route(HTTP_METHOD_GET, "/", default_index_handler, "Default index page");
    result |= torchlight_add_route(HTTP_METHOD_GET, "/api/status", api_status_handler, "API status endpoint");
    result |= torchlight_add_route(HTTP_METHOD_GET, "/api/stats", api_stats_handler, "Server statistics");
    
    return result;
}