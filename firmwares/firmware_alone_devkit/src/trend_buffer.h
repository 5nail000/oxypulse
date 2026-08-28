#pragma once

#include <cstddef>
#include <cstdint>

void trendBufferInit();
void trendBufferPoll();

uint32_t trendBufferGetMask();
bool trendBufferApplySourcesJson(const char *json, size_t len);

using TrendJsonSink = bool (*)(const char *data, size_t len);
void trendBufferWriteJson(TrendJsonSink sink, size_t offset, size_t limit);
