#pragma once

#include <cstdint>

void uartTxInit();
void uartTxStart();
void uartTxSendRr(uint16_t rr_ms);
