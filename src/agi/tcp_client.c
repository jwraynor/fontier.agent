#include "agi/tcp_client.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "agi/log.h"
#ifdef AGI_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#define close closesocket
typedef int ssize_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
#endif

#define MAX_PACKET_HANDLERS 256
#define MAX_PACKET_SIZE 1024 // Adjust this value as needed



struct TcpClient {
    int socket;
    struct sockaddr_in server_addr;
    PacketHandlerInfo handlers[MAX_PACKET_HANDLERS];
    size_t handler_count;
};

static int initialize_winsock(void) {
#ifdef AGI_PLATFORM_WINDOWS
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0) {
        agi_log_error("WSAStartup failed with error: %d", result);
        return -1;
    }
    return 0;
#else
    return 0;
#endif
}

static void cleanup_winsock(void) {
#ifdef AGI_PLATFORM_WINDOWS
    WSACleanup();
#endif
}

TcpClient *tcp_client_create(const char *host, u16 port) {
    if (initialize_winsock() != 0) {
        agi_log_error("Failed to initialize Winsock");
        return NULL;
    }

    TcpClient *client = (TcpClient *) malloc(sizeof(TcpClient));
    if (!client) {
        cleanup_winsock();
        return NULL;
    }

    client->socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client->socket == -1) {
        free(client);
        cleanup_winsock();
        return NULL;
    }

    memset(&client->server_addr, 0, sizeof(client->server_addr));
    client->server_addr.sin_family = AF_INET;
    client->server_addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &client->server_addr.sin_addr);
    client->handler_count = 0;
    return client;
}


agi_result_t tcp_client_register_packet_handler(TcpClient *client, uint16_t packet_type, size_t packet_size,
                                                PacketHandler handler) {
    if (client->handler_count >= MAX_PACKET_HANDLERS) {
        return AGI_ERROR_INVALID_ARGUMENT;
    }

    client->handlers[client->handler_count] = (PacketHandlerInfo){
        .type = packet_type,
        .size = packet_size,
        .handler = handler
    };
    client->handler_count++;

    return AGI_SUCCESS;
}

void tcp_client_destroy(TcpClient *client) {
    if (client) {
        close(client->socket);
        free(client);
    }
    cleanup_winsock();
}

agi_result_t tcp_client_connect(TcpClient *client) {
    int result = connect(client->socket, (struct sockaddr *) &client->server_addr, sizeof(client->server_addr));
    if (result == -1) {
#ifdef AGI_PLATFORM_WINDOWS
        int error = WSAGetLastError();
        agi_log_error("Failed to connect. WSA error: %d", error);
#else
        agi_log_error("Failed to connect. Error: %s", strerror(errno));
#endif
        return AGI_ERROR_NETWORK;
    }
    return AGI_SUCCESS;
}

agi_result_t tcp_client_disconnect(TcpClient *client) {
    if (close(client->socket) == -1) {
        return AGI_ERROR_NETWORK;
    }
    return AGI_SUCCESS;
}

agi_result_t tcp_client_send_packet(TcpClient *client, uint16_t packet_type, const void *packet_data,
                                    size_t data_size) {
    uint8_t *buffer = malloc(sizeof(uint16_t) + data_size);
    if (!buffer) {
        return AGI_ERROR_OUT_OF_MEMORY;
    }

    memcpy(buffer, &packet_type, sizeof(uint16_t));
    memcpy(buffer + sizeof(uint16_t), packet_data, data_size);

    ssize_t sent = send(client->socket, (const char *) buffer, sizeof(uint16_t) + data_size, 0);
    free(buffer);

    if (sent == -1) {
        return AGI_ERROR_NETWORK;
    }
    return AGI_SUCCESS;
}

static PacketHandlerInfo *find_packet_handler(TcpClient *client, uint16_t packet_type) {
    for (size_t i = 0; i < client->handler_count; i++) {
        if (client->handlers[i].type == packet_type) {
            return &client->handlers[i];
        }
    }
    return NULL;
}

agi_result_t tcp_client_process_packets(TcpClient *client) {
    uint8_t buffer[MAX_PACKET_SIZE];
    ssize_t received = recv(client->socket, (char *) buffer, sizeof(buffer), 0);

    if (received <= 0) {
        return AGI_ERROR_NETWORK;
    }

    size_t processed = 0;
    while (processed < received) {
        if (received - processed < sizeof(uint16_t)) {
            // Not enough data for packet type
            break;
        }

        uint16_t packet_type;
        memcpy(&packet_type, buffer + processed, sizeof(uint16_t));
        processed += sizeof(uint16_t);

        PacketHandlerInfo *handler = find_packet_handler(client, packet_type);
        if (!handler) {
            printf("Unknown packet type: %d\n", packet_type);
            return AGI_ERROR_INVALID_ARGUMENT;
        }

        if (received - processed < handler->size) {
            // Not enough data for full packet
            break;
        }

        void *packet_data = malloc(handler->size);
        if (!packet_data) {
            return AGI_ERROR_OUT_OF_MEMORY;
        }

        memcpy(packet_data, buffer + processed, handler->size);
        processed += handler->size;

        handler->handler(client, packet_data);
        free(packet_data);
    }

    return AGI_SUCCESS;
}
