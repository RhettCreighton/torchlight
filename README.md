# TorchLight - Dynamic HTTP Server Module

🔥 **High-performance web applications over Tor hidden services**

GitHub: https://github.com/RhettCreighton/torchlight

## Overview

TorchLight is a complete web application framework optimized for Tor hidden services, enabling dynamic content, APIs, WebSockets, and real-time applications with minimal overhead.

## Key Features

### 🌐 **Complete HTTP/1.1 Server**
- Zero-configuration setup
- Efficient request parsing and response handling
- Keep-alive connections
- Custom error pages
- Built-in security headers

### 🛣️ **Flexible Routing System**
- REST API routing
- Wildcard patterns (`/api/*`)
- Path parameters (`/users/{id}`)
- Method-specific handlers
- Route descriptions and documentation

### 📊 **JSON API Framework**
- Built-in JSON parsing
- Structured API responses
- Error handling with proper HTTP status codes
- CORS support for cross-origin requests

### 🎨 **Template Engine**
- Simple variable substitution (`{{variable}}`)
- File-based templates
- JSON data binding
- Efficient caching

### ⚡ **Real-time WebSocket Support**
- WebSocket handshake handling
- Text and binary frame support
- Ping/pong keepalive
- Connection management

### 🔒 **Security Features**
- CSRF protection
- Rate limiting
- Secure headers (CSP, HSTS, etc.)
- Session management
- Input validation helpers

## Quick Start

### Basic Server Setup

