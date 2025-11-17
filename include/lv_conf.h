#ifndef LV_CONF_H
#define LV_CONF_H

/* Let LVGL find this header by name */
#define LV_CONF_INCLUDE_SIMPLE 1

#define LV_MEM_CUSTOM 1

#define LV_MEM_SIZE (32U * 1024U)    /*32 [bytes]*/


/* Use PSRAM for all LVGL dynamic allocations */
#define LV_USE_BUILTIN_MALLOC  0
#define LV_MEM_CUSTOM 1                 /* enable custom macros */
#define LV_MALLOC(sz)           heap_caps_malloc((sz), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define LV_REALLOC(p,sz)        heap_caps_realloc((p),(sz), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define LV_FREE(p)              heap_caps_free((p))

/* Display size (edit to match your panel) */
#define LV_HOR_RES 480
#define LV_VER_RES 320

/* Keep default color depth; change to 16 if you want RGB565 explicitly */
#define LV_COLOR_DEPTH 16

/* Optional: reduce features if you’re extremely tight on RAM/Flash */
// #define LV_USE_LOG 0

#define LV_USE_ASSERT 1


/* Fonts: keep minimal — big fonts eat Flash/DRAM */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* Theme/System defaults */
#define LV_USE_THEME_DEFAULT 1

/* Draw engine defaults */
#define LV_DRAW_BUF_ALIGN         4
#define LV_ATTRIBUTE_FAST_MEM

#endif // LV_CONF_H

#define LV_USE_IMG 1
#define LV_USE_IMGBTN 1
#define LV_IMG_CACHE_DEF_SIZE 4


