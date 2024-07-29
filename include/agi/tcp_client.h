#pragma once
#include "defines.h"

typedef struct TcpClient TcpClient;
typedef void (*PacketHandler)(TcpClient *client, void *packet_data);

typedef struct {
    uint16_t type;
    size_t size;
    PacketHandler handler;
} PacketHandlerInfo;

TcpClient *tcp_client_create(const char *host, u16 port);
void tcp_client_destroy(TcpClient *client);
agi_result_t tcp_client_connect(TcpClient *client);
agi_result_t tcp_client_disconnect(TcpClient *client);
agi_result_t tcp_client_send_packet(TcpClient *client, uint16_t packet_type, const void *packet_data, size_t data_size);
agi_result_t tcp_client_register_packet_handler(TcpClient *client, uint16_t packet_type, size_t packet_size, PacketHandler handler);
agi_result_t tcp_client_process_packets(TcpClient *client);

#pragma pack(push, 1)
typedef struct {
    u32 version;
    char username[32];
    char hwid[64];
} AuthRequestPacket;

typedef struct {
    b8 success;
    char message[256];
} AuthResponsePacket;

// New struct for font installation request
typedef struct {
    char font_hash[64];
    char font_name[32];
    char font_style[32];
    char font_extension[32];
    b8 install;
} FontInstallRequestPacket;

// New struct for font installation response
typedef struct {
    b8 success;
    char message[256];
} FontInstallResponsePacket;

#pragma pack(pop)
