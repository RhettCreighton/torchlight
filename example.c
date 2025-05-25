/*
 * TorchLight Example Application
 * Demonstrates basic TorchLight usage
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "torchlight.h"

static bool running = true;

void signal_handler(int sig) {
    (void)sig;
    printf("\n🛑 Shutting down TorchLight example server...\n");
    running = false;
}

// Example route handlers

int home_handler(const http_request_t* request, http_response_t* response) {
    (void)request;  // Unused
    
    const char* html = 
        "<!DOCTYPE html>\n"
        "<html><head><title>TorchLight Example</title></head>\n"
        "<body>\n"
        "<h1>🔥 Welcome to TorchLight!</h1>\n"
        "<p>This is an example TorchLight application.</p>\n"
        "<h2>Quick Links</h2>\n"
        "<ul>\n"
        "<li><a href=\"/api/hello\">Hello API</a></li>\n"
        "<li><a href=\"/api/time\">Current Time</a></li>\n"
        "<li><a href=\"/users/123\">User Profile</a></li>\n"
        "<li><a href=\"/template\">Template Example</a></li>\n"
        "</ul>\n"
        "</body></html>\n";
    
    return torchlight_response_html(response, html);
}

int hello_api_handler(const http_request_t* request, http_response_t* response) {
    const char* name = torchlight_get_query_param(request, "name");
    
    char json_data[512];
    snprintf(json_data, sizeof(json_data),
        "{\n"
        "  \"message\": \"Hello, %s!\",\n"
        "  \"timestamp\": %ld,\n"
        "  \"method\": \"%s\"\n"
        "}",
        name ? name : "World",
        time(NULL),
        request->method == HTTP_METHOD_GET ? "GET" : "POST");
    
    return torchlight_json_response(response, json_data, "Hello API response");
}

int time_api_handler(const http_request_t* request, http_response_t* response) {
    (void)request;  // Unused
    
    time_t now = time(NULL);
    struct tm* timeinfo = localtime(&now);
    
    char json_data[256];
    snprintf(json_data, sizeof(json_data),
        "{\n"
        "  \"timestamp\": %ld,\n"
        "  \"formatted\": \"%s\",\n"
        "  \"server\": \"TorchLight Example\"\n"
        "}",
        now,
        asctime(timeinfo));
    
    // Remove newline from asctime
    char* newline = strchr(json_data, '\n');
    if (newline && newline > json_data + 50) {  // Make sure it's the asctime newline
        *newline = ' ';
    }
    
    return torchlight_json_response(response, json_data, "Current time");
}

int user_profile_handler(const http_request_t* request, http_response_t* response) {
    const route_t* route = torchlight_find_route(request);
    char user_id[64] = "unknown";
    
    if (route) {
        torchlight_get_path_param(request, route, "id", user_id, sizeof(user_id));
    }
    
    char html[1024];
    snprintf(html, sizeof(html),
        "<!DOCTYPE html>\n"
        "<html><head><title>User Profile</title></head>\n"
        "<body>\n"
        "<h1>👤 User Profile</h1>\n"
        "<p>User ID: <strong>%s</strong></p>\n"
        "<p>This demonstrates path parameter extraction.</p>\n"
        "<a href=\"/\">← Back to Home</a>\n"
        "</body></html>\n",
        user_id);
    
    return torchlight_response_html(response, html);
}

int template_handler(const http_request_t* request, http_response_t* response) {
    (void)request;  // Unused
    
    const char* template_str = 
        "<!DOCTYPE html>\n"
        "<html><head><title>{{title}}</title></head>\n"
        "<body>\n"
        "<h1>{{heading}}</h1>\n"
        "<p>Welcome, {{user_name}}!</p>\n"
        "<p>You have {{message_count}} new messages.</p>\n"
        "<p>Server status: {{status}}</p>\n"
        "</body></html>\n";
    
    const char* variables = 
        "{\n"
        "  \"title\": \"Template Example\",\n"
        "  \"heading\": \"🎨 Template Engine Demo\",\n"
        "  \"user_name\": \"TorchLight User\",\n"
        "  \"message_count\": \"42\",\n"
        "  \"status\": \"Online\"\n"
        "}";
    
    char* rendered_html = NULL;
    size_t html_size = 0;
    
    if (torchlight_substitute_variables(template_str, variables, &rendered_html, &html_size) == 0) {
        response->status = HTTP_STATUS_OK;
        response->content_type = CONTENT_TYPE_TEXT_HTML;
        response->body = rendered_html;
        response->body_length = html_size;
        return 0;
    }
    
    return torchlight_response_error(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "Template rendering failed");
}

int create_simple_server() {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    
    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        return -1;
    }
    
    // Set socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        close(server_fd);
        return -1;
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);
    
    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        return -1;
    }
    
    // Listen for connections
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        return -1;
    }
    
    return server_fd;
}

int main() {
    printf("🔥 TorchLight Example Server\n");
    printf("============================\n\n");
    
    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize TorchLight
    torchlight_config_t config = {0};
    strcpy(config.document_root, "./www");
    config.enable_sessions = true;
    config.enable_websockets = false;  // Simplified for example
    config.max_connections = 50;
    
    if (torchlight_init(&config) != 0) {
        fprintf(stderr, "❌ Failed to initialize TorchLight\n");
        return 1;
    }
    
    // Register routes
    torchlight_add_route(HTTP_METHOD_GET, "/", home_handler, "Home page");
    torchlight_add_route(HTTP_METHOD_GET, "/api/hello", hello_api_handler, "Hello API");
    torchlight_add_route(HTTP_METHOD_GET, "/api/time", time_api_handler, "Time API");
    torchlight_add_route(HTTP_METHOD_GET, "/users/{id}", user_profile_handler, "User profile");
    torchlight_add_route(HTTP_METHOD_GET, "/template", template_handler, "Template example");
    
    // Register default routes (status, stats)
    torchlight_register_default_routes();
    
    // Start TorchLight
    if (torchlight_start() != 0) {
        fprintf(stderr, "❌ Failed to start TorchLight\n");
        torchlight_shutdown();
        return 1;
    }
    
    // Create simple HTTP server
    int server_fd = create_simple_server();
    if (server_fd < 0) {
        fprintf(stderr, "❌ Failed to create server socket\n");
        torchlight_shutdown();
        return 1;
    }
    
    printf("✅ TorchLight example server running on http://localhost:8080\n");
    printf("   Try these URLs:\n");
    printf("   • http://localhost:8080/          - Home page\n");
    printf("   • http://localhost:8080/api/hello - Hello API\n");
    printf("   • http://localhost:8080/api/time  - Time API\n");
    printf("   • http://localhost:8080/users/123 - User profile\n");
    printf("   • http://localhost:8080/template  - Template demo\n");
    printf("   • http://localhost:8080/api/stats - Server stats\n");
    printf("\n   Press Ctrl+C to stop\n\n");
    
    // Main server loop
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    while (running) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        
        struct timeval timeout = {1, 0};  // 1 second timeout
        int activity = select(server_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (activity < 0 && running) {
            perror("select error");
            break;
        }
        
        if (activity > 0 && FD_ISSET(server_fd, &read_fds)) {
            int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd >= 0) {
                // Handle request with TorchLight
                torchlight_handle_request(client_fd);
                close(client_fd);
            }
        }
    }
    
    // Cleanup
    close(server_fd);
    torchlight_shutdown();
    
    printf("✅ TorchLight example server stopped\n");
    return 0;
}