/*
 * TorchLight JSON API Helper
 * Simplified JSON handling for APIs
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "torchlight.h"

int torchlight_parse_json(const http_request_t* request, char** json_out) {
    if (!request || !json_out) return -1;
    
    const char* content_type = torchlight_get_header(request, "Content-Type");
    if (!content_type || strstr(content_type, "application/json") == NULL) {
        return -1;  // Not JSON content
    }
    
    if (!request->body || request->body_length == 0) {
        return -1;  // No body
    }
    
    *json_out = malloc(request->body_length + 1);
    if (!*json_out) return -1;
    
    memcpy(*json_out, request->body, request->body_length);
    (*json_out)[request->body_length] = '\0';
    
    return 0;
}

int torchlight_json_response(http_response_t* response, const char* data, const char* message) {
    if (!response) return -1;
    
    char* json_body = malloc(2048);
    if (!json_body) return -1;
    
    snprintf(json_body, 2048,
        "{\n"
        "  \"success\": true,\n"
        "  \"message\": \"%s\",\n"
        "  \"data\": %s\n"
        "}\n",
        message ? message : "OK",
        data ? data : "null");
    
    response->status = HTTP_STATUS_OK;
    response->content_type = CONTENT_TYPE_APPLICATION_JSON;
    response->body = json_body;
    response->body_length = strlen(json_body);
    
    return 0;
}

int torchlight_json_error(http_response_t* response, http_status_t status, const char* error_message) {
    if (!response) return -1;
    
    char* json_body = malloc(1024);
    if (!json_body) return -1;
    
    snprintf(json_body, 1024,
        "{\n"
        "  \"success\": false,\n"
        "  \"error\": \"%s\",\n"
        "  \"status\": %d\n"
        "}\n",
        error_message ? error_message : "Unknown error",
        status);
    
    response->status = status;
    response->content_type = CONTENT_TYPE_APPLICATION_JSON;
    response->body = json_body;
    response->body_length = strlen(json_body);
    
    return 0;
}