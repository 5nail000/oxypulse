#pragma once

#include <cstddef>

bool parseCommandJson(const char *json, size_t len);
size_t buildStatusJson(char *buffer, size_t capacity);
