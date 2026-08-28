#pragma once

#include <cstddef>
#include <cstdint>

void servoControllerInit();
bool servoSetAngle(uint8_t pin, uint8_t angle);
bool servoSetAuto(uint8_t pin, bool enabled, uint32_t pause_ms);
size_t servoExportEntries(uint8_t *pins, uint8_t *angles, bool *auto_flags, size_t max_count);
