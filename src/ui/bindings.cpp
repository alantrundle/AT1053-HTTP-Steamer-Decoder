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




