#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *obj0;
    lv_obj_t *stats_lbl_pct_net;
    lv_obj_t *stats_lbl_pct_pcm;
    lv_obj_t *stats_bar_net;
    lv_obj_t *stats_bar_pcm;
    lv_obj_t *stats_lbl_bitrate;
    lv_obj_t *stats_lbl_channels;
    lv_obj_t *stats_lbl_samplerate;
    lv_obj_t *stats_lbl_codec;
    lv_obj_t *stats_lbl_a2dp_name;
    lv_obj_t *stats_lbl_i2s_on;
    lv_obj_t *stats_lbl_a2dp_on;
    lv_obj_t *stats_lbl_a2dp_name_1;
    lv_obj_t *stats_lbl_i2s_on_1;
    lv_obj_t *stats_lbl_a2dp_on_1;
    lv_obj_t *stats_lbl_bitrate_1;
    lv_obj_t *stats_lbl_wifi_ssid;
    lv_obj_t *stats_lbl_wifi_ipaddress;
    lv_obj_t *stats_lbl_wifi_on;
} objects_t;

extern objects_t objects;

enum ScreensEnum {
    SCREEN_ID_MAIN = 1,
};

void create_screen_main();
void tick_screen_main();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/