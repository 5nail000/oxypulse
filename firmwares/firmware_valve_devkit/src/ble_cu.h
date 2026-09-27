#pragma once

#include <cstdint>

void bleCuInit();
void bleCuPoll();
void bleCuNoteRx();
uint32_t bleCuLastRxMs();
bool bleCuClientConnected();
