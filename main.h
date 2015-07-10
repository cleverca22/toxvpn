/*
 * This program is libre software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the COPYING file for more details.
 */
#pragma once

#include <string>

#define USE_SELECT


void to_hex(char *a, const uint8_t *p, int size);
void hex_string_to_bin(const char *hex_string, uint8_t *ret);
void saveState(Tox *tox);

extern std::string myip;
