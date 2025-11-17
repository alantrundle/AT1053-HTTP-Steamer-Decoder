#pragma once
#include <lvgl.h>

// Forward declarations of per-view structs
struct UIMainView;
struct UIStatsView;
struct UIPlayerView;

// Global UI context
struct UIApp {
    lv_display_t* disp;     // active display
    lv_obj_t*     root;     // active screen

    UIMainView*   main_view;
    UIStatsView*  stats_view;
    UIPlayerView* player_view;
};

// Create UI and return global app context
UIApp* ui_init();

// Navigation
void ui_show_main(UIApp* ui);
void ui_show_stats(UIApp* ui);
void ui_show_player(UIApp* ui);

// Stats (StatsView) update functions – used from main.cpp
void ui_update_stats_bars(UIApp* ui, int net_pct, int pcm_pct);
void ui_update_stats_wifi(UIApp* ui, int wifi_status, const char* ip);
void ui_update_stats_decoder(UIApp* ui,
                             const char* codec,
                             int sample_rate_hz,
                             int channels,
                             int bitrate_kbps);
void ui_update_stats_outputs(UIApp* ui,
                             bool i2s_active,
                             bool a2dp_connected,
                             const char* a2dp_name);

// Player (PlayerView) update – ID3 / now playing text
void ui_update_player_id3(UIApp* ui,
                          bool has_meta,
                          const char* artist,
                          const char* title,
                          const char* album,
                          int track_num);
