#pragma once
#include <lvgl.h>

struct UIApp;

struct UIPlayerView {
    UIApp*   app;
    lv_obj_t* root;

    lv_obj_t* art_placeholder;   // 200x200, JPEG to be wired later

    lv_obj_t* lbl_title;
    lv_obj_t* lbl_artist;
    lv_obj_t* lbl_album;

    lv_obj_t* slider_pos;
    lv_obj_t* lbl_time;

    lv_obj_t* slider_vol;
    lv_obj_t* lbl_vol;

    // Transport buttons (not wired yet)
    lv_obj_t* btn_prev;
    lv_obj_t* btn_play;
    lv_obj_t* btn_next;
    lv_obj_t* btn_stop;
};

UIPlayerView* ui_playerview_create(UIApp* app, lv_obj_t* parent);

// Reuse card style
void ui_apply_card_style(lv_obj_t* card);
