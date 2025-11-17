#pragma once
#include <lvgl.h>

struct UIApp;

struct UIMainView {
    UIApp*   app;
    lv_obj_t* root;
    lv_obj_t* btn_stats;
    lv_obj_t* btn_player;
};

UIMainView* ui_mainview_create(UIApp* app, lv_obj_t* parent);
