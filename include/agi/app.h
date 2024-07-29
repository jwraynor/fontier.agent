/**
 * Created by James Raynor on 7/27/24.
 */
#pragma once
#include "defines.h"
#include "tcp_client.h"

// A packet listener for command packets
typedef struct AppDescriptor {
    const char hostname[32];
    const u16 port;
    PacketHandler command_handler;
    PacketHandler auth_handler;
    PacketHandler font_install_handler;  // New handler for font installation
} AppDescriptor;

typedef struct App {
    TcpClient *client;
    AppDescriptor *descriptor;
} App;

// Function prototypes
App *app_create(AppDescriptor *descriptor);
agi_result_t app_connect(App *app);
void app_destroy(App *app);

// AppDescriptor macro
#define APP_OF(...) \
    &(AppDescriptor) { __VA_ARGS__ }

#define app_new(...) \
    app_create(APP_OF(__VA_ARGS__))