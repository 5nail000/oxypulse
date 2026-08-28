#pragma once

#include <cstddef>
#include <cstdint>

void hrvSessionInit();
bool hrvSessionStart(uint32_t duration_sec);
void hrvSessionStop();
void hrvSessionFeedRr(uint16_t rr_ms);
void hrvSessionPoll(bool belt_connected);
using HrvJsonSink = bool (*)(const char *data, size_t len);
bool hrvSessionStreamJson(HrvJsonSink sink);
bool hrvDurationAllowed(uint32_t duration_sec);
