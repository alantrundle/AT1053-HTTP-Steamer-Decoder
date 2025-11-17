#include "ui_mainview.h"
#include "ui_app.h"

static void on_stats(lv_event_t* e) {
    UIApp* app = static_cast<UIApp*>(lv_event_get_user_data(e));
    ui_show_stats(app);
}

static void on_player(lv_event_t* e) {
    UIApp* app = static_cast<UIApp*>(lv_event_get_user_data(e));
    ui_show_player(app);
}

UIMainView* ui_mainview_create(UIApp* app, lv_obj_t* parent) {
    UIMainView* mv = new UIMainView{};
    mv->app = app;

    lv_obj_t* cont = lv_obj_create(parent);
    mv->root = cont;

    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(cont, 16, 0);
    lv_obj_set_style_pad_gap(cont, 16, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    // Background matches app root (dark)
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(cont, lv_color_hex(0xFFFFFF), 0);

    // Title
    lv_obj_t* title = lv_label_create(cont);
    lv_label_set_text(title, "AT1053 Player");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(title, LV_PCT(100));

    // Spacer
    lv_obj_t* spacer = lv_obj_create(cont);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_size(spacer, LV_PCT(100), 10);

    // Stats button
    lv_obj_t* btn_stats = lv_btn_create(cont);
    mv->btn_stats = btn_stats;
    lv_obj_set_size(btn_stats, LV_PCT(100), 60);
    lv_obj_set_style_radius(btn_stats, 12, 0);
    lv_obj_set_style_bg_color(btn_stats, lv_color_hex(0x2A2A2A), 0);
    lv_obj_set_style_bg_opa(btn_stats, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn_stats, 0, 0);
    lv_obj_add_event_cb(btn_stats, on_stats, LV_EVENT_CLICKED, app);

    lv_obj_t* lbl_stats = lv_label_create(btn_stats);
    lv_label_set_text(lbl_stats, "Stats");
    lv_obj_set_style_text_font(lbl_stats, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl_stats);

    // Player button
    lv_obj_t* btn_player = lv_btn_create(cont);
    mv->btn_player = btn_player;
    lv_obj_set_size(btn_player, LV_PCT(100), 60);
    lv_obj_set_style_radius(btn_player, 12, 0);
    lv_obj_set_style_bg_color(btn_player, lv_color_hex(0x2A2A2A), 0);
    lv_obj_set_style_bg_opa(btn_player, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn_player, 0, 0);
    lv_obj_add_event_cb(btn_player, on_player, LV_EVENT_CLICKED, app);

    lv_obj_t* lbl_player = lv_label_create(btn_player);
    lv_label_set_text(lbl_player, "Player");
    lv_obj_set_style_text_font(lbl_player, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl_player);

    return mv;
}
