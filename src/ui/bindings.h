#pragma once
#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_update_stats_bars(uint8_t net_pct, uint8_t pcm_pct);

#ifdef __cplusplus
}
#endif
