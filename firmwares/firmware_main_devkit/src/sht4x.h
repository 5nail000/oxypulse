#pragma once

// SHT4x на рабочей шине, адрес 0x44. Влажность для компенсации STC31.
bool sht4xRead(float &temp_c, float &rh_percent);
