/*
 * TorchLight - Dynamic HTTP Server Module
 * High-performance web applications over Tor hidden services
 * 
 * GitHub: https://github.com/RhettCreighton/torchlight
 * 
 * TorchLight provides a complete web application framework optimized for
 * Tor hidden services, enabling dynamic content, APIs, WebSockets, and 
 * real-time applications with minimal overhead.
 * 
 * Key Features:
 * - Zero-configuration HTTP/1.1 server
 * - Flexible routing system (REST, GraphQL, WebSocket)
 * - Template engine with caching
 * - JSON API framework
 * - Session management with Tor compatibility
 * - Real-time WebSocket support
 * - Built-in security headers
 * - Minimal memory footprint
 * 
 * Use Cases:
 * - Dynamic web applications
 * - REST APIs and microservices  
 * - Real-time chat and collaboration
 * - Control panels and dashboards
 * - Interactive AI interfaces
 * - Secure file sharing portals
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Configuration Constants
// ============================================================================

#define TORCHLIGHT_VERSION "1.0.0"
#define TORCHLIGHT_MAX_ROUTES 256
#define TORCHLIGHT_MAX_HEADERS 32
#define TORCHLIGHT_MAX_SESSIONS 1024
#define TORCHLIGHT_BUFFER_SIZE 16384
#define TORCHLIGHT_MAX_REQUEST_SIZE (10 * 1024 * 1024)  // 10MB
#define TORCHLIGHT_SESSION_TIMEOUT 3600  // 1 hour

// ============================================================================
// HTTP Types and Enums
// ============================================================================

typedef enum {
    HTTP_METHOD_GET = 0,
    HTTP_METHOD_POST = 1,
    HTTP_METHOD_PUT = 2,
    HTTP_METHOD_DELETE = 3,
    HTTP_METHOD_HEAD = 4,
    HTTP_METHOD_OPTIONS = 5,
    HTTP_METHOD_PATCH = 6,
    HTTP_METHOD_UNKNOWN = 7
} http_method_t;

typedef enum {
    HTTP_STATUS_OK = 200,
    HTTP_STATUS_CREATED = 201,
    HTTP_STATUS_ACCEPTED = 202,
    HTTP_STATUS_NO_CONTENT = 204,
    HTTP_STATUS_MOVED_PERMANENTLY = 301,
    HTTP_STATUS_FOUND = 302,
    HTTP_STATUS_NOT_MODIFIED = 304,
    HTTP_STATUS_BAD_REQUEST = 400,
    HTTP_STATUS_UNAUTHORIZED = 401,
    HTTP_STATUS_FORBIDDEN = 403,
    HTTP_STATUS_NOT_FOUND = 404,
    HTTP_STATUS_METHOD_NOT_ALLOWED = 405,
    HTTP_STATUS_CONFLICT = 409,
    HTTP_STATUS_INTERNAL_SERVER_ERROR = 500,
    HTTP_STATUS_NOT_IMPLEMENTED = 501,
    HTTP_STATUS_SERVICE_UNAVAILABLE = 503
} http_status_t;

typedef enum {
    CONTENT_TYPE_TEXT_HTML = 0,
    CONTENT_TYPE_TEXT_PLAIN = 1,
    CONTENT_TYPE_APPLICATION_JSON = 2,
    CONTENT_TYPE_APPLICATION_XML = 3,
    CONTENT_TYPE_TEXT_CSS = 4,
    CONTENT_TYPE_TEXT_JAVASCRIPT = 5,
    CONTENT_TYPE_IMAGE_PNG = 6,
    CONTENT_TYPE_IMAGE_JPEG = 7,
    CONTENT_TYPE_APPLICATION_OCTET_STREAM = 8
} content_type_t;

// ============================================================================
// Core Data Structures
// ============================================================================

// HTTP Header
typedef struct {
    char name[64];
    char value[512];
} http_header_t;

// HTTP Request
typedef struct {
    http_method_t method;
    char path[512];
    char query_string[1024];
    char http_version[16];
    
    http_header_t headers[TORCHLIGHT_MAX_HEADERS];
    int header_count;
    
    char* body;
    size_t body_length;
    
    // Parsed query parameters
    char query_params[32][2][256];  // [param][name/value][string]
    int query_param_count;
    
    // Session info
    char session_id[64];
    bool has_session;
    
    // Connection info
    int socket_fd;
    time_t received_time;
} http_request_t;

// HTTP Response
typedef struct {
    http_status_t status;
    content_type_t content_type;
    
    http_header_t headers[TORCHLIGHT_MAX_HEADERS];
    int header_count;
    
    char* body;
    size_t body_length;
    
    bool keep_alive;
    bool chunked_encoding;
} http_response_t;

// Route Handler Function Type
typedef int (*route_handler_func_t)(const http_request_t* request, http_response_t* response);

// Route Definition
typedef struct {
    http_method_t method;
    char path_pattern[256];      // Supports wildcards: /api/users/{id}
    route_handler_func_t handler;
    const char* description;
    bool requires_auth;
    char* allowed_origins;       // CORS support
} route_t;

// Session Data
typedef struct {
    char session_id[64];
    char user_id[64];
    time_t created_time;
    time_t last_access_time;
    char data[1024];            // JSON string for session data
    bool authenticated;
} session_t;

// TorchLight Server Configuration
typedef struct {
    char document_root[512];
    char template_directory[512];
    char static_directory[512];
    
    bool enable_sessions;
    bool enable_websockets;
    bool enable_cors;
    bool enable_gzip;
    bool enable_cache;
    
    int max_connections;
    int timeout_seconds;
    
    // Security settings
    bool enable_csrf_protection;
    bool enable_rate_limiting;
    int rate_limit_requests_per_minute;
    
    // Custom error pages
    char error_404_page[256];
    char error_500_page[256];
} torchlight_config_t;

// Main TorchLight Server State
typedef struct {
    bool initialized;
    torchlight_config_t config;
    
    route_t routes[TORCHLIGHT_MAX_ROUTES];
    int route_count;
    
    session_t sessions[TORCHLIGHT_MAX_SESSIONS];
    int session_count;
    
    // Statistics
    uint64_t requests_served;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint32_t active_connections;
    uint32_t error_count;
    
    // Callbacks
    void (*on_request_received)(const http_request_t* request);
    void (*on_response_sent)(const http_response_t* response);
    void (*on_error)(int error_code, const char* message);
} torchlight_server_t;

// ============================================================================
// Core Server API
// ============================================================================

// Initialize TorchLight server
int torchlight_init(const torchlight_config_t* config);

// Start serving HTTP requests (non-blocking)
int torchlight_start(void);

// Process a single HTTP request
int torchlight_handle_request(int socket_fd);

// Stop the server gracefully
int torchlight_stop(void);

// Cleanup resources
void torchlight_shutdown(void);

// Get server statistics
void torchlight_get_stats(torchlight_server_t* stats_out);

// ============================================================================
// Routing API
// ============================================================================

// Register a route handler
int torchlight_add_route(http_method_t method, const char* path_pattern, 
                        route_handler_func_t handler, const char* description);

// Register multiple routes at once
int torchlight_add_routes(const route_t* routes, int count);

// Remove a specific route
int torchlight_remove_route(http_method_t method, const char* path_pattern);

// Find matching route for request
const route_t* torchlight_find_route(const http_request_t* request);

// ============================================================================
// Request/Response Utilities
// ============================================================================

// Parse HTTP request from socket
int torchlight_parse_request(int socket_fd, http_request_t* request);

// Send HTTP response to socket
int torchlight_send_response(int socket_fd, const http_response_t* response);

// Create response with JSON content
int torchlight_response_json(http_response_t* response, const char* json_data);

// Create response with HTML content
int torchlight_response_html(http_response_t* response, const char* html_content);

// Create response with file content
int torchlight_response_file(http_response_t* response, const char* file_path);

// Create error response
int torchlight_response_error(http_response_t* response, http_status_t status, const char* message);

// ============================================================================
// Header and Parameter Utilities
// ============================================================================

// Get header value from request
const char* torchlight_get_header(const http_request_t* request, const char* name);

// Add header to response
int torchlight_add_header(http_response_t* response, const char* name, const char* value);

// Get query parameter from request
const char* torchlight_get_query_param(const http_request_t* request, const char* name);

// Parse URL path parameters (e.g., /users/{id} -> id)
int torchlight_get_path_param(const http_request_t* request, const route_t* route, 
                             const char* param_name, char* value_out, size_t value_size);

// ============================================================================
// Session Management
// ============================================================================

// Create new session
int torchlight_create_session(const char* user_id, char* session_id_out);

// Get session by ID
session_t* torchlight_get_session(const char* session_id);

// Update session data
int torchlight_update_session(const char* session_id, const char* data);

// Destroy session
int torchlight_destroy_session(const char* session_id);

// Cleanup expired sessions
int torchlight_cleanup_sessions(void);

// ============================================================================
// Template Engine
// ============================================================================

// Render template with variables
int torchlight_render_template(const char* template_path, const char* variables_json, 
                              char** output, size_t* output_size);

// Simple variable substitution
int torchlight_substitute_variables(const char* template_str, const char* variables_json,
                                   char** output, size_t* output_size);

// ============================================================================
// JSON API Helpers
// ============================================================================

// Parse JSON from request body
int torchlight_parse_json(const http_request_t* request, char** json_out);

// Create JSON response with status
int torchlight_json_response(http_response_t* response, const char* data, const char* message);

// Create JSON error response
int torchlight_json_error(http_response_t* response, http_status_t status, const char* error_message);

// ============================================================================
// WebSocket Support
// ============================================================================

// Check if request is WebSocket upgrade
bool torchlight_is_websocket_request(const http_request_t* request);

// Perform WebSocket handshake
int torchlight_websocket_handshake(int socket_fd, const http_request_t* request);

// Send WebSocket message
int torchlight_websocket_send(int socket_fd, const char* message, size_t length);

// Receive WebSocket message
int torchlight_websocket_receive(int socket_fd, char* buffer, size_t buffer_size, size_t* received_length);

// ============================================================================
// Security Helpers
// ============================================================================

// Generate CSRF token
int torchlight_generate_csrf_token(char* token_out, size_t token_size);

// Validate CSRF token
bool torchlight_validate_csrf_token(const http_request_t* request, const char* expected_token);

// Check rate limiting
bool torchlight_check_rate_limit(const char* client_id);

// Add security headers to response
int torchlight_add_security_headers(http_response_t* response);

// ============================================================================
// Utility Functions
// ============================================================================

// URL encode/decode
int torchlight_url_encode(const char* input, char* output, size_t output_size);
int torchlight_url_decode(const char* input, char* output, size_t output_size);

// HTML escape
int torchlight_html_escape(const char* input, char* output, size_t output_size);

// MIME type detection
content_type_t torchlight_detect_content_type(const char* file_path);

// File utilities
bool torchlight_file_exists(const char* path);
int torchlight_read_file(const char* path, char** content, size_t* size);

// String utilities
bool torchlight_string_starts_with(const char* str, const char* prefix);
bool torchlight_string_ends_with(const char* str, const char* suffix);
int torchlight_string_replace(const char* input, const char* search, const char* replace, 
                             char* output, size_t output_size);

#ifdef __cplusplus
}
#endif