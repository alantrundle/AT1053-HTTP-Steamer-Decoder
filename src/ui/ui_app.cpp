#include "ui_app.h"
#include "ui_mainview.h"
#include "ui_statsview.h"
#include "ui_playerview.h"
#include <cstring>

// ---------------- Simple global dark style helpers ----------------

static void apply_dark_root_style(lv_obj_t* root) {
    if (!root) return;

    lv_color_t bg = lv_color_hex(0x000000);
    lv_color_t fg = lv_color_hex(0xFFFFFF);

    lv_obj_set_style_bg_color(root, bg, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(root, fg, 0);
}

static void apply_card_style(lv_obj_t* card) {
    if (!card) return;
    lv_color_t bg = lv_color_hex(0x101010);

    lv_obj_set_style_bg_color(card, bg, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x303030), 0);
}

// Expose for views
void ui_apply_card_style(lv_obj_t* card) {
    apply_card_style(card);
}

// -------------------- Navigation helpers --------------------------

static void hide_all_views(UIApp* ui) {
    if (!ui) return;
    if (ui->main_view && ui->main_view->root)
        lv_obj_add_flag(ui->main_view->root, LV_OBJ_FLAG_HIDDEN);
    if (ui->stats_view && ui->stats_view->root)
        lv_obj_add_flag(ui->stats_view->root, LV_OBJ_FLAG_HIDDEN);
    if (ui->player_view && ui->player_view->root)
        lv_obj_add_flag(ui->player_view->root, LV_OBJ_FLAG_HIDDEN);
}

void ui_show_main(UIApp* ui) {
    if (!ui || !ui->main_view || !ui->main_view->root) return;
    hide_all_views(ui);
    lv_obj_clear_flag(ui->main_view->root, LV_OBJ_FLAG_HIDDEN);
}

void ui_show_stats(UIApp* ui) {
    if (!ui || !ui->stats_view || !ui->stats_view->root) return;
    hide_all_views(ui);
    lv_obj_clear_flag(ui->stats_view->root, LV_OBJ_FLAG_HIDDEN);
}

void ui_show_player(UIApp* ui) {
    if (!ui || !ui->player_view || !ui->player_view->root) return;
    hide_all_views(ui);
    lv_obj_clear_flag(ui->player_view->root, LV_OBJ_FLAG_HIDDEN);
}

// ------------------------ UI init ---------------------------------

UIApp* ui_init() {
    UIApp* ui = new UIApp{};
    ui->disp = lv_display_get_default();
    ui->root = lv_screen_active();   // current screen

    apply_dark_root_style(ui->root);

    // Create the three views on the same screen
    ui->main_view   = ui_mainview_create(ui, ui->root);
    ui->stats_view  = ui_statsview_create(ui, ui->root);
    ui->player_view = ui_playerview_create(ui, ui->root);

    // Start on main view
    ui_show_main(ui);

    return ui;
}

// ---------------- Stats update functions --------------------------

static int clamp_pct(int v) {
    if (v < 0)   return 0;
    if (v > 100) return 100;
    return v;
}

void ui_update_stats_bars(UIApp* ui, int net_pct, int pcm_pct) {
    if (!ui || !ui->stats_view) return;
    net_pct = clamp_pct(net_pct);
    pcm_pct = clamp_pct(pcm_pct);

    lv_bar_set_value(ui->stats_view->bar_net, net_pct, LV_ANIM_OFF);
    lv_bar_set_value(ui->stats_view->bar_pcm, pcm_pct, LV_ANIM_OFF);

    char buf[8];
    lv_snprintf(buf, sizeof(buf), "%d%%", net_pct);
    lv_label_set_text(ui->stats_view->lbl_net_val, buf);
    lv_snprintf(buf, sizeof(buf), "%d%%", pcm_pct);
    lv_label_set_text(ui->stats_view->lbl_pcm_val, buf);
}

