#include <Arduino.h>
#include "bindings.h"
#include "ui.h"       // EEZ Studio generated header
#include "screens.h"  // for 'objects'

static uint8_t last_net = 255;
static uint8_t last_pcm = 255;
char buf[8];

static inline uint8_t clamp_pct(int v) {
    if (v < 0)   return 0;
    if (v > 100) return 100;
    return (uint8_t)v;
}

void ui_update_stats_bars(uint8_t net_pct, uint8_t pcm_pct)
{
    net_pct = clamp_pct(net_pct);
    pcm_pct = clamp_pct(pcm_pct);

    // NET bar ---------------------------------------------------------
    if (net_pct != last_net) {
        last_net = net_pct;
        if (objects.stats_bar_net) {
             
            snprintf(buf, sizeof(buf), "%u%%", net_pct);
            lv_bar_set_value(objects.stats_bar_net, net_pct, LV_ANIM_ON);
            lv_label_set_text(objects.stats_lbl_pct_net, buf);
        }
    }

    // PCM bar ---------------------------------------------------------
    if (pcm_pct != last_pcm) {
        last_pcm = pcm_pct;
        if (objects.stats_bar_pcm) {
            snprintf(buf, sizeof(buf), "%u%%", pcm_pct);
            lv_bar_set_value(objects.stats_bar_pcm, pcm_pct, LV_ANIM_ON);
            lv_label_set_text(objects.stats_lbl_pct_pcm, buf);
        }
    }
}

void ui_update_stats_decoder(const char* codec,
                             uint32_t samplerate,
                             uint8_t channels,
                             uint16_t kbps)
{
    static char last_codec[16] = "";
    static uint32_t last_sr = 0;
    static uint8_t last_ch = 0;
    static uint16_t last_kbps = 0;

    char buf[32];

    /* ---------------- CODEC NAME ---------------- */
    if (codec && strcmp(codec, last_codec) != 0) {
        strncpy(last_codec, codec, sizeof(last_codec));
        last_codec[sizeof(last_codec)-1] = '\0';

        if (objects.stats_lbl_codec) {
            lv_label_set_text(objects.stats_lbl_codec, last_codec);
        }
    }

    /* ---------------- SAMPLE RATE ---------------- */
    if (samplerate != last_sr) {
        last_sr = samplerate;

        if (objects.stats_lbl_samplerate) {
            snprintf(buf, sizeof(buf), "%lu Hz", (unsigned long)samplerate);
            lv_label_set_text(objects.stats_lbl_samplerate, buf);
        }
    }

    /* ---------------- CHANNELS ---------------- */
    if (channels != last_ch) {
        last_ch = channels;

        if (objects.stats_lbl_channels) {
            snprintf(buf, sizeof(buf), "%u ch", channels);
            lv_label_set_text(objects.stats_lbl_channels, buf);
        }
    }

    /* ---------------- BITRATE ---------------- */
    if (kbps != last_kbps) {
        last_kbps = kbps;

        if (objects.stats_lbl_bitrate) {
            snprintf(buf, sizeof(buf), "%u kbps", kbps);
            lv_label_set_text(objects.stats_lbl_bitrate, buf);
        }
    }
}

void ui_update_stats_outputs(bool i2s_on,
                             bool a2dp_on,
                             const char* a2dp_name)
{
    static bool last_i2s_on  = 0;
    static bool last_a2dp_on = 0;
    static char last_name[32] = "";

    char buf[32];

    /* ---------------- I2S OUTPUT STATE ---------------- */
    if (i2s_on != last_i2s_on) {
        last_i2s_on = i2s_on;

        if (objects.stats_lbl_i2s_on) {
            lv_label_set_text(objects.stats_lbl_i2s_on,
                              i2s_on ? "ON" : "OFF");
        }
    }

    /* ---------------- A2DP OUTPUT STATE ---------------- */
    if (a2dp_on != last_a2dp_on) {
        last_a2dp_on = a2dp_on;

        if (objects.stats_lbl_a2dp_on) {
            lv_label_set_text(objects.stats_lbl_a2dp_on,
                              a2dp_on ? "ON" : "OFF");
        }
    }

    /* ---------------- A2DP DEVICE NAME ---------------- */
    if (a2dp_name && strcmp(a2dp_name, last_name) != 0) {
        strncpy(last_name, a2dp_name, sizeof(last_name));
        last_name[sizeof(last_name)-1] = '\0';

        if (objects.stats_lbl_a2dp_name) {
            if (a2dp_on)
                lv_label_set_text(objects.stats_lbl_a2dp_name, last_name);
            else
                lv_label_set_text(objects.stats_lbl_a2dp_name, "—");
        }
    }
}

void ui_update_stats_wifi(bool connected,
                          const char* ssid,
                          const char* ip)
{
    static bool  last_connected = false;
    static char  last_ssid[32]  = "";
    static char  last_ip[32]    = "";

    /* ---------------- WIFI STATE ---------------- */
    if (connected != last_connected) {
        last_connected = connected;

        if (objects.stats_lbl_wifi_on) {
            lv_label_set_text(objects.stats_lbl_wifi_on,
                              connected ? "Connected"
                                        : "Disconnected");
        }
    }

    /* ---------------- SSID / AP NAME ---------------- */
    const char* ssid_safe = (ssid && *ssid) ? ssid : "—";

    if (strcmp(ssid_safe, last_ssid) != 0) {
        strncpy(last_ssid, ssid_safe, sizeof(last_ssid));
        last_ssid[sizeof(last_ssid)-1] = '\0';

        if (objects.stats_lbl_wifi_ssid) {
            lv_label_set_text(objects.stats_lbl_wifi_ssid, last_ssid);
        }
    }

    /* ---------------- IP ADDRESS ---------------- */
    const char* ip_safe = (connected && ip && *ip) ? ip : "0.0.0.0";

    if (strcmp(ip_safe, last_ip) != 0) {
        strncpy(last_ip, ip_safe, sizeof(last_ip));
        last_ip[sizeof(last_ip)-1] = '\0';

        if (objects.stats_lbl_wifi_ipaddress) {
            lv_label_set_text(objects.stats_lbl_wifi_ipaddress, last_ip);
        }
    }
}






