#pragma once

#include <cstddef>
#include <cstdint>

bool gpioIsPinAllowed(uint8_t pin);
bool gpioSet(uint8_t pin, bool state);
bool gpioTap(uint8_t pin, uint32_t pulse_ms);
bool gpioTapInProgress();
bool gpioGet(uint8_t pin, bool *state);
size_t gpioExportEntries(uint8_t *pins, bool *states, size_t max_count);
