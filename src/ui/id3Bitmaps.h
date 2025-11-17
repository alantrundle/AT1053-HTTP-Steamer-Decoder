#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// All ID3 icons are 14x14 in RGB565
constexpr uint32_t ID3_ICON_W = 14;
constexpr uint32_t ID3_ICON_H = 14;

/* 14×14 ID3 icons as LVGL image descriptors (LVGL 9.x, flash-resident) */

extern const lv_image_dsc_t ID3_ICON_ARTIST_14;
extern const lv_image_dsc_t ID3_ICON_TITLE_14;
extern const lv_image_dsc_t ID3_ICON_ALBUM_14;
extern const lv_image_dsc_t ID3_ICON_TRACK_14;
extern const lv_image_dsc_t ID3_ICON_GENRE_14;
extern const lv_image_dsc_t ID3_ICON_YEAR_14;

#ifdef __cplusplus
} // extern "C"
#endif


