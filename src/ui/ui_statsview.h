#pragma once
#include <lvgl.h>

struct UIApp;

struct UIStatsView {
    UIApp*   app;
    lv_obj_t* root;

    // Bars
    lv_obj_t* bar_net;
    lv_obj_t* bar_pcm;
    lv_obj_t* lbl_net_val;
    lv_obj_t* lbl_pcm_val;

    // Wi-Fi
    lv_obj_t* lbl_wifi_status;
    lv_obj_t* lbl_wifi_ip;

    // Decoder
    lv_obj_t* lbl_dec_codec;
    lv_obj_t* lbl_dec_sr;
    lv_obj_t* lbl_dec_ch;
    lv_obj_t* lbl_dec_br;

    // Outputs
    lv_obj_t* lbl_out_i2s;
    lv_obj_t* lbl_out_a2dp;
};

UIStatsView* ui_statsview_create(UIApp* app, lv_obj_t* parent);

// Card style helper from ui_app.cpp
void ui_apply_card_style(lv_obj_t* card);
