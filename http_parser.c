/*
 * TorchLight HTTP Parser
 * Efficient HTTP/1.1 request parsing
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "torchlight.h"

// HTTP method strings
static const char* HTTP_METHOD_STRINGS[] = {
    "GET", "POST", "PUT", "DELETE", "HEAD", "OPTIONS", "PATCH"
};

// Content type strings
static const char* CONTENT_TYPE_STRINGS[] = {
    "text/html; charset=utf-8",
    "text/plain; charset=utf-8",
    "application/json; charset=utf-8",
    "application/xml; charset=utf-8",
    "text/css; charset=utf-8",
    "text/javascript; charset=utf-8",
    "image/png",
    "image/jpeg",
    "application/octet-stream"
};

static http_method_t parse_http_method(const char* method_str) {
    for (int i = 0; i < 7; i++) {
        if (strcmp(method_str, HTTP_METHOD_STRINGS[i]) == 0) {
            return (http_method_t)i;
        }
    }
    return HTTP_METHOD_UNKNOWN;
}

static int parse_query_string(const char* query_string, http_request_t* request) {
    if (!query_string || !request) return -1;
    
    char* query_copy = strdup(query_string);
    if (!query_copy) return -1;
    
    request->query_param_count = 0;
    char* pair = strtok(query_copy, "&");
    
    while (pair && request->query_param_count < 32) {
        char* equals = strchr(pair, '=');
        if (equals) {
            *equals = '\0';
            
            // URL decode name and value
            strncpy(request->query_params[request->query_param_count][0], pair, 255);
            strncpy(request->query_params[request->query_param_count][1], equals + 1, 255);
            
            request->query_param_count++;
        }
        pair = strtok(NULL, "&");
    }
    
    free(query_copy);
    return 0;
}

int torchlight_parse_request(int socket_fd, http_request_t* request) {
    if (!request) return -1;
    
    char buffer[TORCHLIGHT_BUFFER_SIZE] = {0};
    ssize_t bytes_read = recv(socket_fd, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_read <= 0) {
        printf("❌ Failed to read request data\n");
        return -1;
    }
    
    buffer[bytes_read] = '\0';
    
    // Parse request line
    char* line_end = strstr(buffer, "\r\n");
    if (!line_end) {
        printf("❌ Invalid HTTP request - no CRLF found\n");
        return -1;
    }
    
    *line_end = '\0';
    char method_str[16] = {0};
    char path_and_query[512] = {0};
    
    int parsed = sscanf(buffer, "%15s %511s %15s", method_str, path_and_query, request->http_version);
    if (parsed != 3) {
        printf("❌ Invalid HTTP request line\n");
        return -1;
    }
    
    // Parse method
    request->method = parse_http_method(method_str);
    if (request->method == HTTP_METHOD_UNKNOWN) {
        printf("❌ Unknown HTTP method: %s\n", method_str);
        return -1;
    }
    
    // Split path and query string
    char* query_start = strchr(path_and_query, '?');
    if (query_start) {
        *query_start = '\0';
        strncpy(request->query_string, query_start + 1, sizeof(request->query_string) - 1);
        parse_query_string(request->query_string, request);
    } else {
        request->query_string[0] = '\0';
        request->query_param_count = 0;
    }
    
    strncpy(request->path, path_and_query, sizeof(request->path) - 1);
    
    // Parse headers
    char* header_start = line_end + 2;
    request->header_count = 0;
    
    while (request->header_count < TORCHLIGHT_MAX_HEADERS) {
        line_end = strstr(header_start, "\r\n");
        if (!line_end) break;
        
        // Check for end of headers
        if (line_end == header_start) {
            header_start = line_end + 2;
            break;
        }
        
        *line_end = '\0';
        
        char* colon = strchr(header_start, ':');
        if (colon) {
            *colon = '\0';
            
            // Trim whitespace
            char* value = colon + 1;
            while (*value == ' ' || *value == '\t') value++;
            
            strncpy(request->headers[request->header_count].name, header_start, 63);
            strncpy(request->headers[request->header_count].value, value, 511);
            request->header_count++;
        }
        
        header_start = line_end + 2;
    }
    
    // Parse body if present
    const char* content_length_str = torchlight_get_header(request, "Content-Length");
    if (content_length_str) {
        size_t content_length = (size_t)atol(content_length_str);
        
        if (content_length > 0 && content_length < TORCHLIGHT_MAX_REQUEST_SIZE) {
            // Calculate how much body data we already have
            size_t headers_length = header_start - buffer;
            size_t body_in_buffer = bytes_read - headers_length;
            
            request->body = malloc(content_length + 1);
            if (request->body) {
                // Copy body data already in buffer
                if (body_in_buffer > 0) {
                    memcpy(request->body, header_start, body_in_buffer);
                }
                
                // Read remaining body data if needed
                if (body_in_buffer < content_length) {
                    ssize_t remaining = recv(socket_fd, 
                                           request->body + body_in_buffer,
                                           content_length - body_in_buffer, 0);
                    if (remaining > 0) {
                        body_in_buffer += remaining;
                    }
                }
                
                request->body_length = body_in_buffer;
                request->body[request->body_length] = '\0';
            }
        }
    }
    
    // Check for session cookie
    const char* cookie_header = torchlight_get_header(request, "Cookie");
    if (cookie_header) {
        const char* session_start = strstr(cookie_header, "session_id=");
        if (session_start) {
            session_start += strlen("session_id=");
            const char* session_end = strchr(session_start, ';');
            if (!session_end) session_end = session_start + strlen(session_start);
            
            size_t session_length = session_end - session_start;
            if (session_length < sizeof(request->session_id)) {
                strncpy(request->session_id, session_start, session_length);
                request->session_id[session_length] = '\0';
                request->has_session = true;
            }
        }
    }
    
    printf("   Parsed: %s %s (headers: %d, body: %zu bytes)\n",
           HTTP_METHOD_STRINGS[request->method], request->path, 
           request->header_count, request->body_length);
    
    return 0;
}

int torchlight_send_response(int socket_fd, const http_response_t* response) {
    if (!response) return -1;
    
    // Build status line
    char status_line[256];
    snprintf(status_line, sizeof(status_line), "HTTP/1.1 %d %s\r\n", 
             response->status,
             response->status == HTTP_STATUS_OK ? "OK" :
             response->status == HTTP_STATUS_NOT_FOUND ? "Not Found" :
             response->status == HTTP_STATUS_INTERNAL_SERVER_ERROR ? "Internal Server Error" :
             response->status == HTTP_STATUS_BAD_REQUEST ? "Bad Request" : "Unknown");
    
    // Send status line
    send(socket_fd, status_line, strlen(status_line), 0);
    
    // Send Content-Type header
    const char* content_type = CONTENT_TYPE_STRINGS[response->content_type];
    char content_type_header[256];
    snprintf(content_type_header, sizeof(content_type_header), "Content-Type: %s\r\n", content_type);
    send(socket_fd, content_type_header, strlen(content_type_header), 0);
    
    // Send Content-Length header
    char content_length_header[128];
    snprintf(content_length_header, sizeof(content_length_header), "Content-Length: %zu\r\n", response->body_length);
    send(socket_fd, content_length_header, strlen(content_length_header), 0);
    
    // Send custom headers
    for (int i = 0; i < response->header_count; i++) {
        char header_line[640];
        snprintf(header_line, sizeof(header_line), "%s: %s\r\n",
                response->headers[i].name, response->headers[i].value);
        send(socket_fd, header_line, strlen(header_line), 0);
    }
    
    // Send empty line to end headers
    send(socket_fd, "\r\n", 2, 0);
    
    // Send body
    if (response->body && response->body_length > 0) {
        send(socket_fd, response->body, response->body_length, 0);
    }
    
    return 0;
}

const char* torchlight_get_header(const http_request_t* request, const char* name) {
    if (!request || !name) return NULL;
    
    for (int i = 0; i < request->header_count; i++) {
        if (strcasecmp(request->headers[i].name, name) == 0) {
            return request->headers[i].value;
        }
    }
    
    return NULL;
}

int torchlight_add_header(http_response_t* response, const char* name, const char* value) {
    if (!response || !name || !value) return -1;
    
    if (response->header_count >= TORCHLIGHT_MAX_HEADERS) {
        return -1;  // Too many headers
    }
    
    strncpy(response->headers[response->header_count].name, name, 63);
    strncpy(response->headers[response->header_count].value, value, 511);
    response->header_count++;
    
    return 0;
}

const char* torchlight_get_query_param(const http_request_t* request, const char* name) {
    if (!request || !name) return NULL;
    
    for (int i = 0; i < request->query_param_count; i++) {
        if (strcmp(request->query_params[i][0], name) == 0) {
            return request->query_params[i][1];
        }
    }
    
    return NULL;
}