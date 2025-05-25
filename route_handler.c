/*
 * TorchLight Route Handler
 * URL routing and handler management
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fnmatch.h>
#include "torchlight.h"

// External reference to global server state
extern torchlight_server_t g_server;

int torchlight_add_route(http_method_t method, const char* path_pattern, 
                        route_handler_func_t handler, const char* description) {
    if (!path_pattern || !handler) return -1;
    
    if (g_server.route_count >= TORCHLIGHT_MAX_ROUTES) {
        printf("❌ Maximum routes (%d) exceeded\n", TORCHLIGHT_MAX_ROUTES);
        return -1;
    }
    
    route_t* route = &g_server.routes[g_server.route_count];
    
    route->method = method;
    strncpy(route->path_pattern, path_pattern, sizeof(route->path_pattern) - 1);
    route->handler = handler;
    route->description = description;
    route->requires_auth = false;
    route->allowed_origins = NULL;
    
    g_server.route_count++;
    
    printf("📍 Route registered: %s %s -> %s\n", 
           method == HTTP_METHOD_GET ? "GET" :
           method == HTTP_METHOD_POST ? "POST" :
           method == HTTP_METHOD_PUT ? "PUT" :
           method == HTTP_METHOD_DELETE ? "DELETE" : "OTHER",
           path_pattern,
           description ? description : "No description");
    
    return 0;
}

int torchlight_add_routes(const route_t* routes, int count) {
    if (!routes || count <= 0) return -1;
    
    int added = 0;
    for (int i = 0; i < count; i++) {
        if (torchlight_add_route(routes[i].method, routes[i].path_pattern, 
                                routes[i].handler, routes[i].description) == 0) {
            added++;
        }
    }
    
    return added;
}

int torchlight_remove_route(http_method_t method, const char* path_pattern) {
    if (!path_pattern) return -1;
    
    for (int i = 0; i < g_server.route_count; i++) {
        if (g_server.routes[i].method == method && 
            strcmp(g_server.routes[i].path_pattern, path_pattern) == 0) {
            
            // Shift remaining routes down
            for (int j = i; j < g_server.route_count - 1; j++) {
                g_server.routes[j] = g_server.routes[j + 1];
            }
            
            g_server.route_count--;
            printf("📍 Route removed: %s %s\n", 
                   method == HTTP_METHOD_GET ? "GET" : 
                   method == HTTP_METHOD_POST ? "POST" : "OTHER", 
                   path_pattern);
            return 0;
        }
    }
    
    return -1;  // Route not found
}

static bool path_matches_pattern(const char* path, const char* pattern) {
    // Simple pattern matching with wildcards
    // Supports: /exact/path, /path/*, /path/{param}
    
    if (strcmp(pattern, path) == 0) {
        return true;  // Exact match
    }
    
    // Check for wildcard patterns
    if (strstr(pattern, "*")) {
        return fnmatch(pattern, path, 0) == 0;
    }
    
    // Check for parameter patterns like /users/{id}
    if (strstr(pattern, "{") && strstr(pattern, "}")) {
        // Simple parameter matching
        char pattern_copy[256];
        strncpy(pattern_copy, pattern, sizeof(pattern_copy) - 1);
        
        // Replace {param} with * for fnmatch
        char* param_start = strchr(pattern_copy, '{');
        while (param_start) {
            char* param_end = strchr(param_start, '}');
            if (param_end) {
                *param_start = '*';
                memmove(param_start + 1, param_end + 1, strlen(param_end + 1) + 1);
            }
            param_start = strchr(param_start + 1, '{');
        }
        
        return fnmatch(pattern_copy, path, 0) == 0;
    }
    
    return false;
}

const route_t* torchlight_find_route(const http_request_t* request) {
    if (!request) return NULL;
    
    // Search for exact method and path match first
    for (int i = 0; i < g_server.route_count; i++) {
        if (g_server.routes[i].method == request->method) {
            if (path_matches_pattern(request->path, g_server.routes[i].path_pattern)) {
                return &g_server.routes[i];
            }
        }
    }
    
    // If no exact match, try wildcard routes with matching method
    for (int i = 0; i < g_server.route_count; i++) {
        if (g_server.routes[i].method == request->method) {
            if (strstr(g_server.routes[i].path_pattern, "*") || 
                strstr(g_server.routes[i].path_pattern, "{")) {
                if (path_matches_pattern(request->path, g_server.routes[i].path_pattern)) {
                    return &g_server.routes[i];
                }
            }
        }
    }
    
    return NULL;  // No route found
}

int torchlight_get_path_param(const http_request_t* request, const route_t* route, 
                             const char* param_name, char* value_out, size_t value_size) {
    if (!request || !route || !param_name || !value_out) return -1;
    
    // Find the parameter in the route pattern
    char param_pattern[64];
    snprintf(param_pattern, sizeof(param_pattern), "{%s}", param_name);
    
    const char* param_pos = strstr(route->path_pattern, param_pattern);
    if (!param_pos) return -1;  // Parameter not found in pattern
    
    // Calculate the position in the actual path
    size_t prefix_length = param_pos - route->path_pattern;
    
    // Find the start of the parameter value in the actual path
    const char* value_start = request->path + prefix_length;
    
    // Find the end of the parameter value
    const char* value_end = strchr(value_start, '/');
    if (!value_end) value_end = value_start + strlen(value_start);
    
    // Extract the parameter value
    size_t param_length = value_end - value_start;
    if (param_length >= value_size) return -1;  // Buffer too small
    
    strncpy(value_out, value_start, param_length);
    value_out[param_length] = '\0';
    
    return 0;
}

// Response helper functions

int torchlight_response_json(http_response_t* response, const char* json_data) {
    if (!response || !json_data) return -1;
    
    response->status = HTTP_STATUS_OK;
    response->content_type = CONTENT_TYPE_APPLICATION_JSON;
    response->body_length = strlen(json_data);
    response->body = malloc(response->body_length + 1);
    
    if (!response->body) return -1;
    
    strcpy(response->body, json_data);
    return 0;
}

int torchlight_response_html(http_response_t* response, const char* html_content) {
    if (!response || !html_content) return -1;
    
    response->status = HTTP_STATUS_OK;
    response->content_type = CONTENT_TYPE_TEXT_HTML;
    response->body_length = strlen(html_content);
    response->body = malloc(response->body_length + 1);
    
    if (!response->body) return -1;
    
    strcpy(response->body, html_content);
    return 0;
}

int torchlight_response_file(http_response_t* response, const char* file_path) {
    if (!response || !file_path) return -1;
    
    FILE* file = fopen(file_path, "rb");
    if (!file) {
        return torchlight_response_error(response, HTTP_STATUS_NOT_FOUND, "File not found");
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(file);
        return torchlight_response_error(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "Cannot read file");
    }
    
    // Allocate buffer and read file
    response->body = malloc(file_size + 1);
    if (!response->body) {
        fclose(file);
        return -1;
    }
    
    size_t bytes_read = fread(response->body, 1, file_size, file);
    fclose(file);
    
    if (bytes_read != (size_t)file_size) {
        free(response->body);
        response->body = NULL;
        return torchlight_response_error(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "File read error");
    }
    
    response->body[file_size] = '\0';
    response->body_length = file_size;
    response->status = HTTP_STATUS_OK;
    
    // Detect content type from file extension
    response->content_type = torchlight_detect_content_type(file_path);
    
    return 0;
}

int torchlight_response_error(http_response_t* response, http_status_t status, const char* message) {
    if (!response) return -1;
    
    response->status = status;
    response->content_type = CONTENT_TYPE_TEXT_HTML;
    
    // Create simple error page
    char* error_html = malloc(1024);
    if (!error_html) return -1;
    
    snprintf(error_html, 1024,
        "<!DOCTYPE html>\n"
        "<html><head><title>Error %d</title></head>\n"
        "<body>\n"
        "<h1>Error %d</h1>\n"
        "<p>%s</p>\n"
        "<hr>\n"
        "<small>TorchLight HTTP Server</small>\n"
        "</body></html>\n",
        status, status, message ? message : "An error occurred");
    
    response->body = error_html;
    response->body_length = strlen(error_html);
    
    return 0;
}

content_type_t torchlight_detect_content_type(const char* file_path) {
    if (!file_path) return CONTENT_TYPE_APPLICATION_OCTET_STREAM;
    
    const char* ext = strrchr(file_path, '.');
    if (!ext) return CONTENT_TYPE_APPLICATION_OCTET_STREAM;
    
    ext++;  // Skip the '.'
    
    if (strcasecmp(ext, "html") == 0 || strcasecmp(ext, "htm") == 0) {
        return CONTENT_TYPE_TEXT_HTML;
    } else if (strcasecmp(ext, "css") == 0) {
        return CONTENT_TYPE_TEXT_CSS;
    } else if (strcasecmp(ext, "js") == 0) {
        return CONTENT_TYPE_TEXT_JAVASCRIPT;
    } else if (strcasecmp(ext, "json") == 0) {
        return CONTENT_TYPE_APPLICATION_JSON;
    } else if (strcasecmp(ext, "xml") == 0) {
        return CONTENT_TYPE_APPLICATION_XML;
    } else if (strcasecmp(ext, "png") == 0) {
        return CONTENT_TYPE_IMAGE_PNG;
    } else if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0) {
        return CONTENT_TYPE_IMAGE_JPEG;
    } else if (strcasecmp(ext, "txt") == 0) {
        return CONTENT_TYPE_TEXT_PLAIN;
    }
    
    return CONTENT_TYPE_APPLICATION_OCTET_STREAM;
}