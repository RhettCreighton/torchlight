/*
 * TorchLight Utility Functions
 * String utilities, file operations, and helpers
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>
#include "torchlight.h"

// String utility functions

bool torchlight_string_starts_with(const char* str, const char* prefix) {
    if (!str || !prefix) return false;
    
    size_t str_len = strlen(str);
    size_t prefix_len = strlen(prefix);
    
    if (prefix_len > str_len) return false;
    
    return strncmp(str, prefix, prefix_len) == 0;
}

bool torchlight_string_ends_with(const char* str, const char* suffix) {
    if (!str || !suffix) return false;
    
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    
    if (suffix_len > str_len) return false;
    
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

int torchlight_string_replace(const char* input, const char* search, const char* replace, 
                             char* output, size_t output_size) {
    if (!input || !search || !replace || !output || output_size == 0) return -1;
    
    const char* src = input;
    char* dst = output;
    size_t dst_remaining = output_size - 1;  // Save space for null terminator
    size_t search_len = strlen(search);
    size_t replace_len = strlen(replace);
    
    while (*src && dst_remaining > 0) {
        const char* match = strstr(src, search);
        
        if (match && match == src) {
            // Found match at current position
            if (replace_len <= dst_remaining) {
                strcpy(dst, replace);
                dst += replace_len;
                dst_remaining -= replace_len;
                src += search_len;
            } else {
                // Not enough space for replacement
                break;
            }
        } else {
            // Copy one character
            *dst++ = *src++;
            dst_remaining--;
        }
    }
    
    *dst = '\0';
    return 0;
}

// URL encoding/decoding functions

static char hex_to_char(char c1, char c2) {
    int value = 0;
    if (c1 >= '0' && c1 <= '9') value += (c1 - '0') * 16;
    else if (c1 >= 'A' && c1 <= 'F') value += (c1 - 'A' + 10) * 16;
    else if (c1 >= 'a' && c1 <= 'f') value += (c1 - 'a' + 10) * 16;
    
    if (c2 >= '0' && c2 <= '9') value += (c2 - '0');
    else if (c2 >= 'A' && c2 <= 'F') value += (c2 - 'A' + 10);
    else if (c2 >= 'a' && c2 <= 'f') value += (c2 - 'a' + 10);
    
    return (char)value;
}

int torchlight_url_decode(const char* input, char* output, size_t output_size) {
    if (!input || !output || output_size == 0) return -1;
    
    const char* src = input;
    char* dst = output;
    size_t dst_remaining = output_size - 1;
    
    while (*src && dst_remaining > 0) {
        if (*src == '%' && src[1] && src[2]) {
            // URL encoded character
            *dst++ = hex_to_char(src[1], src[2]);
            src += 3;
            dst_remaining--;
        } else if (*src == '+') {
            // Space character
            *dst++ = ' ';
            src++;
            dst_remaining--;
        } else {
            // Regular character
            *dst++ = *src++;
            dst_remaining--;
        }
    }
    
    *dst = '\0';
    return 0;
}

int torchlight_url_encode(const char* input, char* output, size_t output_size) {
    if (!input || !output || output_size == 0) return -1;
    
    const char* src = input;
    char* dst = output;
    size_t dst_remaining = output_size - 1;
    
    while (*src && dst_remaining >= 4) {  // Need space for %XX plus null terminator
        if (isalnum(*src) || strchr(\"-_.~\", *src)) {\n            // Safe character\n            *dst++ = *src;\n            dst_remaining--;\n        } else {\n            // Encode character\n            snprintf(dst, 4, \"%%%-2X\", (unsigned char)*src);\n            dst += 3;\n            dst_remaining -= 3;\n        }\n        src++;\n    }\n    \n    *dst = '\\0';\n    return 0;\n}\n\n// HTML escaping\n\nint torchlight_html_escape(const char* input, char* output, size_t output_size) {\n    if (!input || !output || output_size == 0) return -1;\n    \n    const char* src = input;\n    char* dst = output;\n    size_t dst_remaining = output_size - 1;\n    \n    while (*src && dst_remaining > 6) {  // Longest escape is &quot; (6 chars)\n        const char* escape = NULL;\n        \n        switch (*src) {\n            case '<': escape = \"&lt;\"; break;\n            case '>': escape = \"&gt;\"; break;\n            case '&': escape = \"&amp;\"; break;\n            case '\"': escape = \"&quot;\"; break;\n            case '\\'': escape = \"&#39;\"; break;\n            default:\n                *dst++ = *src;\n                dst_remaining--;\n                break;\n        }\n        \n        if (escape) {\n            size_t escape_len = strlen(escape);\n            if (escape_len <= dst_remaining) {\n                strcpy(dst, escape);\n                dst += escape_len;\n                dst_remaining -= escape_len;\n            } else {\n                break;  // Not enough space\n            }\n        }\n        \n        src++;\n    }\n    \n    *dst = '\\0';\n    return 0;\n}\n\n// File utility functions\n\nbool torchlight_file_exists(const char* path) {\n    if (!path) return false;\n    \n    struct stat st;\n    return stat(path, &st) == 0 && S_ISREG(st.st_mode);\n}\n\nint torchlight_read_file(const char* path, char** content, size_t* size) {\n    if (!path || !content) return -1;\n    \n    FILE* file = fopen(path, \"rb\");\n    if (!file) return -1;\n    \n    // Get file size\n    fseek(file, 0, SEEK_END);\n    long file_size = ftell(file);\n    fseek(file, 0, SEEK_SET);\n    \n    if (file_size <= 0) {\n        fclose(file);\n        return -1;\n    }\n    \n    // Allocate buffer\n    *content = malloc(file_size + 1);\n    if (!*content) {\n        fclose(file);\n        return -1;\n    }\n    \n    // Read file\n    size_t bytes_read = fread(*content, 1, file_size, file);\n    fclose(file);\n    \n    if (bytes_read != (size_t)file_size) {\n        free(*content);\n        *content = NULL;\n        return -1;\n    }\n    \n    (*content)[file_size] = '\\0';\n    if (size) *size = file_size;\n    \n    return 0;\n}\n\n// Session management utilities\n\nstatic session_t g_sessions[TORCHLIGHT_MAX_SESSIONS] = {0};\nstatic int g_session_count = 0;\nstatic pthread_mutex_t g_session_mutex = PTHREAD_MUTEX_INITIALIZER;\n\n// Generate random session ID\nstatic void generate_session_id(char* session_id_out) {\n    const char* chars = \"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\";\n    \n    for (int i = 0; i < 63; i++) {\n        session_id_out[i] = chars[rand() % 62];\n    }\n    session_id_out[63] = '\\0';\n}\n\nint torchlight_create_session(const char* user_id, char* session_id_out) {\n    pthread_mutex_lock(&g_session_mutex);\n    \n    if (g_session_count >= TORCHLIGHT_MAX_SESSIONS) {\n        pthread_mutex_unlock(&g_session_mutex);\n        return -1;  // Too many sessions\n    }\n    \n    session_t* session = &g_sessions[g_session_count];\n    \n    generate_session_id(session->session_id);\n    if (user_id) {\n        strncpy(session->user_id, user_id, sizeof(session->user_id) - 1);\n    }\n    \n    session->created_time = time(NULL);\n    session->last_access_time = session->created_time;\n    session->authenticated = (user_id != NULL);\n    session->data[0] = '\\0';\n    \n    strcpy(session_id_out, session->session_id);\n    g_session_count++;\n    \n    pthread_mutex_unlock(&g_session_mutex);\n    return 0;\n}\n\nsession_t* torchlight_get_session(const char* session_id) {\n    if (!session_id) return NULL;\n    \n    pthread_mutex_lock(&g_session_mutex);\n    \n    for (int i = 0; i < g_session_count; i++) {\n        if (strcmp(g_sessions[i].session_id, session_id) == 0) {\n            g_sessions[i].last_access_time = time(NULL);\n            pthread_mutex_unlock(&g_session_mutex);\n            return &g_sessions[i];\n        }\n    }\n    \n    pthread_mutex_unlock(&g_session_mutex);\n    return NULL;\n}\n\nint torchlight_update_session(const char* session_id, const char* data) {\n    if (!session_id || !data) return -1;\n    \n    session_t* session = torchlight_get_session(session_id);\n    if (!session) return -1;\n    \n    pthread_mutex_lock(&g_session_mutex);\n    strncpy(session->data, data, sizeof(session->data) - 1);\n    session->data[sizeof(session->data) - 1] = '\\0';\n    pthread_mutex_unlock(&g_session_mutex);\n    \n    return 0;\n}\n\nint torchlight_destroy_session(const char* session_id) {\n    if (!session_id) return -1;\n    \n    pthread_mutex_lock(&g_session_mutex);\n    \n    for (int i = 0; i < g_session_count; i++) {\n        if (strcmp(g_sessions[i].session_id, session_id) == 0) {\n            // Shift remaining sessions down\n            for (int j = i; j < g_session_count - 1; j++) {\n                g_sessions[j] = g_sessions[j + 1];\n            }\n            g_session_count--;\n            pthread_mutex_unlock(&g_session_mutex);\n            return 0;\n        }\n    }\n    \n    pthread_mutex_unlock(&g_session_mutex);\n    return -1;  // Session not found\n}\n\nint torchlight_cleanup_sessions(void) {\n    time_t now = time(NULL);\n    int cleaned = 0;\n    \n    pthread_mutex_lock(&g_session_mutex);\n    \n    for (int i = g_session_count - 1; i >= 0; i--) {\n        if (now - g_sessions[i].last_access_time > TORCHLIGHT_SESSION_TIMEOUT) {\n            // Remove expired session\n            for (int j = i; j < g_session_count - 1; j++) {\n                g_sessions[j] = g_sessions[j + 1];\n            }\n            g_session_count--;\n            cleaned++;\n        }\n    }\n    \n    pthread_mutex_unlock(&g_session_mutex);\n    return cleaned;\n}\n\n// Security functions\n\nint torchlight_generate_csrf_token(char* token_out, size_t token_size) {\n    if (!token_out || token_size < 33) return -1;  // Need at least 32 chars + null\n    \n    const char* chars = \"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\";\n    \n    for (size_t i = 0; i < token_size - 1; i++) {\n        token_out[i] = chars[rand() % 62];\n    }\n    token_out[token_size - 1] = '\\0';\n    \n    return 0;\n}\n\nbool torchlight_validate_csrf_token(const http_request_t* request, const char* expected_token) {\n    if (!request || !expected_token) return false;\n    \n    const char* token = torchlight_get_header(request, \"X-CSRF-Token\");\n    if (!token) {\n        // Check form data for token\n        token = torchlight_get_query_param(request, \"csrf_token\");\n    }\n    \n    return token && strcmp(token, expected_token) == 0;\n}\n\nstatic time_t g_last_request_times[256] = {0};  // Simple rate limiting\nstatic int g_request_counts[256] = {0};\n\nbool torchlight_check_rate_limit(const char* client_id) {\n    if (!client_id) return false;\n    \n    // Simple hash of client ID\n    unsigned char hash = 0;\n    for (const char* p = client_id; *p; p++) {\n        hash ^= *p;\n    }\n    \n    time_t now = time(NULL);\n    \n    // Reset counter if minute has passed\n    if (now - g_last_request_times[hash] >= 60) {\n        g_request_counts[hash] = 0;\n        g_last_request_times[hash] = now;\n    }\n    \n    g_request_counts[hash]++;\n    \n    // Allow up to 60 requests per minute by default\n    return g_request_counts[hash] <= 60;\n}\n\nint torchlight_add_security_headers(http_response_t* response) {\n    if (!response) return -1;\n    \n    torchlight_add_header(response, \"X-Content-Type-Options\", \"nosniff\");\n    torchlight_add_header(response, \"X-Frame-Options\", \"DENY\");\n    torchlight_add_header(response, \"X-XSS-Protection\", \"1; mode=block\");\n    torchlight_add_header(response, \"Referrer-Policy\", \"strict-origin-when-cross-origin\");\n    \n    return 0;\n}"