/*
 * TorchLight Template Engine
 * Simple variable substitution for dynamic content
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "torchlight.h"

// Simple template variable substitution
// Replaces {{variable}} with values from JSON object

static char* find_json_value(const char* json, const char* key) {
    if (!json || !key) return NULL;
    
    // Simple JSON value extraction (not a full parser)
    char search_pattern[128];
    snprintf(search_pattern, sizeof(search_pattern), "\"%s\":", key);
    
    const char* key_pos = strstr(json, search_pattern);
    if (!key_pos) return NULL;
    
    // Find the value after the colon
    const char* value_start = strchr(key_pos, ':');
    if (!value_start) return NULL;
    
    value_start++;
    while (*value_start == ' ' || *value_start == '\t') value_start++;
    
    const char* value_end;
    if (*value_start == '"') {
        // String value
        value_start++;
        value_end = strchr(value_start, '"');
    } else {
        // Number or boolean value
        value_end = value_start;
        while (*value_end && *value_end != ',' && *value_end != '}' && *value_end != '\n') {
            value_end++;
        }
    }
    
    if (!value_end) return NULL;
    
    size_t value_length = value_end - value_start;
    char* value = malloc(value_length + 1);
    if (!value) return NULL;
    
    strncpy(value, value_start, value_length);
    value[value_length] = '\0';
    
    // Trim trailing whitespace
    while (value_length > 0 && (value[value_length - 1] == ' ' || value[value_length - 1] == '\t')) {
        value[--value_length] = '\0';
    }
    
    return value;
}

int torchlight_substitute_variables(const char* template_str, const char* variables_json,
                                   char** output, size_t* output_size) {
    if (!template_str || !output) return -1;
    
    size_t template_length = strlen(template_str);
    size_t result_capacity = template_length * 2;  // Start with 2x template size
    char* result = malloc(result_capacity);
    if (!result) return -1;
    
    const char* src = template_str;
    char* dst = result;
    size_t dst_used = 0;
    
    while (*src) {
        if (src[0] == '{' && src[1] == '{') {
            // Found variable start
            const char* var_start = src + 2;
            const char* var_end = strstr(var_start, "}}");
            
            if (var_end) {
                // Extract variable name
                size_t var_name_length = var_end - var_start;
                char* var_name = malloc(var_name_length + 1);
                if (!var_name) {
                    free(result);
                    return -1;
                }
                
                strncpy(var_name, var_start, var_name_length);
                var_name[var_name_length] = '\0';
                
                // Get variable value
                char* var_value = NULL;
                if (variables_json) {
                    var_value = find_json_value(variables_json, var_name);
                }
                
                if (!var_value) {
                    var_value = strdup("");  // Empty string for missing variables
                }
                
                // Check if we need to expand the result buffer
                size_t value_length = strlen(var_value);
                if (dst_used + value_length >= result_capacity) {
                    result_capacity = (dst_used + value_length + 1024) * 2;
                    char* new_result = realloc(result, result_capacity);
                    if (!new_result) {
                        free(var_name);
                        free(var_value);
                        free(result);
                        return -1;
                    }
                    result = new_result;
                    dst = result + dst_used;
                }
                
                // Copy variable value to result
                strcpy(dst, var_value);
                dst += value_length;
                dst_used += value_length;
                
                free(var_name);
                free(var_value);
                
                src = var_end + 2;  // Skip past }}
            } else {
                // No closing }}, treat as literal text
                *dst++ = *src++;
                dst_used++;
            }
        } else {
            // Regular character
            if (dst_used >= result_capacity - 1) {
                result_capacity *= 2;
                char* new_result = realloc(result, result_capacity);
                if (!new_result) {
                    free(result);
                    return -1;
                }
                result = new_result;
                dst = result + dst_used;
            }
            
            *dst++ = *src++;
            dst_used++;
        }
    }
    
    *dst = '\0';
    *output = result;
    if (output_size) *output_size = dst_used;
    
    return 0;
}

int torchlight_render_template(const char* template_path, const char* variables_json, 
                              char** output, size_t* output_size) {
    if (!template_path || !output) return -1;
    
    // Read template file
    FILE* file = fopen(template_path, "r");
    if (!file) return -1;
    
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(file);
        return -1;
    }
    
    char* template_content = malloc(file_size + 1);
    if (!template_content) {
        fclose(file);
        return -1;
    }
    
    size_t bytes_read = fread(template_content, 1, file_size, file);
    fclose(file);
    
    if (bytes_read != (size_t)file_size) {
        free(template_content);
        return -1;
    }
    
    template_content[file_size] = '\0';
    
    // Perform variable substitution
    int result = torchlight_substitute_variables(template_content, variables_json, output, output_size);
    
    free(template_content);
    return result;
}