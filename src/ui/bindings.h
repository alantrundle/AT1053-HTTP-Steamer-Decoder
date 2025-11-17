#pragma once
#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_update_stats_bars(uint8_t net_pct, uint8_t pcm_pct);
void ui_update_stats_decoder(const char* codec, uint32_t samplerate, uint8_t channels, uint16_t kbps);

#ifdef __cplusplus
}
#endif
