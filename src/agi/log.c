#include "agi/log.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#if defined(AGI_PLATFORM_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(AGI_PLATFORM_LINUX)
#include <sys/time.h>
#endif

// ANSI color codes
#define COLOR_RESET   "\x1b[0m"
#define COLOR_BOLD    "\x1b[1m"
#define COLOR_DIM     "\x1b[2m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_RED     "\x1b[31m"

// Logging implementation
static agi_log_level_t current_log_level = AGI_LOG_LEVEL_INFO;

void agi_log_set_level(agi_log_level_t level) {
    current_log_level = level;
}

static const char *get_filename(const char *path) {
    const char *filename = strrchr(path, '/');
    if (filename == NULL) {
        filename = strrchr(path, '\\');
    }
    return filename ? filename + 1 : path;
}

void agi_log_message(agi_log_level_t level, const char *file, int line, const char *format, ...) {
    if (level < current_log_level) return;

    const char *level_str[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    const char *level_color[] = {COLOR_BLUE, COLOR_GREEN, COLOR_YELLOW, COLOR_RED};

    char timestamp[32];
    time_t now;
    struct tm *tm_info;

    time(&now);
    tm_info = localtime(&now);

    strftime(timestamp, sizeof(timestamp), "%I:%M:%S %p", tm_info);

    fprintf(stderr, "%s%s%s ", COLOR_DIM, timestamp, COLOR_RESET);
    fprintf(stderr, "%s%s%-5s%s ", COLOR_BOLD, level_color[level], level_str[level], COLOR_RESET);
    fprintf(stderr, "%s%s:%d:%s ", COLOR_DIM, get_filename(file), line, COLOR_RESET);

    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n");
    fflush(stderr);
}