```c
#include "torchlight.h"

// Define a route handler
int hello_handler(const http_request_t* request, http_response_t* response) {
    return torchlight_response_html(response, "<h1>Hello from TorchLight!</h1>");
}

// API endpoint
int api_users_handler(const http_request_t* request, http_response_t* response) {
    const char* user_data = "{\"users\": [{\"id\": 1, \"name\": \"Alice\"}]}";
    return torchlight_json_response(response, user_data, \"Users retrieved\");
}

int main() {\n    // Initialize TorchLight\n    torchlight_config_t config = {0};\n    strcpy(config.document_root, \"./www\");\n    config.enable_sessions = true;\n    config.enable_websockets = true;\n    \n    torchlight_init(&config);\n    \n    // Register routes\n    torchlight_add_route(HTTP_METHOD_GET, \"/\", hello_handler, \"Home page\");\n    torchlight_add_route(HTTP_METHOD_GET, \"/api/users\", api_users_handler, \"User API\");\n    \n    // Register default routes (status, stats)\n    torchlight_register_default_routes();\n    \n    // Start server\n    torchlight_start();\n    \n    printf(\"TorchLight server ready!\\n\");\n    \n    // Main request loop (integrate with your application)\n    // torchlight_handle_request(socket_fd);\n    \n    return 0;\n}\n```\n\n### Advanced Route Patterns\n\n```c\n// Path parameters\nint user_profile_handler(const http_request_t* request, http_response_t* response) {\n    char user_id[64];\n    const route_t* route = torchlight_find_route(request);\n    \n    if (torchlight_get_path_param(request, route, \"id\", user_id, sizeof(user_id)) == 0) {\n        char html[512];\n        snprintf(html, sizeof(html), \"<h1>User Profile: %s</h1>\", user_id);\n        return torchlight_response_html(response, html);\n    }\n    \n    return torchlight_response_error(response, HTTP_STATUS_BAD_REQUEST, \"Invalid user ID\");\n}\n\n// Register with parameter\ntorchlight_add_route(HTTP_METHOD_GET, \"/users/{id}\", user_profile_handler, \"User profile page\");\n```\n\n### JSON API Example\n\n```c\nint create_user_handler(const http_request_t* request, http_response_t* response) {\n    char* json_data;\n    if (torchlight_parse_json(request, &json_data) != 0) {\n        return torchlight_json_error(response, HTTP_STATUS_BAD_REQUEST, \"Invalid JSON\");\n    }\n    \n    // Process user creation...\n    \n    const char* result = \"{\\\"id\\\": 123, \\\"name\\\": \\\"New User\\\"}\";\n    free(json_data);\n    \n    return torchlight_json_response(response, result, \"User created successfully\");\n}\n\ntorchlight_add_route(HTTP_METHOD_POST, \"/api/users\", create_user_handler, \"Create user\");\n```\n\n### Template Rendering\n\n```c\nint dashboard_handler(const http_request_t* request, http_response_t* response) {\n    // Template data\n    const char* variables = \"{\n        \\\"user_name\\\": \\\"Alice\\\",\n        \\\"user_count\\\": 42,\n        \\\"server_status\\\": \\\"Online\\\"\n    }\";\n    \n    char* rendered_html;\n    size_t html_size;\n    \n    if (torchlight_render_template(\"templates/dashboard.html\", variables, \n                                  &rendered_html, &html_size) == 0) {\n        response->status = HTTP_STATUS_OK;\n        response->content_type = CONTENT_TYPE_TEXT_HTML;\n        response->body = rendered_html;\n        response->body_length = html_size;\n        return 0;\n    }\n    \n    return torchlight_response_error(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, \"Template error\");\n}\n```\n\n### WebSocket Support\n\n```c\nint websocket_handler(const http_request_t* request, http_response_t* response) {\n    if (torchlight_is_websocket_request(request)) {\n        // Perform handshake\n        if (torchlight_websocket_handshake(request->socket_fd, request) == 0) {\n            // WebSocket connection established\n            // Handle messages in a loop...\n            char buffer[1024];\n            size_t received;\n            \n            while (torchlight_websocket_receive(request->socket_fd, buffer, \n                                              sizeof(buffer), &received) == 0) {\n                // Echo message back\n                torchlight_websocket_send(request->socket_fd, buffer, received);\n            }\n        }\n        return 0;\n    }\n    \n    // Not a WebSocket request, serve regular page\n    return torchlight_response_html(response, \"<script>/* WebSocket client code */</script>\");\n}\n\ntorchlight_add_route(HTTP_METHOD_GET, \"/chat\", websocket_handler, \"WebSocket chat\");\n```\n\n## Integration with Tor\n\nTorchLight is designed to work seamlessly with Tor hidden services:\n\n```c\n// In your Tor integration\n#include \"torchlight.h\"\n\nvoid handle_tor_connection(int socket_fd) {\n    // TorchLight handles the HTTP protocol\n    torchlight_handle_request(socket_fd);\n}\n\n// Register with Tor hidden service handler\nhs_register_builtin_service_port(80, BUILTIN_HANDLER_TORCHLIGHT);\nhs_register_builtin_service_handler(BUILTIN_HANDLER_TORCHLIGHT, handle_tor_connection);\n```\n\n## Configuration Options\n\n```c\ntorchlight_config_t config = {\n    .document_root = \"./www\",                    // Static file directory\n    .template_directory = \"./templates\",         // Template files\n    .static_directory = \"./static\",              // Static assets\n    .enable_sessions = true,                     // Session management\n    .enable_websockets = true,                   // WebSocket support\n    .enable_cors = false,                        // CORS headers\n    .enable_gzip = false,                        // Response compression\n    .enable_cache = true,                        // Template caching\n    .max_connections = 100,                      // Connection limit\n    .timeout_seconds = 30,                       // Request timeout\n    .enable_csrf_protection = false,             // CSRF tokens\n    .enable_rate_limiting = false,               // Rate limiting\n    .rate_limit_requests_per_minute = 60,        // Rate limit threshold\n    .error_404_page = \"errors/404.html\",         // Custom error pages\n    .error_500_page = \"errors/500.html\"\n};\n```\n\n## Performance Characteristics\n\n- **Memory footprint**: ~50KB base + routes/sessions\n- **Request handling**: ~1ms per request (simple routes)\n- **Concurrent connections**: 100+ (configurable)\n- **WebSocket overhead**: ~2KB per connection\n- **Template rendering**: ~10ms for complex templates\n\n## Use Cases\n\n### 🎛️ **Control Panels and Dashboards**\n- Server administration interfaces\n- Network monitoring dashboards\n- Configuration management\n- Real-time status displays\n\n### 🤖 **AI and ML Applications**\n- Interactive AI chat interfaces\n- Model training progress dashboards\n- Data visualization tools\n- API endpoints for AI services\n\n### 💬 **Real-time Communication**\n- Chat applications\n- Collaborative editing\n- Live streaming controls\n- Gaming interfaces\n\n### 🔐 **Secure File Sharing**\n- Private file upload/download\n- Document collaboration\n- Secure messaging portals\n- Privacy-focused applications\n\n## Security Best Practices\n\n1. **Always validate input data**\n2. **Use HTTPS headers even over Tor**\n3. **Implement rate limiting for public APIs**\n4. **Sanitize template variables**\n5. **Use sessions for authentication state**\n6. **Enable CSRF protection for forms**\n\n## Building and Testing\n\n```bash\n# Build TorchLight module\ncd src/modules/torchlight\nmake\n\n# Run tests\ngcc -I. test_torchlight.c *.c -lpthread -lssl -lcrypto -o test_torchlight\n./test_torchlight\n```\n\n## API Reference\n\nSee `torchlight.h` for complete API documentation with detailed function descriptions, parameters, and return values.\n\n## Contributing\n\nTorchLight is designed to be:\n- **Lightweight**: Minimal dependencies\n- **Fast**: Optimized for Tor's constraints\n- **Secure**: Built-in security features\n- **Extensible**: Easy to add new features\n\nContributions welcome! Focus areas:\n- Performance optimizations\n- Security enhancements\n- Additional template features\n- WebSocket improvements\n- Documentation and examples\n\n## License\n\nApache License 2.0 - Same as parent project