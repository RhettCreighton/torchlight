/*
 * TorchLight WebSocket Support
 * Basic WebSocket implementation for real-time features
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include "torchlight.h"

// WebSocket magic string for handshake
#define WEBSOCKET_MAGIC_STRING "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

// Base64 encoding function (simplified)
static char* base64_encode(const unsigned char* input, int length) {
    BIO *bio, *b64;
    BUF_MEM *bufferPtr;
    
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);
    
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, input, length);
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);
    
    char* encoded = malloc(bufferPtr->length + 1);
    if (encoded) {
        memcpy(encoded, bufferPtr->data, bufferPtr->length);
        encoded[bufferPtr->length] = '\0';
    }
    
    BIO_free_all(bio);
    return encoded;
}

bool torchlight_is_websocket_request(const http_request_t* request) {
    if (!request) return false;
    
    const char* connection = torchlight_get_header(request, "Connection");
    const char* upgrade = torchlight_get_header(request, "Upgrade");
    const char* ws_version = torchlight_get_header(request, "Sec-WebSocket-Version");
    const char* ws_key = torchlight_get_header(request, "Sec-WebSocket-Key");
    
    return (connection && strcasestr(connection, "upgrade") &&
            upgrade && strcasecmp(upgrade, "websocket") == 0 &&
            ws_version && strcmp(ws_version, "13") == 0 &&
            ws_key && strlen(ws_key) > 0);
}

int torchlight_websocket_handshake(int socket_fd, const http_request_t* request) {
    if (!torchlight_is_websocket_request(request)) {
        return -1;
    }
    
    const char* ws_key = torchlight_get_header(request, "Sec-WebSocket-Key");
    if (!ws_key) return -1;
    
    // Create accept key: SHA1(ws_key + magic_string) encoded in base64
    char key_and_magic[256];
    snprintf(key_and_magic, sizeof(key_and_magic), "%s%s", ws_key, WEBSOCKET_MAGIC_STRING);
    
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char*)key_and_magic, strlen(key_and_magic), hash);
    
    char* accept_key = base64_encode(hash, SHA_DIGEST_LENGTH);
    if (!accept_key) return -1;
    
    // Send WebSocket handshake response
    char response[1024];
    int response_length = snprintf(response, sizeof(response),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n", accept_key);
    
    free(accept_key);
    
    ssize_t sent = send(socket_fd, response, response_length, 0);
    if (sent != response_length) {
        return -1;
    }
    
    printf("✅ WebSocket handshake completed\n");
    return 0;
}

int torchlight_websocket_send(int socket_fd, const char* message, size_t length) {
    if (!message || length == 0) return -1;
    
    // Simple text frame format
    unsigned char frame[10];
    int frame_length = 0;
    
    // First byte: FIN + opcode (text frame = 0x81)
    frame[frame_length++] = 0x81;
    
    // Payload length
    if (length < 126) {
        frame[frame_length++] = (unsigned char)length;
    } else if (length < 65536) {
        frame[frame_length++] = 126;
        frame[frame_length++] = (length >> 8) & 0xFF;
        frame[frame_length++] = length & 0xFF;
    } else {
        // For simplicity, we don't support very large messages
        return -1;
    }
    
    // Send frame header
    if (send(socket_fd, frame, frame_length, 0) != frame_length) {
        return -1;
    }
    
    // Send payload
    if (send(socket_fd, message, length, 0) != (ssize_t)length) {
        return -1;
    }
    
    return 0;
}

int torchlight_websocket_receive(int socket_fd, char* buffer, size_t buffer_size, size_t* received_length) {
    if (!buffer || buffer_size == 0 || !received_length) return -1;
    
    // Read frame header (at least 2 bytes)
    unsigned char header[14];
    ssize_t header_bytes = recv(socket_fd, header, 2, 0);
    if (header_bytes != 2) return -1;
    
    // Parse frame
    bool fin = (header[0] & 0x80) != 0;
    unsigned char opcode = header[0] & 0x0F;
    bool masked = (header[1] & 0x80) != 0;
    unsigned char payload_length = header[1] & 0x7F;
    
    (void)fin;  // Unused for now
    
    // Handle different payload lengths
    size_t actual_length = payload_length;
    int mask_offset = 2;
    
    if (payload_length == 126) {
        // Extended payload length (16-bit)
        if (recv(socket_fd, header + 2, 2, 0) != 2) return -1;
        actual_length = (header[2] << 8) | header[3];
        mask_offset = 4;
    } else if (payload_length == 127) {
        // Extended payload length (64-bit) - not supported for simplicity
        return -1;
    }
    
    // Read mask if present
    unsigned char mask[4] = {0};
    if (masked) {
        if (recv(socket_fd, mask, 4, 0) != 4) return -1;
        mask_offset += 4;
    }
    
    // Check buffer size
    if (actual_length >= buffer_size) {
        return -1;  // Buffer too small
    }
    
    // Read payload
    ssize_t payload_bytes = recv(socket_fd, buffer, actual_length, 0);
    if (payload_bytes != (ssize_t)actual_length) return -1;
    
    // Unmask payload if needed
    if (masked) {
        for (size_t i = 0; i < actual_length; i++) {
            buffer[i] ^= mask[i % 4];
        }
    }
    
    buffer[actual_length] = '\0';
    *received_length = actual_length;
    
    // Handle different opcodes
    switch (opcode) {
        case 0x8:  // Close frame
            return -2;  // Connection closed
        case 0x9:  // Ping frame
            // Send pong response
            torchlight_websocket_send(socket_fd, buffer, actual_length);
            return 0;
        case 0xA:  // Pong frame
            return 0;  // Ignore pong frames
        case 0x1:  // Text frame
        case 0x2:  // Binary frame
            return 0;  // Normal data
        default:
            return -1;  // Unknown opcode
    }
}