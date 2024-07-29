/**
 * Created by James Raynor on 7/28/24.
 */
#pragma once

#include "defines.h"

agi_result_t install_font(const char *font_hash, const char *font_name, const char *font_style, const char *font_extension);

agi_result_t uninstall_font(const char *font_name, const char *font_style, const char *font_extension);