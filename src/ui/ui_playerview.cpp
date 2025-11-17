#include "ui_playerview.h"
#include "ui_app.h"
#include <cstring>

// Back button handler
static void on_back(lv_event_t* e) {
    UIApp* app = static_cast<UIApp*>(lv_event_get_user_data(e));
    ui_show_main(app);
}

// Placeholder handlers for transport – no-op for now
static void on_prev(lv_event_t* e)  { (void)e; /* hook into your control bus later */ }
static void on_play(lv_event_t* e)  { (void)e; }
static void on_next(lv_event_t* e)  { (void)e; }
static void on_stop(lv_event_t* e)  { (void)e; }

UIPlayerView* ui_playerview_create(UIApp* app, lv_obj_t* parent) {
    UIPlayerView* pv = new UIPlayerView{};
    pv->app = app;

    lv_obj_t* root = lv_obj_create(parent);
    pv->root = root;

    lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(root, 12, 0);
    lv_obj_set_style_pad_gap(root, 10, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(root, lv_color_hex(0xFFFFFF), 0);

    // -------- Top bar (back + title) ----------
    lv_obj_t* top = lv_obj_create(root);
    lv_obj_remove_style_all(top);
    lv_obj_set_width(top, LV_PCT(100));
    lv_obj_set_style_pad_all(top, 0, 0);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(top, 8, 0);

    lv_obj_t* btn_back = lv_btn_create(top);
    lv_obj_set_style_radius(btn_back, 16, 0);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x2A2A2A), 0);
    lv_obj_set_style_bg_opa(btn_back, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn_back, 0, 0);
    lv_obj_set_size(btn_back, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_add_event_cb(btn_back, on_back, LV_EVENT_CLICKED, app);

    lv_obj_t* lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT " Library");
    lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_14, 0);

    lv_obj_t* lbl_title_bar = lv_label_create(top);
    lv_label_set_text(lbl_title_bar, "Now Playing");
    lv_obj_set_style_text_font(lbl_title_bar, &lv_font_montserrat_16, 0);
    lv_obj_set_flex_grow(lbl_title_bar, 1);
    lv_obj_set_style_text_align(lbl_title_bar, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t* spacer = lv_obj_create(top);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_size(spacer, 1, 1);

    // -------- Album art card ----------
    lv_obj_t* art_card = lv_obj_create(root);
    ui_apply_card_style(art_card);
    lv_obj_set_width(art_card, LV_PCT(100));
    lv_obj_set_flex_flow(art_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(art_card, 12, 0);

    // 200x200 placeholder centered
    lv_obj_t* art = lv_image_create(art_card);
    pv->art_placeholder = art;
    lv_obj_set_size(art, 200, 200);
    lv_obj_set_style_bg_color(art, lv_color_hex(0x202020), 0);
    lv_obj_set_style_bg_opa(art, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(art, 10, 0);
    lv_obj_set_style_border_width(art, 1, 0);
    lv_obj_set_style_border_color(art, lv_color_hex(0x404040), 0);
    // No src yet – will be a JPEG later
    lv_obj_center(art);

    // -------- Text / ID3 card ----------
    lv_obj_t* txt_card = lv_obj_create(root);
    ui_apply_card_style(txt_card);
    lv_obj_set_width(txt_card, LV_PCT(100));
    lv_obj_set_flex_flow(txt_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(txt_card, 4, 0);

    pv->lbl_title = lv_label_create(txt_card);
    lv_obj_set_style_text_font(pv->lbl_title, &lv_font_montserrat_18, 0);
    lv_label_set_text(pv->lbl_title, "Title");

    pv->lbl_artist = lv_label_create(txt_card);
    lv_obj_set_style_text_font(pv->lbl_artist, &lv_font_montserrat_14, 0);
    lv_label_set_text(pv->lbl_artist, "Artist");

    pv->lbl_album = lv_label_create(txt_card);
    lv_obj_set_style_text_font(pv->lbl_album, &lv_font_montserrat_14, 0);
    lv_label_set_text(pv->lbl_album, "Album");

    // -------- Position / time card ----------
    lv_obj_t* pos_card = lv_obj_create(root);
    ui_apply_card_style(pos_card);
    lv_obj_set_width(pos_card, LV_PCT(100));
    lv_obj_set_flex_flow(pos_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(pos_card, 4, 0);

    pv->slider_pos = lv_slider_create(pos_card);
    lv_obj_set_width(pv->slider_pos, LV_PCT(100));
    lv_slider_set_range(pv->slider_pos, 0, 100);
    lv_slider_set_value(pv->slider_pos, 0, LV_ANIM_OFF);

    pv->lbl_time = lv_label_create(pos_card);
    lv_obj_set_style_text_font(pv->lbl_time, &lv_font_montserrat_12, 0);
    lv_obj_set_width(pv->lbl_time, LV_PCT(100));
    lv_obj_set_style_text_align(pv->lbl_time, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(pv->lbl_time, "0:00 / 0:00");

    // -------- Transport row ----------
    lv_obj_t* row_tr = lv_obj_create(root);
    lv_obj_remove_style_all(row_tr);
    lv_obj_set_width(row_tr, LV_PCT(100));
    lv_obj_set_flex_flow(row_tr, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(row_tr, 12, 0);
    lv_obj_set_style_pad_all(row_tr, 0, 0);

    auto make_icon_button = [&](const char* txt,
                                lv_event_cb_t cb) -> lv_obj_t* {
        lv_obj_t* b = lv_btn_create(row_tr);
        lv_obj_set_style_radius(b, 20, 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(0x2A2A2A), 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(b, 0, 0);
        lv_obj_set_size(b, 64, 40);
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, app);

        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, txt);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
        lv_obj_center(l);
        return b;
    };

    pv->btn_prev = make_icon_button("⏮", on_prev);
    pv->btn_play = make_icon_button("▶︎", on_play);
    pv->btn_next = make_icon_button("⏭", on_next);
    pv->btn_stop = make_icon_button("⏹", on_stop);

    // -------- Volume card ----------
    lv_obj_t* vol_card = lv_obj_create(root);
    ui_apply_card_style(vol_card);
    lv_obj_set_width(vol_card, LV_PCT(100));
    lv_obj_set_flex_flow(vol_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(vol_card, 4, 0);

    lv_obj_t* lbl_vol_title = lv_label_create(vol_card);
    lv_label_set_text(lbl_vol_title, "Volume");
    lv_obj_set_style_text_font(lbl_vol_title, &lv_font_montserrat_14, 0);

    lv_obj_t* row_vol = lv_obj_create(vol_card);
    lv_obj_remove_style_all(row_vol);
    lv_obj_set_width(row_vol, LV_PCT(100));
    lv_obj_set_flex_flow(row_vol, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(row_vol, 8, 0);

    pv->slider_vol = lv_slider_create(row_vol);
    lv_slider_set_range(pv->slider_vol, 0, 100);
    lv_slider_set_value(pv->slider_vol, 50, LV_ANIM_OFF);
    lv_obj_set_flex_grow(pv->slider_vol, 1);

    pv->lbl_vol = lv_label_create(row_vol);
    lv_obj_set_style_text_font(pv->lbl_vol, &lv_font_montserrat_12, 0);
    lv_label_set_text(pv->lbl_vol, "50%");

    // You can later wire a callback to slider_vol to update real volume
    // and label text.

    // Start hidden (MainView is default)
    lv_obj_add_flag(root, LV_OBJ_FLAG_HIDDEN);
    return pv;
}
