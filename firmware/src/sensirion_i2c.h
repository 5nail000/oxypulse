#pragma once

#include <cstddef>
#include <cstdint>

// SCD4x и родственные: CRC-8 init 0xFF (Sensirion environmental).
uint8_t sensirionCrc8(const uint8_t *data, size_t len);

// SFM3xxx mass flow: CRC-8 init 0x00 (Sensirion GF_AN mass flow meters).
uint8_t sensirionCrc8MassFlow(const uint8_t *data, size_t len);

bool sensirionSendCommand(uint8_t address, uint16_t command);
bool sensirionSendCommandArg(uint8_t address, uint16_t command, uint16_t argument);

// SCD4x — CRC init 0xFF
bool sensirionReadWords(uint8_t address, uint16_t *words, size_t count);
bool sensirionReadAfterCommand(uint8_t address,
                               uint16_t command,
                               uint16_t *words,
                               size_t count,
                               uint32_t delay_ms);

// SFM3300 / SFM3xxx — CRC init 0x00
bool sensirionReadWordsMassFlow(uint8_t address, uint16_t *words, size_t count);
bool sensirionReadAfterCommandMassFlow(uint8_t address,
                                       uint16_t command,
                                       uint16_t *words,
                                       size_t count,
                                       uint32_t delay_ms);

// 0x1000 + пауза + первое чтение под одним I2C-lock (без чужих транзакций).
bool sensirionStartContinuousMassFlow(uint8_t address,
                                      uint16_t command,
                                      uint16_t *first_word,
                                      uint32_t delay_ms,
                                      uint8_t attempts);
