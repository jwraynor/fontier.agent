//
// Created by jraynor on 7/27/2024.
//
#pragma once
#include "defines.h"
// Logging
typedef enum {
    AGI_LOG_LEVEL_DEBUG,
    AGI_LOG_LEVEL_INFO,
    AGI_LOG_LEVEL_WARNING,
    AGI_LOG_LEVEL_ERROR
} agi_log_level_t;

void agi_log_set_level(agi_log_level_t level);
void agi_log_message(agi_log_level_t level, const char *file, int line, const char *format, ...);

#define agi_log_debug(format, ...) agi_log_message(AGI_LOG_LEVEL_DEBUG, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define agi_log_info(format, ...) agi_log_message(AGI_LOG_LEVEL_INFO, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define agi_log_warning(format, ...) agi_log_message(AGI_LOG_LEVEL_WARNING, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define agi_log_error(format, ...) agi_log_message(AGI_LOG_LEVEL_ERROR, __FILE__, __LINE__, format, ##__VA_ARGS__)
