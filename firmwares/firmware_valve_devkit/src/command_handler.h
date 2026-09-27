#pragma once

#include <cstddef>

bool parseCommandJson(const char *json, size_t len);
size_t buildLiveJson(char *buffer, size_t capacity);