void ui_update_stats_wifi(UIApp* ui, int wifi_status, const char* ip) {
    if (!ui || !ui->stats_view) return;

    // Don't depend on WL_CONNECTED macro here
    bool connected = (wifi_status != 0);
    lv_label_set_text(ui->stats_view->lbl_wifi_status,
                      connected ? "Wi-Fi: Connected" : "Wi-Fi: Not connected");

    char buf[64];
    const char* shown_ip = (connected && ip && *ip) ? ip : "-";
    lv_snprintf(buf, sizeof(buf), "IP: %s", shown_ip);
    lv_label_set_text(ui->stats_view->lbl_wifi_ip, buf);
}

void ui_update_stats_decoder(UIApp* ui,
                             const char* codec,
                             int sample_rate_hz,
                             int channels,
                             int bitrate_kbps) {
    if (!ui || !ui->stats_view) return;

    char buf[64];

    lv_snprintf(buf, sizeof(buf), "Codec: %s",
                (codec && *codec) ? codec : "-");
    lv_label_set_text(ui->stats_view->lbl_dec_codec, buf);

    if (sample_rate_hz > 0)
        lv_snprintf(buf, sizeof(buf), "Sample rate: %d Hz", sample_rate_hz);
    else
        lv_snprintf(buf, sizeof(buf), "Sample rate: -");
    lv_label_set_text(ui->stats_view->lbl_dec_sr, buf);

    if (channels > 0)
        lv_snprintf(buf, sizeof(buf), "Channels: %d", channels);
    else
        lv_snprintf(buf, sizeof(buf), "Channels: -");
    lv_label_set_text(ui->stats_view->lbl_dec_ch, buf);

    if (bitrate_kbps > 0)
        lv_snprintf(buf, sizeof(buf), "Bitrate: %d kbps", bitrate_kbps);
    else
        lv_snprintf(buf, sizeof(buf), "Bitrate: -");
    lv_label_set_text(ui->stats_view->lbl_dec_br, buf);
}

void ui_update_stats_outputs(UIApp* ui,
                             bool i2s_active,
                             bool a2dp_connected,
                             const char* a2dp_name) {
    if (!ui || !ui->stats_view) return;

    lv_label_set_text(ui->stats_view->lbl_out_i2s,
                      i2s_active ? "I2S: On" : "I2S: Off");

    char buf[64];
    if (a2dp_connected) {
        const char* nm = (a2dp_name && *a2dp_name) ? a2dp_name : "(sink)";
        lv_snprintf(buf, sizeof(buf), "A2DP: On (%s)", nm);
    } else {
        lv_snprintf(buf, sizeof(buf), "A2DP: Off");
    }
    lv_label_set_text(ui->stats_view->lbl_out_a2dp, buf);
}

// ---------------- Player update (ID3) -----------------------------

void ui_update_player_id3(UIApp* ui,
                          bool has_meta,
                          const char* artist,
                          const char* title,
                          const char* album,
                          int track_num) {
    if (!ui || !ui->player_view) return;

    char buf[128];

    // Title
    if (title && *title)
        lv_label_set_text(ui->player_view->lbl_title, title);
    else if (!has_meta)
        lv_label_set_text(ui->player_view->lbl_title, "No metadata");
    else
        lv_label_set_text(ui->player_view->lbl_title, "-");

    // Artist
    if (artist && *artist) {
        lv_snprintf(buf, sizeof(buf), "%s", artist);
        lv_label_set_text(ui->player_view->lbl_artist, buf);
    } else {
        lv_label_set_text(ui->player_view->lbl_artist, "-");
    }

    // Album (+ optional track number)
    if (album && *album) {
        if (track_num > 0) {
            lv_snprintf(buf, sizeof(buf), "Track %d \u2022 %s", track_num, album);
        } else {
            lv_snprintf(buf, sizeof(buf), "%s", album);
        }
        lv_label_set_text(ui->player_view->lbl_album, buf);
    } else if (track_num > 0) {
        lv_snprintf(buf, sizeof(buf), "Track %d", track_num);
        lv_label_set_text(ui->player_view->lbl_album, buf);
    } else {
        lv_label_set_text(ui->player_view->lbl_album, "-");
    }
}
