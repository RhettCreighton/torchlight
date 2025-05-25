/*
 * TorchLight Utility Functions
 * String utilities, file operations, and helpers
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>
#include <time.h>
#include <pthread.h>
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
        if (isalnum(*src) || strchr("-_.~", *src)) {
            // Safe character
            *dst++ = *src;
            dst_remaining--;
        } else {
            // Encode character
            snprintf(dst, 4, "%%%02X", (unsigned char)*src);
            dst += 3;
            dst_remaining -= 3;
        }
        src++;
    }
    
    *dst = '\0';
    return 0;
}

// HTML escaping

int torchlight_html_escape(const char* input, char* output, size_t output_size) {
    if (!input || !output || output_size == 0) return -1;
    
    const char* src = input;
    char* dst = output;
    size_t dst_remaining = output_size - 1;
    
    while (*src && dst_remaining > 6) {  // Longest escape is &quot; (6 chars)
        const char* escape = NULL;
        
        switch (*src) {
            case '<': escape = "&lt;"; break;
            case '>': escape = "&gt;"; break;
            case '&': escape = "&amp;"; break;
            case '"': escape = "&quot;"; break;
            case '\'': escape = "&#39;"; break;
            default:
                *dst++ = *src;
                dst_remaining--;
                break;
        }
        
        if (escape) {
            size_t escape_len = strlen(escape);
            if (escape_len <= dst_remaining) {
                strcpy(dst, escape);
                dst += escape_len;
                dst_remaining -= escape_len;
            } else {
                break;  // Not enough space
            }
        }
        
        src++;
    }
    
    *dst = '\0';
    return 0;
}

// File utility functions

bool torchlight_file_exists(const char* path) {
    if (!path) return false;
    
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

int torchlight_read_file(const char* path, char** content, size_t* size) {
    if (!path || !content) return -1;
    
    FILE* file = fopen(path, "rb");
    if (!file) return -1;
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(file);
        return -1;
    }
    
    // Allocate buffer
    *content = malloc(file_size + 1);
    if (!*content) {
        fclose(file);
        return -1;
    }
    
    // Read file
    size_t bytes_read = fread(*content, 1, file_size, file);
    fclose(file);
    
    if (bytes_read != (size_t)file_size) {
        free(*content);
        *content = NULL;
        return -1;
    }
    
    (*content)[file_size] = '\0';
    if (size) *size = file_size;
    
    return 0;
}

// Session management utilities

static session_t g_sessions[TORCHLIGHT_MAX_SESSIONS] = {0};
static int g_session_count = 0;
static pthread_mutex_t g_session_mutex = PTHREAD_MUTEX_INITIALIZER;

// Generate random session ID
static void generate_session_id(char* session_id_out) {
    const char* chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    
    for (int i = 0; i < 63; i++) {
        session_id_out[i] = chars[rand() % 62];
    }
    session_id_out[63] = '\0';
}

int torchlight_create_session(const char* user_id, char* session_id_out) {
    pthread_mutex_lock(&g_session_mutex);
    
    if (g_session_count >= TORCHLIGHT_MAX_SESSIONS) {
        pthread_mutex_unlock(&g_session_mutex);
        return -1;  // Too many sessions
    }
    
    session_t* session = &g_sessions[g_session_count];
    
    generate_session_id(session->session_id);
    if (user_id) {
        strncpy(session->user_id, user_id, sizeof(session->user_id) - 1);
    }
    
    session->created_time = time(NULL);
    session->last_access_time = session->created_time;
    session->authenticated = (user_id != NULL);
    session->data[0] = '\0';
    
    strcpy(session_id_out, session->session_id);
    g_session_count++;
    
    pthread_mutex_unlock(&g_session_mutex);
    return 0;
}

session_t* torchlight_get_session(const char* session_id) {
    if (!session_id) return NULL;
    
    pthread_mutex_lock(&g_session_mutex);
    
    for (int i = 0; i < g_session_count; i++) {
        if (strcmp(g_sessions[i].session_id, session_id) == 0) {
            g_sessions[i].last_access_time = time(NULL);
            pthread_mutex_unlock(&g_session_mutex);
            return &g_sessions[i];
        }
    }
    
    pthread_mutex_unlock(&g_session_mutex);
    return NULL;
}

int torchlight_cleanup_sessions(void) {
    time_t now = time(NULL);
    int cleaned = 0;
    
    pthread_mutex_lock(&g_session_mutex);
    
    for (int i = g_session_count - 1; i >= 0; i--) {
        if (now - g_sessions[i].last_access_time > TORCHLIGHT_SESSION_TIMEOUT) {
            // Remove expired session
            for (int j = i; j < g_session_count - 1; j++) {
                g_sessions[j] = g_sessions[j + 1];
            }
            g_session_count--;
            cleaned++;
        }
    }
    
    pthread_mutex_unlock(&g_session_mutex);
    return cleaned;
}

// Security functions

int torchlight_add_security_headers(http_response_t* response) {
    if (!response) return -1;
    
    torchlight_add_header(response, "X-Content-Type-Options", "nosniff");
    torchlight_add_header(response, "X-Frame-Options", "DENY");
    torchlight_add_header(response, "X-XSS-Protection", "1; mode=block");
    torchlight_add_header(response, "Referrer-Policy", "strict-origin-when-cross-origin");
    
    return 0;
}