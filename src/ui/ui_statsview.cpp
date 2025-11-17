#include "ui_statsview.h"
#include "ui_app.h"

// Back button handler
static void on_back(lv_event_t* e) {
    UIApp* app = static_cast<UIApp*>(lv_event_get_user_data(e));
    ui_show_main(app);
}

static lv_obj_t* make_hbar(lv_obj_t* parent,
                           const char* label_text,
                           lv_obj_t** out_bar,
                           lv_obj_t** out_val) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_style_pad_all(row, 2, 0);
    lv_obj_set_style_pad_gap(row, 6, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl = lv_label_create(row);
    lv_label_set_text(lbl, label_text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_min_width(lbl, 40, 0);

    lv_obj_t* bar = lv_bar_create(row);
    lv_obj_set_flex_grow(bar, 1);
    lv_obj_set_height(bar, 12);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);

    lv_obj_t* val = lv_label_create(row);
    lv_label_set_text(val, "0%");
    lv_obj_set_style_text_font(val, &lv_font_montserrat_12, 0);
    lv_obj_set_style_min_width(val, 32, 0);
    lv_obj_set_style_text_align(val, LV_TEXT_ALIGN_RIGHT, 0);

    if (out_bar) *out_bar = bar;
    if (out_val) *out_val = val;
    return row;
}

UIStatsView* ui_statsview_create(UIApp* app, lv_obj_t* parent) {
    UIStatsView* sv = new UIStatsView{};
    sv->app = app;

    lv_obj_t* root = lv_obj_create(parent);
    sv->root = root;

    lv_obj_set_size(root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(root, 12, 0);
    lv_obj_set_style_pad_gap(root, 12, 0);
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

    // Back button
    lv_obj_t* btn_back = lv_btn_create(top);
    lv_obj_set_style_radius(btn_back, 16, 0);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x2A2A2A), 0);
    lv_obj_set_style_bg_opa(btn_back, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn_back, 0, 0);
    lv_obj_set_size(btn_back, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_add_event_cb(btn_back, on_back, LV_EVENT_CLICKED, app);

    lv_obj_t* lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT " Main");
    lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_14, 0);

    // Title
    lv_obj_t* lbl_title = lv_label_create(top);
    lv_label_set_text(lbl_title, "Stats");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_16, 0);
    lv_obj_set_flex_grow(lbl_title, 1);
    lv_obj_set_style_text_align(lbl_title, LV_TEXT_ALIGN_CENTER, 0);

    // Spacer to balance flex
    lv_obj_t* spacer = lv_obj_create(top);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_size(spacer, 1, 1);

    // -------- Buffers card ----------
    lv_obj_t* card_buf = lv_obj_create(root);
    ui_apply_card_style(card_buf);
    lv_obj_set_width(card_buf, LV_PCT(100));
    lv_obj_set_style_pad_gap(card_buf, 6, 0);
    lv_obj_set_flex_flow(card_buf, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* lbl_buf = lv_label_create(card_buf);
    lv_label_set_text(lbl_buf, "Buffers");
    lv_obj_set_style_text_font(lbl_buf, &lv_font_montserrat_14, 0);

    make_hbar(card_buf, "NET", &sv->bar_net, &sv->lbl_net_val);
    make_hbar(card_buf, "PCM", &sv->bar_pcm, &sv->lbl_pcm_val);

    // -------- Wi-Fi card ----------
    lv_obj_t* card_wifi = lv_obj_create(root);
    ui_apply_card_style(card_wifi);
    lv_obj_set_width(card_wifi, LV_PCT(100));
    lv_obj_set_flex_flow(card_wifi, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(card_wifi, 4, 0);

    lv_obj_t* lbl_wifi = lv_label_create(card_wifi);
    lv_label_set_text(lbl_wifi, "Wi-Fi");
    lv_obj_set_style_text_font(lbl_wifi, &lv_font_montserrat_14, 0);

    sv->lbl_wifi_status = lv_label_create(card_wifi);
    lv_obj_set_style_text_font(sv->lbl_wifi_status, &lv_font_montserrat_12, 0);
    lv_label_set_text(sv->lbl_wifi_status, "Wi-Fi: Not connected");

    sv->lbl_wifi_ip = lv_label_create(card_wifi);
    lv_obj_set_style_text_font(sv->lbl_wifi_ip, &lv_font_montserrat_12, 0);
    lv_label_set_text(sv->lbl_wifi_ip, "IP: -");

    // -------- Decoder card ----------
    lv_obj_t* card_dec = lv_obj_create(root);
    ui_apply_card_style(card_dec);
    lv_obj_set_width(card_dec, LV_PCT(100));
    lv_obj_set_flex_flow(card_dec, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(card_dec, 2, 0);

    lv_obj_t* lbl_dec = lv_label_create(card_dec);
    lv_label_set_text(lbl_dec, "Decoder");
    lv_obj_set_style_text_font(lbl_dec, &lv_font_montserrat_14, 0);

    sv->lbl_dec_codec = lv_label_create(card_dec);
    sv->lbl_dec_sr    = lv_label_create(card_dec);
    sv->lbl_dec_ch    = lv_label_create(card_dec);
    sv->lbl_dec_br    = lv_label_create(card_dec);

    lv_obj_set_style_text_font(sv->lbl_dec_codec, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_font(sv->lbl_dec_sr,    &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_font(sv->lbl_dec_ch,    &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_font(sv->lbl_dec_br,    &lv_font_montserrat_12, 0);

    lv_label_set_text(sv->lbl_dec_codec, "Codec: -");
    lv_label_set_text(sv->lbl_dec_sr,    "Sample rate: -");
    lv_label_set_text(sv->lbl_dec_ch,    "Channels: -");
    lv_label_set_text(sv->lbl_dec_br,    "Bitrate: -");

    // -------- Outputs card ----------
    lv_obj_t* card_out = lv_obj_create(root);
    ui_apply_card_style(card_out);
    lv_obj_set_width(card_out, LV_PCT(100));
    lv_obj_set_flex_flow(card_out, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(card_out, 2, 0);

    lv_obj_t* lbl_out = lv_label_create(card_out);
    lv_label_set_text(lbl_out, "Outputs");
    lv_obj_set_style_text_font(lbl_out, &lv_font_montserrat_14, 0);

    sv->lbl_out_i2s  = lv_label_create(card_out);
    sv->lbl_out_a2dp = lv_label_create(card_out);
    lv_obj_set_style_text_font(sv->lbl_out_i2s,  &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_font(sv->lbl_out_a2dp, &lv_font_montserrat_12, 0);
    lv_label_set_text(sv->lbl_out_i2s,  "I2S: Off");
    lv_label_set_text(sv->lbl_out_a2dp, "A2DP: Off");

    // Start hidden (MainView is default)
    lv_obj_add_flag(root, LV_OBJ_FLAG_HIDDEN);
    return sv;
}
