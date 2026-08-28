#include "logger.h"

#include <Arduino.h>
#include <cstdarg>
#include <cstdio>

void logPrintf(const char *fmt, ...) {
    const uint32_t ms = millis();
    const uint32_t total_sec = ms / 1000U;
    const uint32_t hours = (total_sec / 3600U) % 24U;
    const uint32_t minutes = (total_sec / 60U) % 60U;
    const uint32_t seconds = total_sec % 60U;
    const uint32_t millis_part = ms % 1000U;

    char prefix[24];
    snprintf(prefix, sizeof(prefix), "[%02lu:%02lu:%02lu.%03lu] ",
             static_cast<unsigned long>(hours),
             static_cast<unsigned long>(minutes),
             static_cast<unsigned long>(seconds),
             static_cast<unsigned long>(millis_part));
    Serial.print(prefix);

    char body[192];
    va_list args;
    va_start(args, fmt);
    vsnprintf(body, sizeof(body), fmt, args);
    va_end(args);
    Serial.println(body);
}
