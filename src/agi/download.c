//
// Created by jraynor on 7/28/2024.
//

#include "agi/download.h"

#include <agi/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ws2tcpip.h>

#if defined(_WIN32)
#include <windns.h>
#pragma comment(lib, "Dnsapi.lib")
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#else
#include <curl/curl.h>
#endif

#define MAX_DOWNLOAD_ATTEMPTS 3
#define DOWNLOAD_TIMEOUT 30L
#define CHUNK_SIZE 16384

typedef struct {
    FILE* fp;
    size_t size;
} WriteData;

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    WriteData* wd = (WriteData*)userp;
    size_t written = fwrite(contents, size, nmemb, wd->fp);
    wd->size += written;
    return written;
}

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wininet.h>
#include <stdio.h>

#pragma comment(lib, "wininet.lib")


static agi_result_t download_file_windows(const char* url, const char* output_path) {
    HINTERNET hInternet, hConnect;
    HANDLE hFile;
    DWORD bytesRead, bytesWritten;
    char buffer[4096];
    BOOL result = FALSE;

    // Initialize WinINet
    hInternet = InternetOpen("DownloadAgent", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) {
        agi_log_error("Error initializing WinINet: %lu\n", GetLastError());
        return AGI_ERROR_NETWORK;
    }

    // Open the URL
    hConnect = InternetOpenUrl(hInternet, url, NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (!hConnect) {
        agi_log_error("Error opening URL: %lu\n", GetLastError());
        InternetCloseHandle(hInternet);
        return AGI_ERROR_NETWORK;
    }

    // Create the output file
    hFile = CreateFileA(output_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        agi_log_error("Error creating output file: %lu\n", GetLastError());
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return AGI_ERROR_IO;
    }

    // Download and write the file
    do {
        if (!InternetReadFile(hConnect, buffer, sizeof(buffer), &bytesRead)) {
            agi_log_error("Error reading from URL: %lu\n", GetLastError());
            break;
        }

        if (bytesRead == 0) {
            result = TRUE;  // Download complete
            break;
        }

        if (!WriteFile(hFile, buffer, bytesRead, &bytesWritten, NULL) || bytesWritten != bytesRead) {
            agi_log_error("Error writing to file: %lu\n", GetLastError());
            break;
        }
    } while (TRUE);

    // Clean up
    CloseHandle(hFile);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    if (result) {
        agi_log_debug("File downloaded successfully: %s\n", output_path);
        return AGI_SUCCESS;
    } else {
        DeleteFileA(output_path);  // Clean up partial download
        agi_log_error("Failed to download file\n");
        return AGI_ERROR_NETWORK;
    }
}
#else
static agi_result_t download_file_curl(const char* url, const char* output_path) {
    CURL* curl;
    CURLcode res;
    WriteData wd = {NULL, 0};
    int attempt = 0;
    agi_result_t result = AGI_ERROR_NETWORK;

    curl = curl_easy_init();
    if (!curl) {
        agi_log_error("Failed to initialize curl");
        return AGI_ERROR_NETWORK;
    }

    while (attempt < MAX_DOWNLOAD_ATTEMPTS) {
        wd.fp = fopen(output_path, "wb");
        if (!wd.fp) {
            agi_log_error("Failed to open file for writing: %s", output_path);
            curl_easy_cleanup(curl);
            return AGI_ERROR_IO;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &wd);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, DOWNLOAD_TIMEOUT);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

        res = curl_easy_perform(curl);
        fclose(wd.fp);

        if (res == CURLE_OK) {
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            if (http_code == 200 && wd.size > 0) {
                agi_log_info("File downloaded successfully: %s (Size: %zu bytes)", output_path, wd.size);
                result = AGI_SUCCESS;
                break;
            } else {
                agi_log_error("HTTP error: %ld", http_code);
            }
        } else {
            agi_log_error("Curl error on attempt %d: %s", attempt + 1, curl_easy_strerror(res));
        }

        attempt++;
        if (attempt < MAX_DOWNLOAD_ATTEMPTS) {
            agi_log_info("Retrying download (attempt %d of %d)...", attempt + 1, MAX_DOWNLOAD_ATTEMPTS);
            Sleep(1000 * attempt);  // Exponential backoff
        }
    }

    curl_easy_cleanup(curl);

    if (result != AGI_SUCCESS) {
        remove(output_path);  // Clean up partial download
        agi_log_error("Failed to download file after %d attempts", MAX_DOWNLOAD_ATTEMPTS);
    }

    return result;
}
#endif

agi_result_t download_file(const char* url, const char* output_path) {
#if defined(_WIN32)
    return download_file_windows(url, output_path);
#else
    return download_file_curl(url, output_path);
#endif
}