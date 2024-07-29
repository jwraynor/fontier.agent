/**
 * Created by James Raynor on 7/28/24.
 */
#include "agi/fonts.h"

#include <agi/download.h>
#include <agi/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define WIN32_LEAN_AND_MEAN
#include <Shlwapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <windows.h>
#include <wininet.h>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "Shlwapi.lib")

#define AGI_FONT_URL "http://192.168.1.36:6968/perma/"
#define MAX_DOWNLOAD_ATTEMPTS 3
#define DOWNLOAD_TIMEOUT 30L
#define MAX_PATH 1024
#define HASH_LENGTH 64
// Removes anything after  a '\' and return a new string with the correct sized hash
static char* sanitize_hash(const char* hash) {
    size_t hash_len = strlen(hash);
    char* sanitized_hash = malloc(HASH_LENGTH + 1);  // +1 for null terminator
    if (!sanitized_hash) {
        agi_log_error("Failed to allocate memory for sanitized hash");
        return NULL;
    }

    // Initialize with zeros
    memset(sanitized_hash, 0, HASH_LENGTH + 1);

    // Find the start of the valid hash characters
    const char* start = hash;
    while (*start && !isalnum(*start)) {
        start++;
    }

    // Copy up to HASH_LENGTH valid characters
    size_t i = 0;
    while (*start && i < HASH_LENGTH) {
        if (isalnum(*start)) {
            sanitized_hash[i++] = *start;
        }
        start++;
    }

    // If we didn't get enough characters, pad with zeros
    while (i < HASH_LENGTH) {
        sanitized_hash[i++] = '0';
    }

    sanitized_hash[HASH_LENGTH] = '\0';  // Ensure null termination

    return sanitized_hash;
}
static char* create_url(const char* base_url, const char* font_hash, const char* font_extension) {
    char* clean_hash = sanitize_hash(font_hash);
    if (!clean_hash) {
        return NULL;
    }

    size_t url_length = strlen(base_url) + strlen(clean_hash) + strlen(font_extension) + 1;  // +1 for null terminator
    char* url = malloc(url_length);
    if (!url) {
        free(clean_hash);
        agi_log_error("Failed to allocate memory for URL");
        return NULL;
    }

    // Use snprintf to safely concatenate the strings
    snprintf(url, url_length, "%s%s%s", base_url, clean_hash, font_extension);

    free(clean_hash);
    return url;
}
static void get_temp_dir(char* temp_dir) {
    GetTempPathA(MAX_PATH, temp_dir);
}

static BOOL is_admin() {
    BOOL is_admin = FALSE;
    HANDLE token_handle = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token_handle)) {
        TOKEN_ELEVATION elevation;
        DWORD size = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(token_handle, TokenElevation, &elevation, sizeof(elevation), &size)) {
            is_admin = elevation.TokenIsElevated;
        }
    }
    if (token_handle) {
        CloseHandle(token_handle);
    }
    return is_admin;
}

agi_result_t install_font_windows(const char* font_path) {
    BOOL admin = is_admin();
    char font_dir[MAX_PATH];

    if (admin) {
        if (FAILED(SHGetFolderPathA(NULL, CSIDL_FONTS, NULL, 0, font_dir))) {
            agi_log_error("Failed to get system font directory");
            return AGI_ERROR_IO;
        }
    } else {
        if (FAILED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, font_dir))) {
            agi_log_error("Failed to get local app data directory");
            return AGI_ERROR_IO;
        }
        strcat(font_dir, "\\Microsoft\\Windows\\Fonts");
        CreateDirectoryA(font_dir, NULL);
    }

    char* font_name = PathFindFileNameA(font_path);
    char dest_path[MAX_PATH];
    snprintf(dest_path, sizeof(dest_path), "%s\\%s", font_dir, font_name);

    if (!CopyFileA(font_path, dest_path, FALSE)) {
        agi_log_error("Failed to copy font file, file may already exist");
        return AGI_ERROR_IO;
    }

    if (AddFontResourceA(dest_path) == 0) {
        agi_log_error("Failed to add font resource");
        return AGI_ERROR_IO;
    }

    SendMessageA(HWND_BROADCAST, WM_FONTCHANGE, 0, 0);

    if (admin) {
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
            RegSetValueExA(hKey, font_name, 0, REG_SZ, (BYTE*)dest_path, strlen(dest_path) + 1);
            RegCloseKey(hKey);
        } else {
            agi_log_error("Failed to add font to registry");
        }
    }

    return AGI_SUCCESS;
}

agi_result_t uninstall_font(const char* font_name, const char* font_style, const char* font_extension) {
    BOOL admin = is_admin();
    char font_dir[MAX_PATH];
    char font_path[MAX_PATH];

    if (admin) {
        if (FAILED(SHGetFolderPathA(NULL, CSIDL_FONTS, NULL, 0, font_dir))) {
            agi_log_error("Failed to get system font directory");
            return AGI_ERROR_IO;
        }
    } else {
        if (FAILED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, font_dir))) {
            agi_log_error("Failed to get local app data directory");
            return AGI_ERROR_IO;
        }
        strcat(font_dir, "\\Microsoft\\Windows\\Fonts");
    }

    snprintf(font_path, sizeof(font_path), "%s\\%s_%s%s", font_dir, font_name, font_style, font_extension);

    if (RemoveFontResourceA(font_path) == 0) {
        agi_log_error("Failed to remove font resource");
        return AGI_ERROR_IO;
    }

    if (!DeleteFileA(font_path)) {
        agi_log_error("Failed to delete font file");
        return AGI_ERROR_IO;
    }

    SendMessageA(HWND_BROADCAST, WM_FONTCHANGE, 0, 0);

    if (admin) {
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
            RegDeleteValueA(hKey, font_name);
            RegCloseKey(hKey);
        } else {
            agi_log_error("Failed to remove font from registry");
        }
    }

    agi_log_info("Font uninstalled successfully: %s", font_name);
    return AGI_SUCCESS;
}

agi_result_t install_font(const char* font_hash, const char* font_name, const char* font_style, const char* font_extension) {
    char* url = create_url(AGI_FONT_URL, font_hash, font_extension);
    if (!url) return AGI_ERROR_OUT_OF_MEMORY;

    char temp_dir[MAX_PATH];
    get_temp_dir(temp_dir);

    char output_file_location[MAX_PATH];
    snprintf(output_file_location, MAX_PATH, "%s%s_%s%s", temp_dir, font_name, font_style, font_extension);

    agi_result_t download_result = download_file(url, output_file_location);

    free(url);

    if (download_result != AGI_SUCCESS) {
        agi_log_error("Failed to download font");
        return AGI_ERROR_NETWORK;
    }

    return install_font_windows(output_file_location);
}

