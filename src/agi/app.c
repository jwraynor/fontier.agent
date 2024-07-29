#include <agi/app.h>
#include <agi/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agi/defines.h"
#include "agi/tcp_client.h"

#if defined(AGI_PLATFORM_APPLE)
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <pwd.h>
#elif defined(AGI_PLATFORM_WINDOWS)
#include <intrin.h>
#include <windows.h>
#endif

#define HASH_SIZE 64
#define HWID_SIZE 32

// Function prototypes
static void get_current_username(char* username, size_t max_length);
static void get_motherboard_id(unsigned char* hwid, size_t hwid_size);
static void compute_hwid_hash(const unsigned char* hwid, size_t hwid_size, char* hash_str, size_t hash_str_size);

// Custom 32-bit hash function
static uint32_t custom_hash_32(const unsigned char* data, size_t length) {
    uint32_t hash = 0x811C9DC5;  // FNV offset basis
    for (size_t i = 0; i < length; i++) {
        hash ^= (uint8_t)data[i];
        hash *= 0x01000193;  // FNV prime
    }
    return hash;
}

// Computes an 8-character hash string from the input data
static void compute_hwid_hash(const unsigned char* hwid, size_t hwid_size, char* hash_str, size_t hash_str_size) {
    uint32_t hash = custom_hash_32(hwid, hwid_size);
    snprintf(hash_str, hash_str_size, "%08x", hash);
}

static void get_motherboard_id(unsigned char* hwid, size_t hwid_size) {
    memset(hwid, 0, hwid_size);  // Initialize the buffer
#if defined(AGI_PLATFORM_APPLE)
    io_service_t platformExpert = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching("IOPlatformExpertDevice"));
    if (platformExpert) {
        CFTypeRef serialNumberAsCFString = IORegistryEntryCreateCFProperty(platformExpert, __CFStringMakeConstantString("IOPlatformSerialNumber"), kCFAllocatorDefault, 0);
        if (serialNumberAsCFString) {
            CFStringGetCString((CFStringRef)serialNumberAsCFString, (char*)hwid, hwid_size, kCFStringEncodingUTF8);
            CFRelease(serialNumberAsCFString);
        }
        IOObjectRelease(platformExpert);
    }
#elif defined(AGI_PLATFORM_WINDOWS)
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD type;
        DWORD dataSize = hwid_size;
        RegQueryValueExA(hKey, "SystemManufacturer", NULL, &type, hwid, &dataSize);
        RegCloseKey(hKey);
    }
#endif
    // Ensure the string is null-terminated
    hwid[hwid_size - 1] = '\0';
}

// Wrapper function to handle authentication
agi_result_t handle_authentication(TcpClient* client) {
    char username[32];
    unsigned char hwid[HWID_SIZE];
    char hwid_hash[HASH_SIZE + 1];  // 8 hex chars + null terminator

    get_current_username(username, sizeof(username));
    get_motherboard_id(hwid, HWID_SIZE);
    compute_hwid_hash(hwid, strlen((char*)hwid), hwid_hash, sizeof(hwid_hash));

    AuthRequestPacket auth_packet = {
        .version = 0x01
    };
    strncpy(auth_packet.username, username, sizeof(auth_packet.username) - 1);
    strncpy(auth_packet.hwid, hwid_hash, sizeof(auth_packet.hwid) - 1);

    return tcp_client_send_packet(client, 0, &auth_packet, sizeof(auth_packet));
}

// Get the current user's username
static void get_current_username(char* username, size_t max_length) {
#if defined(AGI_PLATFORM_APPLE)
    struct passwd* pwd = getpwuid(getuid());
    if (pwd != NULL) {
        strncpy(username, pwd->pw_name, max_length - 1);
        username[max_length - 1] = '\0';
    } else {
        strncpy(username, "unknown", max_length - 1);
    }
#elif defined(AGI_PLATFORM_WINDOWS)
    DWORD username_len = max_length;
    if (!GetUserNameA(username, &username_len)) {
        strncpy(username, "unknown", max_length - 1);
    }
#endif
}

// Get a unique hardware ID
static void get_hardware_id(unsigned char* hwid, size_t hwid_size) {
#if defined(AGI_PLATFORM_APPLE)
    uuid_t uuid;
    uuid_generate(uuid);
    memcpy(hwid, uuid, MIN(sizeof(uuid), hwid_size));
#elif defined(AGI_PLATFORM_WINDOWS)
        int cpu_info[4] = {0};
        __cpuid(cpu_info, 0);
        memcpy(hwid, cpu_info, MIN(sizeof(cpu_info), hwid_size));
#endif
}

 App* app_create(AppDescriptor* descriptor) {
    App* app = malloc(sizeof(App));
    if (app == NULL) {
        return NULL;
    }

    app->client = tcp_client_create(descriptor->hostname, descriptor->port);
    TcpClient* client = app->client;
    if (client == NULL) {
        agi_log_error("Failed to create TCP client");
        free(app);
        return NULL;
    }

    tcp_client_register_packet_handler(client, 1, sizeof(AuthResponsePacket), descriptor->auth_handler);
    // Register the font installation packet handler
    tcp_client_register_packet_handler(client, 2, sizeof(FontInstallRequestPacket), descriptor->font_install_handler);

    app->descriptor = descriptor;
    return app;
}

 agi_result_t app_connect(App* app) {
    agi_result_t result = tcp_client_connect(app->client);
    if (result != AGI_SUCCESS) {
        agi_log_error("Failed to connect to server");
        return result;
    }

    agi_log_info("Successfully connected to server");
    return handle_authentication(app->client);
}

 void app_destroy(App* app) {
    tcp_client_disconnect(app->client);
    tcp_client_destroy(app->client);
    free(app);
}