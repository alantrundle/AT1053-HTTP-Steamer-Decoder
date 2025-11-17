#pragma once
#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_update_stats_bars(uint8_t net_pct, uint8_t pcm_pct);
void ui_update_stats_decoder(const char* codec, uint32_t samplerate, uint8_t channels, uint16_t kbps);
void ui_update_stats_outputs(bool i2s_on, bool a2dp_on, const char* a2dp_name);
void ui_update_stats_wifi(bool connected, const char* ssid, const char* ip);

#ifdef __cplusplus
}
#endif
