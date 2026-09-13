#pragma once

#include <cstddef>

#include <ArduinoJson.h>

#include "igt_session.h"

bool parseCommandJson(const char *json, size_t len);
size_t buildStatusJson(char *buffer, size_t capacity);
void fillIgtJson(JsonObject obj, const IgtPublicStatus &igt, bool include_phases);
