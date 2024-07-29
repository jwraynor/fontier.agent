#include <agi/defines.h>
#include <stdio.h>
#include <string.h>
#if defined(AGI_PLATFORM_APPLE)
#include <unistd.h>
#define Sleep(x) usleep(x * 1000)
#elif defined(AGI_PLATFORM_WINDOWS)
#include <winsock2.h>
#endif
#include <agi/log.h>

#include "agi/app.h"
#include "agi/fonts.h"
#include "agi/tcp_client.h"

void handle_auth_response(TcpClient *client, void *packet_data) {
    AuthResponsePacket *response = (AuthResponsePacket *)packet_data;
    agi_log_debug("Auth response: %s", response->success ? "Success" : "Failure");
    agi_log_debug("Message: %s", response->message);
}

void handle_font_install_request(TcpClient *client, void *packet_data) {
    FontInstallRequestPacket *request = (FontInstallRequestPacket *)packet_data;
    agi_log_debug("Font install request received for font: %s", request->font_name);
    if (request->install) {
        FontInstallResponsePacket response = {0};

        agi_result_t result = install_font(request->font_hash, request->font_name, request->font_style, request->font_extension);

        if (result == AGI_SUCCESS) {
            response.success = 1;
            snprintf(response.message, sizeof(response.message), "Font %s installed successfully", request->font_name);
        } else {
            response.success = 0;
            snprintf(response.message, sizeof(response.message), "Failed to install font %s", request->font_name);
        }

        tcp_client_send_packet(client, 3, &response, sizeof(response));
    } else {
        FontInstallResponsePacket response = {0};
        agi_result_t result = uninstall_font(request->font_name, request->font_style, request->font_extension);

        if (result == AGI_SUCCESS) {
            response.success = 1;
            snprintf(response.message, sizeof(response.message), "Font %s uninstalled successfully", request->font_name);
        } else {
            response.success = 0;
            snprintf(response.message, sizeof(response.message), "Failed to uninstall font %s", request->font_name);
        }

        tcp_client_send_packet(client, 3, &response, sizeof(response));
    }
    agi_log_debug("Font install response sent");
}

int main() {
    agi_log_set_level(AGI_LOG_LEVEL_DEBUG);
    App *app = app_new(.hostname = "192.168.1.36",
                       .port = 6969,
                       .auth_handler = handle_auth_response,
                       .font_install_handler = handle_font_install_request);

    if (app == NULL) {
        agi_log_debug("Failed to create app");
        return 1;
    }

    agi_result_t result = app_connect(app);
    if (result != AGI_SUCCESS) {
        agi_log_debug("Failed to connect to server");
        app_destroy(app);
        return 1;
    }

    // Register the font installation packet handler
    result = tcp_client_register_packet_handler(app->client, 4, sizeof(FontInstallRequestPacket), handle_font_install_request);
    if (result != AGI_SUCCESS) {
        agi_log_debug("Failed to register font install packet handler");
        app_destroy(app);
        return 1;
    }

    // Main loop to process incoming packets
    while (1) {
        result = tcp_client_process_packets(app->client);
        if (result != AGI_SUCCESS) {
            agi_log_debug("Error processing packets: %d", result);
            break;
        }
        Sleep(100);
    }

    app_destroy(app);
    return 0;
}