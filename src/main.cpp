// AT1053 Audio Streamer - Optimized MP3 Decoder Firmware
// Written by Alan Trundle - Version 3.2 optimized
// LGVL Support Added - Running almost entierly in PSRAM
// ESP32-WROVER (8MB PSRAM), with A2DP + I2S output

// AAC support added - working
// AAC + MP3 supported

// Dont forget to put lv_conf.h in AT1053 HTTP Steamer Decoder\.pio\libdeps\esp32dev\lvgl\

// Recommend ESP32 WROVER with 8MB PSRAM with TF Card
// https://www.aliexpress.com/item/32879370278.html?spm=a2g0o.order_list.order_list_main.151.73d21802kujr90
//
// Recommend 4" TFT Capacitive Touch
// https://www.aliexpress.com/item/1005006698127763.html?spm=a2g0o.order_list.order_list_main.106.73d21802kujr90
//
// Recommend PCM5102 DAC (I2S with Stereo RCA and headphone/lineout jack outputs)
// https://www.aliexpress.com/item/1005005352684938.html?spm=a2g0o.order_list.order_list_main.136.73d21802kujr90

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>  // add near other C headers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <vector>
#include <algorithm>
#include <Wire.h>
#include <SPI.h>

#include "esp_idf_version.h"
#include "freertos/FreeRTOS.h"  // needed for ESP Arduino < 2.0
#include "freertos/FreeRTOSConfig.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 2, 0)
#include "freertos/xtensa_api.h"
#else
#include "xtensa_api.h"
#endif
#include "driver/spi_slave.h"
#include "driver/gpio.h"
#include "headerDetection.h"
#include "idv3Parser.h"
#include "MP3DecoderHelix.h"
#include "AACDecoderHelix.h"      // <-- ADD
#include "libhelix-mp3/mp3dec.h"  // Make sure this is included somewhere

#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "esp_heap_caps.h"
#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_spp_api.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_wifi.h"

#ifdef ARDUINO_ARCH_ESP32
#include "esp32-hal-bt.h"
#include "esp32-hal-log.h"
#else
#include "esp_log.h"
#endif

#include "driver/i2s.h"

// TFT inludes
#include <lvgl.h>
#include "gfxDriver.h"   // provides: extern LGFX_ST7796 display;
#include "ui/ui.h"        // your UI (bars + keyboard)
#include "ui/bindings.h"

// Add once at the top of the file:
#include "driver/periph_ctrl.h"  // periph_module_disable/enable on ESP32

// WiFi Stuff
#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_coexist.h"

// Optional: if your IDF has periph_module_reset()
#ifndef HAVE_PERIPH_MODULE_RESET
#define HAVE_PERIPH_MODULE_RESET 0
#endif

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#ifndef EXT_RAM_ATTR
#define EXT_RAM_ATTR __attribute__((section(".ext_ram.bss")))
#endif

using namespace libhelix;

#define I2S_BCK 14
#define I2S_WS 12
#define I2S_DATA 13

//LVGL
#ifndef TFT_HOR_RES
#define TFT_HOR_RES 480
#endif
#ifndef TFT_VER_RES
#define TFT_VER_RES 320
#endif
#ifndef PANEL_ROTATION
#define PANEL_ROTATION 3   // 0..3 depending on your mounting
#endif
//LVGL

TaskHandle_t httpTaskHandle = nullptr;
TaskHandle_t decodeTaskHandle = NULL;
TaskHandle_t i2sTaskHandle = NULL;
TaskHandle_t lvglDisplayUpdateHandle = NULL;

// --- Wi-Fi + Stream config ---
static const char* WIFI_SSID = "WyseNet";
static const char* WIFI_PASS = "cdf45424e4";

// URL Vars
// Supports AAC and MP3 at the moment.
// Ogg Vorbis, better AAC support, and WMA support coming.
// HTTP files - restriced access from 81.2.125.96/28 ONLY
// I host my test audio files on a local LAMP server, which i have resricted access to due to legistlation.
/*
const char* urls[] = {
  "http://81.2.125.100/codec_board/sample4.aac",
  "http://81.2.125.100/codec_board/Maximum_Lush-You_I_-_Alex_K_Remix-All_Around_The_World.aac",
  "http://81.2.125.100/codec_board/Wham!/01-Wham%20Rap!%20(Enjoy%20What%20You%20Do_).mp3",
  "http://81.2.125.100/codec_board/sample2.aac",
  "http://81.2.125.100/codec_board/Wham!/02-I'm%20Your%20Man.mp3",
  "http://81.2.125.100/codec_board/Wham!/02-Young%20Guns%20(Go%20For%20It).mp3",
  "http://81.2.125.100/codec_board/Wham!/03-Bad%20Boys.mp3",
  "http://81.2.125.100/codec_board/Wham!/05-Freedom.mp3",
  "http://81.2.125.100/codec_board/Wham!/08-The%20Edge%20of%20Heaven.mp3",
  "http://81.2.125.100/codec_board/Wham!/12-Battlestations.mp3",
  "http://81.2.125.100/codec_board/Wham!/06-If%20You%20Were%20There.mp3",
  "http://81.2.125.100/codec_board/Wham!/02-Everything%20She%20Wants.mp3",
  "http://81.2.125.100/codec_board/Wham!/06-Careless%20Whisper.mp3",
  "http://81.2.125.100/codec_board/Wham!/04-Club%20Tropicana.mp3"
};
*/

const char* urls[] = {
  "http://81.2.125.100/codec_board/01%20-%20Sultans%20Of%20Swing.mp3",
  "http://81.2.125.100/codec_board/10-Spin%20Doctors%20-%20Two%20princes.mp3",
  "http://81.2.125.100/codec_board/10-Sweet%20Little%20Mystery.mp3",
  "http://81.2.125.100/codec_board/09-Private%20investigations.mp3",
  "http://81.2.125.100/codec_board/Cheek%20to%20Cheek/15-Lady%20In%20Red.mp3",
  "http://81.2.125.100/codec_board/Cheek%20to%20Cheek/01-I%20Want%20To%20Know%20What%20Love%20Is.mp3",
  "http://81.2.125.100/codec_board/Cheek%20to%20Cheek/12-Baby%20I%20Love%20Your%20Way%20_%20Freebird.mp3",
  "http://81.2.125.100/codec_board/09%20-%20Without%20Me.mp3",
  "http://81.2.125.100/codec_board/Steel%20Bars.mp3",
  "http://81.2.125.100/codec_board/sample4.aac"
};

// Number of tracks in the playlist
const int playlist_count = sizeof(urls) / sizeof(urls[0]);

// Current track index (0-based)
volatile int current_track_index = 0;

// Session == current track index (for tagging slots)
volatile uint32_t g_play_session = 0;

// Manual “skip now” trigger that httpFillTask will observe
volatile bool g_force_next = false;

// Optional: slot metadata (if you rolled back and removed them, re-add)
enum SlotCodec : uint8_t { SLOT_UNKNOWN = 0, SLOT_MP3 = 1, SLOT_AAC = 2 };

// pause in between tracks
int track_pause_length = 1; // seconds (set to 0 to disable)

// --- HTTP ring buffer in PSRAM - 256 × 1024 (256KB) ---
#define MAX_CHUNK_SIZE 1024
#define NUM_BUFFERS 512

EXT_RAM_ATTR  static uint8_t* net_pool = nullptr;                     // PSRAM pool
EXT_RAM_ATTR static uint8_t* netBuffers[NUM_BUFFERS] = { nullptr };  // slot pointers
volatile bool netBufFilled[NUM_BUFFERS] = { false };
volatile int netWrite = 0;
volatile int netRead = 0;

// These must be sized to NUM_BUFFERS elsewhere with your other net* arrays
uint16_t netSize[NUM_BUFFERS]   = {0};
uint8_t  netTag[NUM_BUFFERS]    = {0};
uint32_t netOffset[NUM_BUFFERS]   = {0};
uint32_t netSess[NUM_BUFFERS]   = {0};

// PCM Buffer also in PSRAM. Shared buffer for A2DP + I2S outputs.
// I2S Off by default. Turn it on using AT+I2S=ON
// Sink scan: AT+BTSCAN
// Sink Connect: AT+BTCONNECTI=<index>
#define PCM_BUFFER_SIZE 256                       // KB
#define A2DP_BUFFER_SIZE (1024 * PCM_BUFFER_SIZE) //PCM buffer
EXT_RAM_ATTR  static uint8_t* a2dp_buffer = nullptr;
static volatile size_t a2dp_write_index = 0, a2dp_read_index = 0, a2dp_buffer_fill = 0, a2dp_read_index_i2s = 0;
portMUX_TYPE a2dp_mux = portMUX_INITIALIZER_UNLOCKED;

//ID3 Detection
static ID3v2Meta id3m{};

// A2DP Name lookup - GAP 
static bool name_lookup_active = false;
static esp_bd_addr_t name_lookup_bda = {0};
volatile bool a2dp_connected = false;
char a2dp_connected_name[64];
esp_bd_addr_t a2dp_connected_bda;
// End

volatile bool a2dp_audio_ready = false;
volatile bool decoder_paused = false;
volatile bool output_ready = false;

volatile bool i2s_output = false;  // set to enable i2s by default

volatile bool stream_running = false; // HTTP stream started/ stopped (EOF)

// keep timestamp global so it persists between loop() calls
static uint32_t lastStackDump = 0;

// setup - bluetooth stack init
static bool g_bt_profiles_registered = false;

// --- Radio poll state (used by loop() logger) ---

volatile uint32_t g_lastNetWriteMs = 0;    // set each time httpFill publishes a slot
volatile uint32_t g_httpBytesTick  = 0;    // add 'wrote' to this when you publish a slot

volatile bool     g_a2dpConnected  = false; // set true/false in your A2DP events
volatile uint32_t g_lastA2dpPushMs = 0;     // set at end of your A2DP PCM callback

// Cold-prime mode: fill PCM even when no outputs are active
static volatile bool cold_prime_mode = true;  // true after reset until any output goes active
static volatile size_t cold_read_index = 0;   // virtual "slowest reader" while priming

// Eq Vars
struct Biquad {
  float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
  float z1L = 0, z2L = 0, z1R = 0, z2R = 0;  // per-channel states (stereo)
};

const int EQ_BANDS = 10;
const float kEqFreqs[EQ_BANDS] = { 31.25f, 62.5f, 125.f, 250.f, 500.f, 1000.f, 2000.f, 4000.f, 8000.f, 16000.f }; //freq
volatile bool g_eq_enabled = false;
float g_eq_gain_db[EQ_BANDS] = { 0 };

// >>> ADD THESE THREE LINES HERE <<<
EXT_RAM_ATTR Biquad g_eq[EQ_BANDS];
int g_eq_sr = 44100;
portMUX_TYPE g_eq_mux = portMUX_INITIALIZER_UNLOCKED;
// End Eq

// === Hard-pause state (move this block ABOVE updateDREQ) ===
enum PauseState : uint8_t { PAUSE_RUNNING = 0,
                            PAUSE_ARMED = 1,
                            PAUSE_HELD = 2 };
static volatile PauseState g_pause = PAUSE_RUNNING;

// SPI checksum status
enum : uint8_t {
  SLOT_EMPTY = 0,  // not filled / available for DMA
  SLOT_READY = 1,  // checksum OK, safe for decoder to read
  SLOT_RETRY = 2   // checksum fail; re-queued to be overwritten by master
};

static volatile uint8_t slot_state[NUM_BUFFERS] = { 0 };

typedef struct {
  uint16_t samplerate;
  uint8_t channels;
  uint16_t kbps;
  uint8_t layer;
} MP3StatusInfo;

volatile MP3StatusInfo currentMP3Info = { 0 };

static inline void resetMP3Info(volatile MP3StatusInfo& s) {
  s.samplerate = 0;
  s.channels = 0;
  s.kbps = 0;
  s.layer = 0;
}

// --- Playlist control (sequential, wrap to 0) ---
static constexpr int URL_COUNT = sizeof(urls) / sizeof(urls[0]);
static volatile int g_url_index = 0;

static inline void next_url() {
  g_url_index = (g_url_index + 1) % URL_COUNT;
}

// When we hop to a new track, ask decoder to relock headers cleanly
static volatile bool g_new_track_pending = false;
// End URL vars

// --- Global codec indicator for OLED ---
enum CodecKind : uint8_t { CODEC_UNKNOWN = 0,
                           CODEC_MP3 = 1,
                           CODEC_AAC = 2 };

volatile CodecKind feed_codec = CODEC_UNKNOWN;

// ---- SPI clock activity monitor ----
volatile uint32_t g_spi_setup_count = 0;  // increments each time a master transaction begins
volatile uint32_t g_spi_done_count = 0;   // increments each time a transaction completes
// We don’t call millis() inside ISRs; we count edges and time them in the task.

// ─────────────────────────────────────────────────────────────────────
//                    A2DP device picker (scan → menu → connect)
// ─────────────────────────────────────────────────────────────────────
enum UiState { UI_IDLE,
               UI_SCANNING,
               UI_MENU,
               UI_CONNECTING,
               UI_STREAMING };
static volatile UiState ui_state = UI_IDLE;

struct BTDevice {
  char name[25];
  esp_bd_addr_t addr;  // 6 bytes
  int rssi;

  // de-dup / rate limit
  int last_emit_rssi = -127;     // dBm (init to very low)
  uint32_t last_emit_ms = 0;     // last time we printed this device
  bool ever_emitted = false;     // first sighting gets printed
};

#define MAX_SCAN_ENTRIES        16
#define EMIT_MIN_DELTA_DB       6     // re-emit only if RSSI improves by >= 6 dB
#define EMIT_MIN_INTERVAL_MS    3000  // ...and at least 3 seconds since last emit

static BTDevice g_devs[MAX_SCAN_ENTRIES];
static int g_dev_count = 0;

static const int MAX_DEV = 16;
static BTDevice found[MAX_DEV];
static int found_count = 0;

static esp_bd_addr_t target_addr = { 0 };

// Connect timeout deadline (0 = not connecting)
static uint32_t connect_deadline_ms = 0;

// --- Raw GAP scan state ---
static volatile bool g_gap_discovery_active = false;

// Simple status for a direct MAC connect
enum BtConnectionStatus {
  BT_CONNECT_SUCCESS = 0,
  BT_CONNECT_FAIL_REQUEST = -1,
  BT_CONNECT_NULL_MAC = -2
};

// end BT

static bool a2dp_started = false;       // we start A2DP once and keep it up
static bool gap_cb_registered = false;  // register GAP callback once

// Namespace keys
static const char* NVS_NS_BT = "a2dp";
static const char* KEY_LAST_BDA = "last_bda";
static const char* KEY_AUTORC = "auto_rc";

// ───────── Auto-reconnect globals (add once) ─────────
static bool g_auto_reconnect_enabled = true;  // persisted via NVS; default ON
static bool g_have_last_bda = false;
static uint8_t g_last_bda[6] = { 0 };
static TaskHandle_t g_autorc_task = nullptr;

// ─── AutoRC pacing ──────────────────────────────────────────────
static volatile bool g_connect_inflight = false;
static uint32_t g_next_reconnect_ms = 0;
static const uint32_t AUTORC_MIN_COOLDOWN_MS = 3000;  // 5s floor between attempts

// Wifi Information
struct WiFiStatus {
  bool connected = false;
  String ip;
  String gateway;
  String subnet;
  String ssid;
};

WiFiStatus wifi_status;

#undef TASK_NEVER_RETURN
#define TASK_NEVER_RETURN() \
  for (;;) vTaskDelay(portMAX_DELAY)

// LVGL display + draw buffers
static lv_display_t *lv_disp = nullptr;
static lv_color_t *lvbuf1 = nullptr;
static lv_color_t *lvbuf2 = nullptr;
// End LVGL

// ---- forward decls ----
static void stopAllDiscovery();
static void startRawScan(uint32_t seconds = 10);
static bool ensure_bt_ready();
static void clear_found();
static int add_or_update_device(const char* name, const esp_bd_addr_t addr, int rssi);
static bool a2dp_try_connect_last(uint32_t timeout_ms = 6000);
static void a2dp_start_autorc_task();
static bool nvs_load_autorc(bool* out);
static bool nvs_load_last_bda(uint8_t out[6]);
static bool nvs_save_last_bda(const uint8_t bda[6]);
static void nvs_save_autorc(bool en);
static void nvs_forget_last_bda();

static void eq_reset_all();

static void eq_set_gain_freq(float freq, float dB);

static int pcm_buffer_percent();
static inline int net_filled_slots();

void lvglUpdateTask(void* /*param*/);

bool player_next();

void clearAudioBuffers();

static void connectToMenuIndex(int oneBased);
static bool parse_mac(const char* s, uint8_t out[6]);
static void onA2DPConnectState(esp_a2d_connection_state_t state, void*);
static void onA2DPAudioState(esp_a2d_audio_state_t state, void*);
static BtConnectionStatus connect_direct_mac(const uint8_t* mac);
int startI2S();
int stopI2S();

MP3DecoderHelix mp3;
AACDecoderHelix aac;


// ─────────────────────────────────────────────────────────────────────
//                       TFT LVGL Stuff
// ─────────────────────────────────────────────────────────────────────
const char* codec_name_from_enum(CodecKind c) {
  switch (c) {
    case CODEC_MP3:  return "MP3";
    case CODEC_AAC:  return "AAC";
    case CODEC_UNKNOWN:
    default:         return "NONE";
  }
}

// Millis as LVGL tick source
static uint32_t my_tick(void) {
  return (uint32_t)millis();
}

// LVGL -> panel flush
static void my_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  (void)disp;
  const uint32_t w = (uint32_t)(area->x2 - area->x1 + 1);
  const uint32_t h = (uint32_t)(area->y2 - area->y1 + 1);

  display.startWrite();
  display.setAddrWindow(area->x1, area->y1, w, h);
  display.writePixels((lgfx::rgb565_t*)px_map, w * h);  // px_map is RGB565
  display.endWrite();

  lv_display_flush_ready(disp);
}

// LVGL touch read -> use driver’s readTouch()
static void my_touch_read(lv_indev_t *indev, lv_indev_data_t *data) {
  (void)indev;
  if (!data) return;

  uint16_t x=0, y=0; bool pressed=false;
  bool ok = display.readTouch(x, y, pressed);
  data->state   = (ok && pressed) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
  if (ok && pressed) {
    data->point.x = (lv_coord_t)x;
    data->point.y = (lv_coord_t)y;
  }
}

void setupTFT() {

  // --- Display init ---
  display.init();
  display.setRotation(PANEL_ROTATION);
  display.setBrightness(200);
  display.fillScreen(0x0000);

  // --- LVGL init ---
  lv_init();
  lv_tick_set_cb(my_tick);

  // Display driver for LVGL
  lv_disp = lv_display_create(TFT_HOR_RES, TFT_VER_RES);
  lv_display_set_flush_cb(lv_disp, my_flush);

  // Allocate small INTERNAL DMA buffers (partial rendering)
  int lines = 30;                                   // tune (20..40 works w
  size_t bytes = (size_t)TFT_HOR_RES * lines * sizeof(lv_color_t);
  lvbuf1 = (lv_color_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  lvbuf2 = (lv_color_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
 

  if (!lvbuf1 || !lvbuf2) {
    // graceful fallback → single buffer
    if (lvbuf2) { heap_caps_free(lvbuf2); lvbuf2 = nullptr; }
    if (!lvbuf1) {
      lines = 20;
      bytes = (size_t)TFT_HOR_RES * lines * sizeof(lv_color_t);
      lvbuf1 = (lv_color_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    lv_display_set_buffers(lv_disp, lvbuf1, nullptr, bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
  } else {
    lv_display_set_buffers(lv_disp, lvbuf1, lvbuf2, bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
  }

  // Touch input device
  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, my_touch_read);
  lv_indev_set_display(indev, lv_disp);

  // --- Build your UI (bars + keyboard, maps in FLASH) ---
 
  ui_init();

  Serial.println("[LVGL] TFT setup done");
}

static ID3v2Meta g_id3;

// Call once after lv_init() + driver registration
void lvgl_start_task() {
  xTaskCreatePinnedToCore(
    [](void*) {

      TickType_t last = xTaskGetTickCount();
      const TickType_t period = pdMS_TO_TICKS(5); // 60Hz

      for (;;) {
        const int net_pct = (net_filled_slots() * 100) / NUM_BUFFERS;
        const int pcm_pct  = pcm_buffer_percent();
        ui_update_stats_bars(net_pct , pcm_pct);
        ui_update_stats_decoder(codec_name_from_enum(feed_codec), currentMP3Info.samplerate, currentMP3Info.channels, currentMP3Info.kbps);
        ui_update_stats_outputs(i2s_output, a2dp_connected, a2dp_connected_name);
        ui_update_stats_wifi(WiFi.status(), WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
        //ui_update_player_id3(g_ui, true, id3m.artist, id3m.title, id3m.album, (int)id3m.track);

        lv_timer_handler();

        vTaskDelayUntil(&last, period);  
      }
    },
    "LVGL", 6144, nullptr, 1, nullptr, 1
  );
}

// ─────────────────────────────────────────────────────────────────────
//                       ID3 Helpers / Parsers
// ─────────────────────────────────────────────────────────────────────
// Helpers for distances on circular buffer
static inline size_t ring_dist(size_t from, size_t to, size_t cap) {
  return (to >= from) ? (to - from) : (cap - from + to);
}
static inline size_t ring_free(size_t wr, size_t rmin, size_t cap) {
  // Free space is everything after the slowest reader up to write-1
  return cap - ring_dist(rmin, wr, cap);
}

// Active-reader min pointer (caller holds no lock)
static inline size_t rmin_active_unlocked() {
  // Snapshot first to avoid TOCTOU
  bool a2dp_active, i2s_active;
  size_t rA, rI, w;
  portENTER_CRITICAL(&a2dp_mux);
  a2dp_active = a2dp_audio_ready;
  i2s_active = i2s_output;
  rA = a2dp_read_index;
  rI = a2dp_read_index_i2s;
  w = a2dp_write_index;
  portEXIT_CRITICAL(&a2dp_mux);

  if (a2dp_active && i2s_active) return (rA <= rI) ? rA : rI;
  if (a2dp_active) return rA;
  if (i2s_active) return rI;
  return w;  // no active readers: don't block the writer
}

// Same but assume caller already holds a2dp_mux
static inline size_t rmin_active_locked() {
  if (a2dp_audio_ready && i2s_output)
    return (a2dp_read_index <= a2dp_read_index_i2s) ? a2dp_read_index : a2dp_read_index_i2s;
  if (a2dp_audio_ready) return a2dp_read_index;
  if (i2s_output) return a2dp_read_index_i2s;
  return a2dp_write_index;  // no readers active
}

// Write once into shared PCM ring; never block. If not enough contiguous free
// (relative to the *slowest* reader), drop this frame.
static inline void ring_write_pcm_shared(const uint8_t* src, size_t nbytes) {
  if (!src || nbytes == 0) return;

  portENTER_CRITICAL(&a2dp_mux);

  bool any_active = (a2dp_audio_ready || i2s_output);

  // Choose the "min reader" for capacity accounting
  size_t rmin = any_active ? rmin_active_locked() : cold_read_index;

  // Free space vs chosen rmin (protect the oldest data while priming)
  size_t dist = ring_dist(rmin, a2dp_write_index, A2DP_BUFFER_SIZE);
  size_t free_bytes = A2DP_BUFFER_SIZE - dist;

  if (free_bytes >= nbytes) {
    size_t idx = a2dp_write_index;
    size_t first = std::min(nbytes, A2DP_BUFFER_SIZE - idx);
    memcpy(&a2dp_buffer[idx], src, first);
    if (nbytes > first) memcpy(&a2dp_buffer[0], src + first, nbytes - first);

    a2dp_write_index = (a2dp_write_index + nbytes) % A2DP_BUFFER_SIZE;

    // Update reported fill using the same rmin policy
    rmin = any_active ? rmin_active_locked() : cold_read_index;
    a2dp_buffer_fill = ring_dist(rmin, a2dp_write_index, A2DP_BUFFER_SIZE);
  }
  // else: drop this frame silently (decodeTask will throttle via DREQ)

  portEXIT_CRITICAL(&a2dp_mux);
}

static String mac_to_str(const uint8_t b[6]) {
  char s[18];
  sprintf(s, "%02X:%02X:%02X:%02X:%02X:%02X", b[0], b[1], b[2], b[3], b[4], b[5]);
  return String(s);
}

// Pause & Resume functions for use with AVRCP
void pauseDecoder() {
  g_pause = PAUSE_HELD;   // hard pause: user-requested
  decoder_paused = true;  // stops decodeTask watermark logic
}

void resumeDecoder() {
  g_pause = PAUSE_RUNNING;  // allow decoding again
  decoder_paused = false;
}

// HTTP Streaming
// Return number of filled slots
static inline int net_filled_slots() {
  int n = 0;
  for (int i = 0; i < NUM_BUFFERS; ++i)
    if (netBufFilled[i]) n++;
  return n;
}

static void ensure_wifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t0) < 15000) {
    delay(200);
  }
}

static inline void net_ring_clear() {
  for (int i = 0; i < NUM_BUFFERS; ++i) {
    netBufFilled[i] = false;
    netSize[i] = 0;
    netTag[i]  = SLOT_UNKNOWN;
    netSess[i] = 0;
  }
  netWrite = 0;
  netRead  = 0;
}

// Return the average PCM buffer fill percentage across A2DP/I2S
static int pcm_buffer_percent() {
  size_t w = 0, r_a2dp = 0, r_i2s = 0;
  size_t fill_a2dp = 0, fill_i2s = 0;

  portENTER_CRITICAL(&a2dp_mux);
  w = a2dp_write_index;
  r_a2dp = a2dp_read_index;
  r_i2s = a2dp_read_index_i2s;
  portEXIT_CRITICAL(&a2dp_mux);

  // Compute distances in ring terms
  auto ring_dist = [](size_t r, size_t w, size_t cap) -> size_t {
    return (w >= r) ? (w - r) : (cap - (r - w));
  };

  fill_a2dp = ring_dist(r_a2dp, w, A2DP_BUFFER_SIZE);
  fill_i2s  = ring_dist(r_i2s,  w, A2DP_BUFFER_SIZE);

  // Average of both outputs (if both enabled), otherwise whichever is active
  int percent = 0;
  if (a2dp_audio_ready && i2s_output)
    percent = (int)(((fill_a2dp + fill_i2s) / 2) * 100 / A2DP_BUFFER_SIZE);
  else if (a2dp_audio_ready)
    percent = (int)(fill_a2dp * 100 / A2DP_BUFFER_SIZE);
  else if (i2s_output)
    percent = (int)(fill_i2s * 100 / A2DP_BUFFER_SIZE);
  else
    percent = 0;

  return percent;
}

static inline void clear_a2dp_buffer() {
  portENTER_CRITICAL(&a2dp_mux);
  a2dp_write_index = 0;
  a2dp_read_index = 0;
  a2dp_read_index_i2s = 0;
  a2dp_buffer_fill = 0;
  if (a2dp_buffer) memset(a2dp_buffer, 0, A2DP_BUFFER_SIZE);
  portEXIT_CRITICAL(&a2dp_mux);
  Serial.println("[BUFFER] A2DP PCM buffer cleared");
}

// Force decoder to run again
static inline void request_run() {
  g_pause = PAUSE_RUNNING;
  decoder_paused = false;
}

// Hard-pause decoder immediately
static inline void request_pause_held() {
  g_pause = PAUSE_HELD;
  decoder_paused = true;
}

static const char* current_url() {
  if (current_track_index >= 0 && current_track_index < playlist_count)
    return urls[current_track_index];
  return nullptr;
}

// Optional helpers if you need them elsewhere:
static inline int wrap_inc(int i, int n) { return (i + 1) % n; }
static inline int wrap_dec(int i, int n) { return (i + n - 1) % n; }

static SlotCodec guess_slot_codec(const String& ctype, const char* url) {
  // lower-case a copy of content-type
  String ct = ctype;
  ct.toLowerCase();

  // make a mutable String for the URL, then lower-case it
  String u = url ? String(url) : String();
  u.toLowerCase();

  // quick checks (header or extension)
  if (ct.indexOf("aac") >= 0 || ct.indexOf("adts") >= 0 || ct.indexOf("mp4a") >= 0 ||
      u.endsWith(".aac") || u.endsWith(".m4a")) {
    return SLOT_AAC;
  }
  if (ct.indexOf("mpeg") >= 0 || ct.indexOf("mp3") >= 0 || u.endsWith(".mp3")) {
    return SLOT_MP3;
  }
  return SLOT_UNKNOWN;
}

static void httpFillTask(void* arg) {
  ensure_wifi();

  HTTPClient http;
  WiFiClient sock;
  sock.setNoDelay(true);

  auto prep_http = [&](HTTPClient& h, WiFiClient& s) {
    h.setReuse(true);
    h.setTimeout(2500);
    h.setConnectTimeout(4000);
  };

  // ---- Retry control (per track) ----
  constexpr int MAX_TRACK_RETRIES = 3;
  int stall_retries = 0;

  // Session-scoped stamping state
  SlotCodec session_tag = SLOT_UNKNOWN;
  bool      session_locked = false;

  // ID3v2 (per-session)
  ID3v2Collector id3c{};
  id3v2_reset_collector(&id3c);
  id3v2_reset_meta(&id3m);

  // Crossing-header helper until we lock
  uint8_t tail6[6] = {0};
  bool    tail_valid = false;

  auto connect_current = [&](int64_t& content_len) -> bool {
    const uint32_t want_session = g_play_session;
    const char* url = current_url();
    if (!url || !*url) return false;

    http.end();
    sock.stop();

    if (!http.begin(sock, url)) return false;
    prep_http(http, sock);

    int code = http.GET();
    if (g_play_session != want_session) {
      http.end();
      sock.stop();
      return false;
    }
    if (!(code == HTTP_CODE_OK || code == HTTP_CODE_PARTIAL_CONTENT)) {
      http.end();
      sock.stop();
      return false;
    }

    content_len = (int64_t)http.getSize(); // -1 if chunked

    stream_running      = true;
    g_new_track_pending = true;

    // Reset stamping + ID3 state for new session
    session_tag    = SLOT_UNKNOWN;
    session_locked = false;
    tail_valid     = false;

    id3v2_reset_collector(&id3c);
    id3v2_reset_meta(&id3m);

    Serial.printf("[HTTP] Connected: %s (sess=%u, len=%lld)\n",
                  url, (unsigned)want_session, (long long)content_len);
    return true;
  };

  // Bootstrap
  g_play_session = (uint32_t)current_track_index;
  uint32_t my_session = g_play_session;

  int64_t content_len = -1;
  int64_t bytes_seen  = 0;

  while (!connect_current(content_len)) {
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  bytes_seen      = 0;
  stall_retries   = 0;

  for (;;) {
    // Forced/implicit session change
    if (g_play_session != my_session || g_force_next) {
      g_force_next    = false;
      stream_running  = false;
      http.end();
      sock.stop();

      net_ring_clear();

      session_tag    = SLOT_UNKNOWN;
      session_locked = false;
      tail_valid     = false;

      id3v2_reset_collector(&id3c);
      id3v2_reset_meta(&id3m);

      my_session          = g_play_session;
      g_new_track_pending = true;

      content_len    = -1;
      bytes_seen     = 0;
      stall_retries  = 0;   // new session/track → reset retries

      while (!connect_current(content_len)) {
        vTaskDelay(pdMS_TO_TICKS(200));
      }
      continue;
    }

    // Need a free NET slot
    if (netBufFilled[netWrite]) {
      vTaskDelay(pdMS_TO_TICKS(1));
      continue;
    }

    uint8_t* dst          = netBuffers[netWrite];
    size_t   need         = MAX_CHUNK_SIZE;
    size_t   wrote        = 0;
    uint32_t soft_deadline = millis() + 1200;  // ~1.2s stall window

    while (need > 0) {
      if (g_play_session != my_session) {
        wrote = 0;
        break;
      }

      // Enforce Content-Length if known
      if (content_len >= 0) {
        int64_t remain = content_len - bytes_seen;
        if (remain <= 0) break; // EOF
        if ((int64_t)need > remain) need = (size_t)remain;
      }

      int avail = sock.available();
      if (avail <= 0) {
        // No data ready
        if (content_len >= 0 && bytes_seen >= content_len) break; // EOF

        // Stalled?
        if (!sock.connected() || millis() > soft_deadline) {
          // ---- Stall detected: drop this connection ----
          stream_running = false;
          http.end();
          sock.stop();
          net_ring_clear();

          session_tag    = SLOT_UNKNOWN;
          session_locked = false;
          tail_valid     = false;

          id3v2_reset_collector(&id3c);
          id3v2_reset_meta(&id3m);

          if (stall_retries < MAX_TRACK_RETRIES) {
            stall_retries++;
            Serial.printf(
              "[HTTP] Stall mid-track → retry %d/%d (sess=%u, bytes_seen=%lld)\n",
              stall_retries, MAX_TRACK_RETRIES,
              (unsigned)my_session, (long long)bytes_seen
            );

            content_len = -1;
            bytes_seen  = 0;

            while (!connect_current(content_len)) {
              vTaskDelay(pdMS_TO_TICKS(200));
            }

            wrote = 0;
            // We stay on the SAME track/session, just restarted it.
            break;
          }

          // ---- Too many stalls → advance to next track ----
          Serial.printf(
            "[HTTP] Stall mid-track → giving up after %d retries → next track\n",
            MAX_TRACK_RETRIES
          );

          stall_retries = 0;

          current_track_index = (current_track_index + 1) % playlist_count;
          g_play_session      = (uint32_t)current_track_index;
          my_session          = g_play_session;
          g_new_track_pending = true;

          content_len = -1;
          bytes_seen  = 0;

          while (!connect_current(content_len)) {
            vTaskDelay(pdMS_TO_TICKS(200));
          }

          wrote = 0;
          break;
        }

        vTaskDelay(pdMS_TO_TICKS(1));
        continue;
      }

      int toRead = avail;
      if (toRead > (int)need) toRead = (int)need;

      int n = sock.read(dst + (MAX_CHUNK_SIZE - need), toRead);
      if (n > 0) {
        need        -= (size_t)n;
        wrote       += (size_t)n;
        bytes_seen  += n;
        soft_deadline = millis() + 1200;
      } else {
        break;
      }
    }

    if (wrote > 0) {
      // ====================== ID3v2 prefilter ======================
      uint8_t* cur        = dst;
      size_t   detect_len = wrote;   // bytes remaining after any ID3 peel

      if (!session_locked && !id3m.header_found) {
        // Try begin only at absolute stream start (first packet)
        id3v2_try_begin(cur,
                        detect_len,
                        (uint64_t)(bytes_seen - wrote),
                        (size_t)MAX_CHUNK_SIZE,
                        &id3c);

        if (id3c.collecting) {
          size_t taken = id3v2_consume(cur, detect_len, &id3c, &id3m);
          cur        += taken;
          detect_len -= taken;

          if (id3m.header_found) {
            // You can later display id3m.title / artist / album, etc.
          }
        }
      }

      // ====================== Header detection / stamping ======================
      SlotCodec tag_for_this_slot    = session_tag;
      uint16_t  offset_for_this_slot = 0;

      if (!session_locked) {
        // Build a crossing-slot view from what remains
        uint8_t view[MAX_CHUNK_SIZE + 6];
        int     vlen = 0;

        int cp = (detect_len > MAX_CHUNK_SIZE) ? MAX_CHUNK_SIZE : (int)detect_len;
        if (tail_valid) {
          memcpy(view,       tail6, 6);
          memcpy(view + 6,   cur,   cp);
          vlen = 6 + cp;
        } else {
          memcpy(view, cur, cp);
          vlen = cp;
        }

        audetect::DetectResult dr{};
        if (audetect::detect_audio_format_strict(view, vlen, &dr)) {
          switch (dr.format) {
            case audetect::AF_MP3:
              tag_for_this_slot = SLOT_MP3;
              break;
            case audetect::AF_AAC_ADTS:
            case audetect::AF_AAC_LOAS:
            case audetect::AF_AAC_ADIF:
              tag_for_this_slot = SLOT_AAC;
              break;
            default:
              tag_for_this_slot = SLOT_UNKNOWN;
              break;
          }

          if (tag_for_this_slot != SLOT_UNKNOWN) {
            const int head_bias = tail_valid ? 6 : 0;
            int off = dr.offset - head_bias;
            if (off < 0) off = 0;
            if (off > (int)detect_len) off = (int)detect_len;
            offset_for_this_slot = (uint16_t)off;

            session_tag    = tag_for_this_slot;
            session_locked = true;
            tail_valid     = false;

            Serial.printf(
              "🪪 HTTP stamp: %s%s (offset %u) sess %u\n",
              (session_tag == SLOT_AAC ? "AAC" : "MP3"),
              (id3m.header_found ? " + ID3" : ""),
              (unsigned)offset_for_this_slot,
              (unsigned)my_session
            );
          } else {
            // Update tail6 from the remaining data for cross-slot detection
            if (detect_len >= 6) {
              memcpy(tail6, cur + detect_len - 6, 6);
              tail_valid = true;
            } else if (detect_len > 0) {
              if (tail_valid) {
                memmove(tail6, tail6 + detect_len, 6 - detect_len);
                memcpy(tail6 + (6 - detect_len), cur, detect_len);
              } else {
                memset(tail6, 0, 6 - detect_len);
                memcpy(tail6 + (6 - detect_len), cur, detect_len);
              }
              tail_valid = true;
            }
          }
        } else {
          // No header yet; slide tail window
          if (detect_len >= 6) {
            memcpy(tail6, cur + detect_len - 6, 6);
            tail_valid = true;
          } else if (detect_len > 0) {
            if (tail_valid) {
              memmove(tail6, tail6 + detect_len, 6 - detect_len);
              memcpy(tail6 + (6 - detect_len), cur, detect_len);
            } else {
              memset(tail6, 0, 6 - detect_len);
              memcpy(tail6 + (6 - detect_len), cur, detect_len);
            }
            tail_valid = true;
          }
        }
      }

      // Publish this slot (size = bytes left after ID3 peel)
      netSize[netWrite]      = (uint16_t)detect_len;
      netTag[netWrite]       = (uint8_t)tag_for_this_slot;
      netOffset[netWrite]    = offset_for_this_slot;
      netSess[netWrite]      = my_session;
      netBufFilled[netWrite] = true;

      g_lastNetWriteMs = millis();
      g_httpBytesTick  += detect_len;

      netWrite = (netWrite + 1) % NUM_BUFFERS;
      taskYIELD();
    }

    // Natural EOF → graceful drain → next track
    if (content_len > 0 && bytes_seen >= content_len) {
      stream_running = false;
      http.end();
      sock.stop();

      uint32_t t0 = millis();
      while (net_filled_slots() > 0 || pcm_buffer_percent() > 0) {
        if (g_play_session != my_session || g_force_next) break;
        vTaskDelay(pdMS_TO_TICKS(10));
        if (millis() - t0 > 8000) break;
      }

      current_track_index = (current_track_index + 1) % playlist_count;
      g_play_session      = (uint32_t)current_track_index;
      my_session          = g_play_session;
      g_new_track_pending = true;

      session_tag    = SLOT_UNKNOWN;
      session_locked = false;
      tail_valid     = false;

      id3v2_reset_collector(&id3c);
      id3v2_reset_meta(&id3m);

      content_len   = -1;
      bytes_seen    = 0;
      stall_retries = 0;  // new track, fresh retry budget

      while (!connect_current(content_len)) {
        vTaskDelay(pdMS_TO_TICKS(200));
      }
      continue;
    }

    // vTaskDelay(pdMS_TO_TICKS(1));  // optional: let WiFi breathe
  }
}

bool player_stop() {
  request_pause_held();
  stream_running = false;
  Serial.println("[PLAYER] ⏹ Stop");
  return true;
}

bool player_next() {
  if (playlist_count <= 0) return false;

  net_ring_clear();
  clearAudioBuffers();
  
  // Advance playlist index (wrap at end)prevous
  current_track_index = wrap_inc(current_track_index, playlist_count);

  // Bump to a fresh, unique session id (monotonic so decoder always detects change)
  static uint32_t session_seq = 0;
  g_play_session = ++session_seq;

  // Tell HTTP task to advance immediately; it will reconnect to current_url()
  g_force_next        = true;
  g_new_track_pending = true;     // useful for UI or any "track-start" hooks

  
  // Do NOT touch stream_running, decoder_paused, output_ready
  // Do NOT clear NET ring or PCM/A2DP buffers — seamless handover
  request_run();                  // nudge worker tasks if your system uses this

  Serial.printf("[PLAYER] ▶ Next → %d: %s (sess=%u)\r\n",
                current_track_index, current_url(), (unsigned)g_play_session);

  return true;
}

bool player_previous() {
  if (playlist_count <= 0) return false;

  net_ring_clear();
  clearAudioBuffers();

  // Step back one track (wrap at start)
  current_track_index = wrap_dec(current_track_index, playlist_count);

  // Bump to a fresh, unique session id so the decoder switches cleanly
  static uint32_t session_seq = 0;
  g_play_session = ++session_seq;

  // Ask HTTP task to advance immediately to the new URL
  g_force_next        = true;
  g_new_track_pending = true;   // helpful for UI/metadata refresh hooks

  if (i2s_output) {
    stopI2S();  
    i2s_zero_dma_buffer(I2S_NUM_0);
    clearAudioBuffers();
    startI2S();  
  }

  clearAudioBuffers();

  request_run();                // nudge workers if your system uses this

  // No ring clears, no pausing — let decodeTask roll through seamlessly
  Serial.printf("[PLAYER] ◀ Prev → %d: %s (sess=%u)\r\n",
                current_track_index, current_url(), (unsigned)g_play_session);
  return true;
}

// ────────────────────────────────────────────────
// EQ
// ────────────────────────────────────────────────
// ───────── String utils ─────────
static inline bool ieq(const char* a, const char* b) {
  if (!a || !b) return false;
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
    ++a;
    ++b;
  }
  return *a == '\0' && *b == '\0';
}

// Parse MAC in forms: "AA:BB:CC:DD:EE:FF" or "AA-BB-...-FF" (case-insensitive)
// Returns true on success, fills out[6]
static bool parse_mac(const char* s, uint8_t out[6]) {
  if (!s) return false;
  int vals[6];
  // tolerate either ':' or '-' separators (or none with 12 hex chars)
  if (strchr(s, ':') || strchr(s, '-')) {
    int c = sscanf(s, "%x%*[:-]%x%*[:-]%x%*[:-]%x%*[:-]%x%*[:-]%x",
               &vals[0], &vals[1], &vals[2], &vals[3], &vals[4], &vals[5]);
    if (c != 6) return false;
  } else {
    // compact 12-hex format
    if (strlen(s) != 12) return false;
    int c = sscanf(s, "%2x%2x%2x%2x%2x%2x",
                   &vals[0], &vals[1], &vals[2], &vals[3], &vals[4], &vals[5]);
    if (c != 6) return false;
  }
  for (int i = 0; i < 6; ++i) {
    if (vals[i] < 0 || vals[i] > 255) return false;
    out[i] = (uint8_t)vals[i];
  }
  return true;
}

// Accepts printable ASCII (space..~) and simple punctuation in a 0x00-framed line
static bool is_ascii_line(const uint8_t* buf, size_t n) {
  if (!buf || n == 0) return false;
  for (size_t i = 0; i < n; ++i) {
    uint8_t c = buf[i];
    // allow typical text incl. spaces, commas, equals, colon, plus, digits/letters, CR/LF
    if (c == '\r' || c == '\n' || c == ' ' || c == ',' || c == '=' || c == ':' || c == '+' || c == '-')
      continue;
    if (c < 0x21 || c > 0x7E) return false;  // reject control chars (except CR/LF handled above)
  }
  return true;
}

void serial_process() {
  static String cmdLine;

  // Non-blocking serial read
  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\r' || c == '\n') {
      if (cmdLine.length() == 0) continue;
      cmdLine.trim();
      const char* line = cmdLine.c_str();
      bool ok = true;

      // ---- AT command handling ----
      if (!strcasecmp(line, "AT")) {
        Serial.println("OK");
      }

      else if (!strncasecmp(line, "AT+BTSCAN", 9)) {
        uint32_t secs = 10;
        if (const char* p = strchr(line, '=')) {
          int v = atoi(p + 1);
          if (v >= 3 && v <= 60) secs = (uint32_t)v;
        }
        if (ui_state != UI_CONNECTING) {
          if (g_autorc_task) { vTaskDelete(g_autorc_task); g_autorc_task = nullptr; }
          startRawScan(secs);
          Serial.println("OK");
        } else ok = false;
      }

      else if (!strcasecmp(line, "AT+BTCANCEL")) {
        stopAllDiscovery();
        if (a2dp_connected) {
          esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_STOP);
          esp_a2d_source_disconnect(target_addr);
        }
        ui_state = UI_MENU;
        connect_deadline_ms = 0;
        Serial.println("OK");
      }

      else if (!strncasecmp(line, "AT+BTCONNECT=", 13)) {
        uint8_t mac[6];
        if (!parse_mac(line + 13, mac) || g_gap_discovery_active) ok = false;
        else {
          memcpy(target_addr, mac, 6);
          auto st = connect_direct_mac(target_addr);
          Serial.println((st == BT_CONNECT_SUCCESS) ? "OK" : "ERROR");
        }
      }

      else if (!strncasecmp(line, "AT+BTCONNECTI=", 14)) {
        int idx = atoi(line + 14);
        if (idx < 1 || idx > found_count || g_gap_discovery_active) ok = false;
        else { connectToMenuIndex(idx); Serial.println("OK"); }
      }

      else if (!strcasecmp(line, "AT+BTDISCONNECT")) {
        if (a2dp_connected) {
          esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_STOP);
          esp_a2d_source_disconnect(target_addr);
        }
        Serial.println("OK");
      }

      else if (!strcasecmp(line, "AT+BTSTATUS?")) {
        if (g_have_last_bda || nvs_load_last_bda(g_last_bda)) {
          g_have_last_bda = true;
          Serial.println("OK");
        } else {
          Serial.println("ERROR");
        }
      }

      else if (!strncasecmp(line, "AT+AUTO=", 8)) {
        const char* v = line + 8;
        if (!strcasecmp(v, "ON")) {
          g_auto_reconnect_enabled = true;
          nvs_save_autorc(true);
          if (!a2dp_connected && (g_have_last_bda || nvs_load_last_bda(g_last_bda))) {
            g_have_last_bda = true;
            a2dp_start_autorc_task();
          }
          Serial.println("OK");
        } else if (!strcasecmp(v, "OFF")) {
          g_auto_reconnect_enabled = false;
          nvs_save_autorc(false);
          Serial.println("OK");
        } else ok = false;
      }

      else if (!strncasecmp(line, "AT+EQ=", 6)) {
        const char* v = line + 6;
        if (!strcasecmp(v, "ON"))  { g_eq_enabled = true;  Serial.println("OK"); }
        else if (!strcasecmp(v, "OFF")) { g_eq_enabled = false; Serial.println("OK"); }
        else ok = false;
      }

      // ── NEW: I2S ON/OFF ───────────────────────────────────────────
      else if (!strncasecmp(line, "AT+I2S=", 7)) {
        const char* v = line + 7;
        if (!strcasecmp(v, "ON"))  { 
          startI2S();  
          Serial.println("OK"); 
        }
        else if (!strcasecmp(v, "OFF")) { 
          stopI2S(); 
          Serial.println("OK"); 
        }
        else ok = false;
      }
      // ──────────────────────────────────────────────────────────────

      else {
        ok = false;
      }

      if (!ok) Serial.println("ERROR");
      cmdLine = "";
    }
    else if (isPrintable(c)) {
      cmdLine += c;
      if (cmdLine.length() > 128) cmdLine = ""; // prevent runaway
    }
  }
}

// Search & Discovery code
static void init_nvs_bt_once() {
  static bool done = false;
  if (done) return;

  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    ret = nvs_flash_init();
  }
  if (ret != ESP_OK) {
    Serial.printf("NVS init failed: %s\r\n", esp_err_to_name(ret));
    // we keep going; some BT stacks still work, but it's best to fix NVS
  }
  done = true;
}

// EIR name extractor
static void get_name_from_eir(uint8_t* eir, char* out, size_t out_len) {
  if (!eir || !out || out_len == 0) return;
  uint8_t* name = nullptr;
  uint8_t name_len = 0;

  name = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &name_len);
  if (!name) name = esp_bt_gap_resolve_eir_data(eir, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &name_len);

  if (name && name_len) {
    if (name_len >= out_len) name_len = out_len - 1;
    memcpy(out, name, name_len);
    out[name_len] = '\0';
  } else {
    out[0] = '\0';
  }
}

// Save/load/forget last connected device MAC
static bool nvs_save_last_bda(const uint8_t bda[6]) {
  nvs_handle_t h;
  if (nvs_open(NVS_NS_BT, NVS_READWRITE, &h) != ESP_OK) return false;
  esp_err_t e = nvs_set_blob(h, KEY_LAST_BDA, bda, 6);
  if (e == ESP_OK) e = nvs_commit(h);
  nvs_close(h);
  return e == ESP_OK;
}
static bool nvs_load_last_bda(uint8_t out[6]) {
  size_t sz = 6;
  nvs_handle_t h;
  if (nvs_open(NVS_NS_BT, NVS_READONLY, &h) != ESP_OK) return false;
  esp_err_t e = nvs_get_blob(h, KEY_LAST_BDA, out, &sz);
  nvs_close(h);
  return (e == ESP_OK && sz == 6);
}
static void nvs_forget_last_bda() {
  nvs_handle_t h;
  if (nvs_open(NVS_NS_BT, NVS_READWRITE, &h) != ESP_OK) return;
  nvs_erase_key(h, KEY_LAST_BDA);
  nvs_commit(h);
  nvs_close(h);
}

// Save/load auto-reconnect flag
static void nvs_save_autorc(bool en) {
  nvs_handle_t h;
  if (nvs_open(NVS_NS_BT, NVS_READWRITE, &h) != ESP_OK) return;
  nvs_set_u8(h, KEY_AUTORC, en ? 1 : 0);
  nvs_commit(h);
  nvs_close(h);
}
static bool nvs_load_autorc(bool* out) {
  uint8_t v = 1;
  nvs_handle_t h;
  if (nvs_open(NVS_NS_BT, NVS_READONLY, &h) != ESP_OK) {
    *out = true;
    return false;
  }
  esp_err_t e = nvs_get_u8(h, KEY_AUTORC, &v);
  nvs_close(h);
  *out = (e == ESP_OK) ? (v != 0) : true;
  return (e == ESP_OK);
}

// Compact g_devs[] -> found[] (optionally RSSI sort for nicer menu)
static void rebuild_found_from_scan(bool sort_by_rssi_desc) {
  found_count = 0;
  for (int i = 0; i < g_dev_count && found_count < MAX_SCAN_ENTRIES; ++i) {
    // ... your duplicate filtering if any ...
    found[found_count] = g_devs[i];  // struct copy: includes addr, name, rssi
    if (!found[found_count].name[0]) {
      strlcpy(found[found_count].name, "Unknown", sizeof(found[found_count].name));
    }
    ++found_count;
  }

  if (sort_by_rssi_desc && found_count > 1) {
    std::sort(&found[0], &found[0] + found_count, [](const BTDevice& A, const BTDevice& B){
      return A.rssi > B.rssi;
    });
  }
}

static void startRawScan(uint32_t seconds);

// GAP callback: collect devices and emit +BTRES lines (+BTSCAN:END on stop) 
static void raw_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param) {
  switch (event) {

    // -----------------------------------------------------------------------
    // Discovery state changes (normal AT+SCAN support)
    // -----------------------------------------------------------------------
    case ESP_BT_GAP_DISC_STATE_CHANGED_EVT: {
      if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED) {
        for (int i = 0; i < MAX_SCAN_ENTRIES; ++i) g_devs[i] = BTDevice{};
        g_dev_count = 0;
        g_gap_discovery_active = true;
        Serial.println("AT+SCAN:START");
      }
      else if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
        g_gap_discovery_active = false;

        // Build and publish final scan list
        rebuild_found_from_scan(true /* sort_by_rssi_desc */);
        for (int i = 0; i < found_count; ++i) {
          char mac[18];
          snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                   found[i].addr[0], found[i].addr[1], found[i].addr[2],
                   found[i].addr[3], found[i].addr[4], found[i].addr[5]);
          const char* nm = (found[i].name[0] != '\0') ? found[i].name : "Unknown";
          Serial.printf("AT+SCANRESULT=%d,\"%s\",%s,%d\r\n", i + 1, nm, mac, found[i].rssi);
        }
        Serial.println("AT+SCAN:STOP");
        Serial.printf("AT+SCANEND,%d\r\n", found_count);
      }
    } break;

    // -----------------------------------------------------------------------
    // Discovery results (normal scan device found)
    // -----------------------------------------------------------------------
    case ESP_BT_GAP_DISC_RES_EVT: {
      if (!g_gap_discovery_active) break;

      // De-duplicate by MAC
      int idx = -1;
      for (int i = 0; i < g_dev_count; ++i) {
        if (memcmp(g_devs[i].addr, param->disc_res.bda, sizeof(esp_bd_addr_t)) == 0) {
          idx = i; break;
        }
      }
      if (idx < 0) {
        if (g_dev_count >= MAX_SCAN_ENTRIES) break;
        idx = g_dev_count++;
        memcpy(g_devs[idx].addr, param->disc_res.bda, sizeof(esp_bd_addr_t));
        g_devs[idx].rssi = -127;
        g_devs[idx].last_emit_rssi = -127;
        g_devs[idx].ever_emitted = false;
        strlcpy(g_devs[idx].name, "Unknown", sizeof(g_devs[idx].name));
      }

      // Extract name from EIR/BDNAME
      char nm_buf[ESP_BT_GAP_MAX_BDNAME_LEN + 1] = {0};
      for (int i = 0; i < param->disc_res.num_prop; ++i) {
        const esp_bt_gap_dev_prop_t* p = &param->disc_res.prop[i];
        if (p->type == ESP_BT_GAP_DEV_PROP_EIR && p->val) {
          get_name_from_eir((uint8_t*)p->val, nm_buf, sizeof(nm_buf));
          if (nm_buf[0]) break;
        }
      }
      if (!nm_buf[0]) {
        for (int i = 0; i < param->disc_res.num_prop; ++i) {
          const esp_bt_gap_dev_prop_t* p = &param->disc_res.prop[i];
          if (p->type == ESP_BT_GAP_DEV_PROP_BDNAME && p->val && p->len > 0) {
            size_t n = p->len;
            if (n > sizeof(nm_buf) - 1) n = sizeof(nm_buf) - 1;
            memcpy(nm_buf, p->val, n);
            nm_buf[n] = '\0';
            break;
          }
        }
      }

      // Store name if available
      if (nm_buf[0] && (!g_devs[idx].name[0] || strcmp(g_devs[idx].name, "Unknown") == 0)) {
        strlcpy(g_devs[idx].name, nm_buf, sizeof(g_devs[idx].name));
      }

      // Track strongest RSSI
      for (int i = 0; i < param->disc_res.num_prop; ++i) {
        const esp_bt_gap_dev_prop_t* p = &param->disc_res.prop[i];
        if (p->type == ESP_BT_GAP_DEV_PROP_RSSI && p->val && p->len == sizeof(int8_t)) {
          int r = *(int8_t*)p->val;
          if (r > g_devs[idx].rssi) g_devs[idx].rssi = r;
        }
      }
    } break;

    // -----------------------------------------------------------------------
    // Remote name response (post-connection name resolution)
    // -----------------------------------------------------------------------
    case ESP_BT_GAP_READ_REMOTE_NAME_EVT: {
      if (param->read_rmt_name.stat == ESP_BT_STATUS_SUCCESS) {
        const char* name = (const char*)param->read_rmt_name.rmt_name;
        if (name && *name) {
          strlcpy(a2dp_connected_name, name, sizeof(a2dp_connected_name));
          Serial.printf("[BT] Sink name resolved (remote): %s\r\n", a2dp_connected_name);
        } else {
          Serial.println("[BT] Remote name read OK but empty");
        }
      } else {
        Serial.printf("[BT] Failed to read remote name: %d\r\n", param->read_rmt_name.stat);
      }
    } break;

    default:
      break;
  }
}


// Hard-stop ANY discovery path (library + GAP) before connecting
static void stopAllDiscovery() {
  // Do NOT tear down the A2DP source here; we might connect next.
  if (g_gap_discovery_active) {
    esp_bt_gap_cancel_discovery();
    // we'll get DISC_STATE_CHANGED -> stopped soon
  }
}

static void startRawScan(uint32_t seconds) {
  Serial.println(F("\n🔍 Scanning for A2DP sinks (raw GAP)…"));
  if (!ensure_bt_ready()) {
    Serial.println(F("❌ Bluetooth stack not ready for GAP scan."));
    return;
  }

  clear_found();
  ui_state = UI_SCANNING;

  if (!gap_cb_registered) {
    esp_bt_gap_register_callback(raw_gap_cb);
    gap_cb_registered = true;
  }
  // general inquiry; 'seconds' is the inquiry window
  esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, seconds, 0);
}

// Direct MAC connect using ESP-IDF (no discovery running)
static BtConnectionStatus connect_direct_mac(const uint8_t* mac) {
  if (!mac) return BT_CONNECT_NULL_MAC;

  stopAllDiscovery();  // just cancels GAP now
  if (!ensure_bt_ready()) return BT_CONNECT_FAIL_REQUEST;

  if (!a2dp_started) {
    a2dp_started = true;
    delay(200);
  }

  esp_err_t err = esp_a2d_source_connect((uint8_t*)mac);  // API wants non-const
  if (err == ESP_OK) {
    ui_state = UI_CONNECTING;
    connect_deadline_ms = millis() + 30000;  // 30s timeout
    return BT_CONNECT_SUCCESS;
  } else {
    // do NOT end() here; keep stack alive for immediate retry
    return BT_CONNECT_FAIL_REQUEST;
  }
}

static void print_addr(const esp_bd_addr_t a) {
  char buf[18];
  sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X", a[0], a[1], a[2], a[3], a[4], a[5]);
  Serial.print(buf);
}

// Clears the discovered device list
static void clear_found() {
  found_count = 0;
}

// Compare two 6-byte BT addresses
static inline bool addr_equal(const esp_bd_addr_t a, const esp_bd_addr_t b) {
  return memcmp(a, b, 6) == 0;
}

static int add_or_update_device(const char* name, const esp_bd_addr_t addr, int rssi) {
  // 1) Update path
  for (int i = 0; i < found_count; ++i) {
    if (memcmp(found[i].addr, addr, 6) == 0) {
      strlcpy(found[i].name, (name && *name) ? name : "(unknown)", sizeof(found[i].name));

      found[i].rssi = rssi;
      return i + 1;  // one-based index
    }
  }

  // 2) Add path
  if (found_count < MAX_DEV) {
    BTDevice& d = found[found_count++];
    strlcpy(d.name, (name && *name) ? name : "(unknown)", sizeof(d.name));
    memcpy(d.addr, addr, 6);
    d.rssi = rssi;
    return found_count;  // one-based index of new item
  }

  // List full
  return 0;
}

// Connect by the 1-based menu index (unchanged API)
static void connectToMenuIndex(int oneBased) {
  if (oneBased <= 0 || oneBased > found_count) return;
  const BTDevice& d = found[oneBased - 1];

  memcpy(target_addr, d.addr, 6);

  Serial.print(F("🔗 Connecting to "));
  Serial.print(d.name);
  Serial.print(F("  "));
  print_addr(d.addr);
  Serial.println();

  BtConnectionStatus st = connect_direct_mac(target_addr);
  if (st == BT_CONNECT_SUCCESS) {
    Serial.println(F("…connect request sent; waiting for CONNECTED."));
  } else {
    Serial.println(F("❌ connect request failed immediately — try again (device in pairing mode?)."));
    ui_state = UI_MENU;
  }
}

static bool ensure_bt_ready() {
  // Make sure NVS is ready for bonding/keys
  init_nvs_bt_once();

  // Controller status
  esp_bt_controller_status_t cstat = esp_bt_controller_get_status();

  if (cstat == ESP_BT_CONTROLLER_STATUS_ENABLED) {
    // Already up (and you started Classic-only earlier) — good to go.
  } else {
    // If it's not idle, deinit it so we can reconfigure cleanly
    if (cstat != ESP_BT_CONTROLLER_STATUS_IDLE) {
      esp_bt_controller_disable();
      esp_bt_controller_deinit();
    }

    // We do NOT use BLE — free its memory BEFORE init
    esp_bt_controller_mem_release(ESP_BT_MODE_BLE);

    // Init + enable CLASSIC ONLY
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if (esp_bt_controller_init(&bt_cfg) != ESP_OK) return false;
    if (esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT) != ESP_OK) return false;
  }

  // Bluedroid (Classic stack)
  esp_bluedroid_status_t bstat = esp_bluedroid_get_status();
  if (bstat == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
    if (esp_bluedroid_init() != ESP_OK) return false;
  }
  if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
    if (esp_bluedroid_enable() != ESP_OK) return false;
  }

  return true;
}

// Try a direct connect to last device (returns true if CONNECTED within timeout)
static bool a2dp_try_connect_last(uint32_t timeout_ms) {
  if (!g_have_last_bda && !nvs_load_last_bda(g_last_bda)) return false;

  stopAllDiscovery();
  if (!ensure_bt_ready()) return false;

  // If a connect() is already in progress, wait (up to timeout_ms)
  uint32_t wait_t0 = millis();
  while (g_connect_inflight && (millis() - wait_t0) < timeout_ms) {
    delay(25);
  }
  if (g_connect_inflight) return false;  // still busy

  g_connect_inflight = true;
  Serial.printf("📡 AutoRC: trying %s\r\n", mac_to_str(g_last_bda).c_str());

  esp_err_t err = esp_a2d_source_connect((uint8_t*)g_last_bda);
  if (err != ESP_OK) {
    Serial.printf("⚠️  esp_a2d_source_connect failed: %s\r\n", esp_err_to_name(err));
    g_connect_inflight = false;
    return false;
  }

  uint32_t t0 = millis();
  while (!a2dp_connected && (millis() - t0) < timeout_ms) {
    delay(25);
  }

  g_connect_inflight = false;
  return a2dp_connected;
}

// ───────── AutoRC task (robust, cancellable) ─────────
static void a2dp_autorc_task(void* arg) {
  uint32_t backoff = 3000;             // 5s initial
  const uint32_t backoff_max = 30000;  // 30s cap

  // Pin optional: vTaskCoreAffinitySet(NULL, (1<<0)); // core 0

  for (;;) {
    // 0) Allow external cancel with notify (instant)
    if (ulTaskNotifyTake(pdTRUE, 0) > 0) break;

    // 1) Exit if already connected or feature off
    if (!g_auto_reconnect_enabled || a2dp_connected) break;

    // 2) Respect minimum cooldown window
    uint32_t now = millis();
    if (now < g_next_reconnect_ms) {
      uint32_t wait_ms = g_next_reconnect_ms - now;
      // Wait, but allow cancel during wait
      if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(wait_ms)) > 0) break;
      if (!g_auto_reconnect_enabled || a2dp_connected) break;
    }

    // 3) Do not collide with discovery
    stopAllDiscovery();

    // 4) Try to connect to last device (7s window)
    (void)a2dp_try_connect_last(3000);
    if (a2dp_connected) break;

    // 5) Jittered backoff, cancellable sleep
    uint32_t ticks  = xTaskGetTickCount();
    uint32_t jitter = (ticks ^ (ticks << 3)) & 0x2EE;
    uint32_t sleep_ms = backoff + (jitter % 751);
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(sleep_ms)) > 0) break;

    if (backoff < backoff_max) backoff = std::min(backoff * 2, backoff_max);
    g_next_reconnect_ms = millis() + AUTORC_MIN_COOLDOWN_MS; // small spacing to next try
  }

  // Self-teardown (safe even if we were externally cancelled)
  g_autorc_task = nullptr;
  vTaskDelete(NULL);
}

// Start only when meaningful
static void a2dp_start_autorc_task() {
  if (!g_auto_reconnect_enabled) return;
  if (a2dp_connected) return;
  if (g_autorc_task) return;
  if (!g_have_last_bda && !nvs_load_last_bda(g_last_bda)) return;

  BaseType_t ok = xTaskCreatePinnedToCore(
      a2dp_autorc_task, "a2dp_autorc",
      4096, nullptr, 2, &g_autorc_task, 0); // low prio on core 0

  if (ok != pdPASS) {
    g_autorc_task = nullptr;
    Serial.println("⚠️ AutoRC task create failed");
  }
}

// Cancel immediately (from any context)
static void a2dp_stop_autorc_task() {
  if (!g_autorc_task) return;
  // Notify once; task will null the handle itself
  xTaskNotifyGive(g_autorc_task);
}

void clearAudioBuffers() {
  portENTER_CRITICAL(&a2dp_mux);
  a2dp_write_index = 0;
  a2dp_read_index = 0;
  a2dp_read_index_i2s = 0;
  a2dp_buffer_fill = 0;
  cold_read_index = 0;     // NEW
  cold_prime_mode = true;  // NEW
  portEXIT_CRITICAL(&a2dp_mux);

  Serial.println("[BUFFER] Shared PCM buffer cleared");
}

void setupI2S() {
  i2s_config_t config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,  // keep modest to save IRAM
    .dma_buf_len = 128,
    .use_apll = false,
    .tx_desc_auto_clear = true
  };
  i2s_pin_config_t pins = {
    .bck_io_num = I2S_BCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_DATA,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  esp_err_t e = i2s_driver_install(I2S_NUM_0, &config, 0, NULL);
  if (e != ESP_OK) {
    Serial.printf("i2s_driver_install failed: %s\r\n", esp_err_to_name(e));
    // Don’t proceed—avoid later calls that will touch null queues
    i2s_output = false;  // gate I2S task and pcm writer
    return;
  }
  e = i2s_set_pin(I2S_NUM_0, &pins);
  if (e != ESP_OK) {
    Serial.printf("i2s_set_pin failed: %s\r\n", esp_err_to_name(e));
    i2s_driver_uninstall(I2S_NUM_0);
    i2s_output = false;
    return;
  }
}

// ───────────────────────────────────────────────
// Start/Stop I²S: independent of A2DP
// ───────────────────────────────────────────────
int startI2S() {
  Serial.println("🔊 I2S Output Active (dual-reader)");
  i2s_output = true;

  vTaskResume(i2sTaskHandle);

  portENTER_CRITICAL(&a2dp_mux);
  if (cold_prime_mode) {
    // Start I²S from the primed beginning
    a2dp_read_index_i2s = cold_read_index;
    // Keep A2DP reader where it is; it will be set on A2DP start
    // Maintain a2dp_buffer_fill vs ACTIVE readers now that we have one
    a2dp_buffer_fill = ring_dist(rmin_active_locked(), a2dp_write_index, A2DP_BUFFER_SIZE);
    cold_prime_mode = false;  // we have at least one real consumer now
  } else {
    // Align to current shared position (no backlog catch-up)
    a2dp_read_index_i2s = a2dp_read_index;
    a2dp_buffer_fill = ring_dist(rmin_active_locked(), a2dp_write_index, A2DP_BUFFER_SIZE);
  }
  portEXIT_CRITICAL(&a2dp_mux);

  
  //if (pcm_buffer_percent() >= 50) output_ready = true;
  return 0;
}

int stopI2S() {
  Serial.println("🔇 I2S Output Disabled");
  i2s_output = false;

  vTaskSuspend(i2sTaskHandle);

  i2s_zero_dma_buffer(I2S_NUM_0);

  // When I²S turns off, keep I²S reader caught up to avoid blocking the writer
  portENTER_CRITICAL(&a2dp_mux);
  a2dp_read_index_i2s = a2dp_read_index;
  a2dp_buffer_fill = ring_dist(rmin_active_locked(), a2dp_write_index, A2DP_BUFFER_SIZE);
  portEXIT_CRITICAL(&a2dp_mux);

  return 0;
}
// Graphic EQ
// ─────────────────────────── EQ (10-band graphic) ───────────────────────────
static void eq_set_peak(Biquad& biq, float fs, float f0, float dBgain, float Q = 1.00f) {
  // No work if 0 dB (set to passthrough)
  if (fabsf(dBgain) < 1e-6f) {
    biq = Biquad{};
    biq.b0 = 1.0f;
    return;
  }
  const float A = powf(10.0f, dBgain / 40.0f);
  const float w0 = 2.0f * (float)M_PI * (f0 / fs);
  const float cw0 = cosf(w0);
  const float sw0 = sinf(w0);
  const float alpha = sw0 / (2.0f * Q);

  float b0 = 1.0f + alpha * A;
  float b1 = -2.0f * cw0;
  float b2 = 1.0f - alpha * A;
  float a0 = 1.0f + alpha / A;
  float a1 = -2.0f * cw0;
  float a2 = 1.0f - alpha / A;

  biq.b0 = b0 / a0;
  biq.b1 = b1 / a0;
  biq.b2 = b2 / a0;
  biq.a1 = a1 / a0;
  biq.a2 = a2 / a0;

  // clear states (avoid zipper from old coeffs)
  biq.z1L = biq.z2L = biq.z1R = biq.z2R = 0.0f;
}

// Recompute all bands for current sample rate
static void eq_rebuild_all_locked() {
  for (int i = 0; i < EQ_BANDS; ++i) {
    // eqc_set_peak(g_eq[i], ...)   // <-- typo
    eq_set_peak(g_eq[i], (float)g_eq_sr, kEqFreqs[i], g_eq_gain_db[i], 1.00f);
  }
}

static void eq_set_samplerate(int sr) {
  if (sr <= 8000 || sr > 192000) return;
  portENTER_CRITICAL(&g_eq_mux);
  if (g_eq_sr != sr) {
    g_eq_sr = sr;
    eq_rebuild_all_locked();
  }
  portEXIT_CRITICAL(&g_eq_mux);
}

static void eq_set_gain_freq(float freq, float dB) {
  // clamp to –15..+15
  if (dB < -15.0f) dB = -15.0f;
  if (dB > +15.0f) dB = +15.0f;

  // find nearest band
  int bi = 0;
  float best = 1e9f;
  for (int i = 0; i < EQ_BANDS; ++i) {
    float diff = fabsf(freq - kEqFreqs[i]);
    if (diff < best) {
      best = diff;
      bi = i;
    }
  }
  portENTER_CRITICAL(&g_eq_mux);
  g_eq_gain_db[bi] = dB;
  eq_set_peak(g_eq[bi], (float)g_eq_sr, kEqFreqs[bi], dB, 1.00f);
  portEXIT_CRITICAL(&g_eq_mux);
}

static void eq_reset_all() {
  portENTER_CRITICAL(&g_eq_mux);
  for (int i = 0; i < EQ_BANDS; ++i) g_eq_gain_db[i] = 0.0f;
  eq_rebuild_all_locked();
  portEXIT_CRITICAL(&g_eq_mux);
}

// Process interleaved S16 stereo in-place (len = number of int16_t samples)
static inline void eq_process_block(int16_t* pcm, size_t len) {
  if (!g_eq_enabled || !pcm || len < 2) return;

  // Take EQ snapshot & decision under a single lock
  Biquad local_eq[EQ_BANDS];
  float  local_gain_db[EQ_BANDS];

  portENTER_CRITICAL(&g_eq_mux);
  bool allZero = true;
  for (int i = 0; i < EQ_BANDS; ++i) {
    local_eq[i]       = g_eq[i];           // copy coeffs + state
    local_gain_db[i]  = g_eq_gain_db[i];
    if (fabsf(local_gain_db[i]) > 1e-6f) {
      allZero = false;
    }
  }
  portEXIT_CRITICAL(&g_eq_mux);

  if (allZero) return;

  // Work in floats for stability, clamp back to S16
  // Stereo interleaved: L,R,L,R,...
  for (size_t i = 0; i + 1 < len; i += 2) {
    float xL = (float)pcm[i + 0] / 32768.0f;
    float xR = (float)pcm[i + 1] / 32768.0f;

    // Apply all bands using *local* coeffs/states
    for (int b = 0; b < EQ_BANDS; ++b) {
      const Biquad& c = local_eq[b];

      // Skip true bypass band (b0==1,b1=b2=a1=a2=0)
      if (fabsf(c.b0 - 1.0f) < 1e-6f &&
          fabsf(c.b1)        < 1e-6f &&
          fabsf(c.b2)        < 1e-6f &&
          fabsf(c.a1)        < 1e-6f &&
          fabsf(c.a2)        < 1e-6f) {
        continue;
      }

      // Left
      float yL = c.b0 * xL + local_eq[b].z1L;
      local_eq[b].z1L = c.b1 * xL - c.a1 * yL + local_eq[b].z2L;
      local_eq[b].z2L = c.b2 * xL - c.a2 * yL;

      // Right
      float yR = c.b0 * xR + local_eq[b].z1R;
      local_eq[b].z1R = c.b1 * xR - c.a1 * yR + local_eq[b].z2R;
      local_eq[b].z2R = c.b2 * xR - c.a2 * yR;

      xL = yL;
      xR = yR;
    }

    // soft clamp
    if (xL > 1.0f) xL = 1.0f;
    else if (xL < -1.0f) xL = -1.0f;
    if (xR > 1.0f) xR = 1.0f;
    else if (xR < -1.0f) xR = -1.0f;

    pcm[i + 0] = (int16_t)lrintf(xL * 32767.0f);
    pcm[i + 1] = (int16_t)lrintf(xR * 32767.0f);
  }

  // Push updated states back once at the end
  portENTER_CRITICAL(&g_eq_mux);
  for (int i = 0; i < EQ_BANDS; ++i) {
    g_eq[i].z1L = local_eq[i].z1L;
    g_eq[i].z2L = local_eq[i].z2L;
    g_eq[i].z1R = local_eq[i].z1R;
    g_eq[i].z2R = local_eq[i].z2R;
  }
  portEXIT_CRITICAL(&g_eq_mux);
}

// Process interleaved S16 stereo in-place (len = number of int16_t samples)< 1e-6f) continue
void onA2DPConnected(const uint8_t* bda) {
  a2dp_connected = true;
  memcpy(a2dp_connected_bda, bda, sizeof(esp_bd_addr_t));

  // Default placeholder until the name is resolved
  strlcpy(a2dp_connected_name, "Unknown", sizeof(a2dp_connected_name));

  // Attempt to read remote name directly (works even while connected)
  esp_err_t err = esp_bt_gap_read_remote_name((uint8_t*)bda);
  if (err == ESP_OK) {
    Serial.println("[BT] Requested remote device name...");
  } else {
    Serial.printf("[BT] Failed to request remote name: %d\r\n", err);
  }

  // Print MAC immediately as feedback
  char macbuf[18];
  snprintf(macbuf, sizeof(macbuf),
           "%02X:%02X:%02X:%02X:%02X:%02X",
           bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
  Serial.printf("[BT] Connected to %s (name pending)\r\n", macbuf);
}


static void onA2DPDisconnected()
{
  a2dp_connected = false;
  a2dp_connected_name[0] = '\0';
  memset(a2dp_connected_bda, 0, sizeof(a2dp_connected_bda));
  Serial.println("[A2DP] Disconnected");
}

// NEW: AAC -> PCM callback (shared ring writer; no I2S buffer)
void aacDataCallback(AACFrameInfo& info, int16_t* pcm, size_t len, void*) {
  if (!pcm || len == 0) return;

  // Resolve total interleaved samples (int16_t units)
  size_t total_samples = len;
#ifdef LEN_IS_SAMPLES_PER_CHANNEL
  if (info.nChans > 0) total_samples *= (size_t)info.nChans;
#endif

  // Some decoders can occasionally report odd sample counts for mono;
  // ensure we don't split a stereo pair if nChans==2.
  if (info.nChans == 2 && (total_samples & 1U)) total_samples -= 1U;
  if (total_samples == 0) return;

  // OLED/status
  currentMP3Info.samplerate = (uint16_t)info.sampRateOut;  // or sampRateCore if you prefer
  currentMP3Info.channels   = (uint8_t)info.nChans;
  currentMP3Info.kbps       = 0;      // not provided here
  currentMP3Info.layer      = 0;      // not MP3

  // Only touch EQ samplerate if it changed (avoids work each frame)
  static uint16_t last_eq_sr = 0;
  if (last_eq_sr != currentMP3Info.samplerate) {
    eq_set_samplerate((int)currentMP3Info.samplerate);
    last_eq_sr = currentMP3Info.samplerate;
  }

  // In-place EQ, then write to shared ring
  eq_process_block(pcm, total_samples);  // interleaved S16
  const size_t pcm_bytes = total_samples * sizeof(int16_t);
  ring_write_pcm_shared(reinterpret_cast<uint8_t*>(pcm), pcm_bytes);
}

// --- MP3 -> PCM callback (shared ring writer; no I2S buffer)
// --- MP3 -> PCM callback (shared ring writer; no I2S buffer)
void mp3dataCallback(MP3FrameInfo& info, int16_t* pcm, size_t len, void*) {
  if (!pcm || len == 0) return;

  // Resolve total interleaved samples (int16_t units)
  size_t total_samples = len;
#ifdef LEN_IS_SAMPLES_PER_CHANNEL
  if (info.nChans > 0) total_samples *= (size_t)info.nChans;
#endif
  // Ensure we don't split a stereo pair if nChans==2
  if (info.nChans == 2 && (total_samples & 1U)) total_samples -= 1U;
  if (total_samples == 0) return;

  // --- Lightweight status update (throttled) ---
  static uint32_t last_bitrate = 0;
  static uint16_t last_sr      = 0;
  static uint8_t  last_ch      = 0;
  static uint8_t  last_layer   = 0;
  static uint8_t  frame_mod    = 0;

  const uint32_t cur_bitrate = (uint32_t)info.bitrate;
  const uint16_t cur_sr      = (uint16_t)info.samprate;
  const uint8_t  cur_ch      = (uint8_t)info.nChans;
  const uint8_t  cur_layer   = (uint8_t)info.layer;

  bool changed =
      (cur_bitrate != last_bitrate) ||
      (cur_sr      != last_sr)      ||
      (cur_ch      != last_ch)      ||
      (cur_layer   != last_layer);

  // Only touch global status every 4 frames or if something actually changed
  if (++frame_mod >= 4 || changed) {
    frame_mod    = 0;
    last_bitrate = cur_bitrate;
    last_sr      = cur_sr;
    last_ch      = cur_ch;
    last_layer   = cur_layer;

    currentMP3Info.samplerate = cur_sr;
    currentMP3Info.channels   = cur_ch;

    int kbps = (last_bitrate > 0) ? (int)(last_bitrate / 1000U) : 0;
    if (kbps < 8 || kbps > 512) kbps = 0;  // sanity clamp
    currentMP3Info.kbps  = (uint16_t)kbps;
    currentMP3Info.layer = cur_layer;

    // Only touch EQ samplerate when it actually changes
    static uint16_t last_eq_sr = 0;
    if (last_eq_sr != cur_sr) {
      eq_set_samplerate((int)cur_sr);
      last_eq_sr = cur_sr;
    }
  }

  // --- EQ + ring write (hot path) ---
  eq_process_block(pcm, total_samples);  // interleaved S16
  const size_t pcm_bytes = total_samples * sizeof(int16_t);
  ring_write_pcm_shared(reinterpret_cast<uint8_t*>(pcm), pcm_bytes);
}

// A2DP (sink) bluetooth data callback
int32_t pcm_data_callback(uint8_t* data, int32_t len) {
  if (!data || len <= 0) return 0;

  // If the stream or output path isn't ready (e.g. during BT teardown), don't touch the ring
  if (!a2dp_audio_ready || !output_ready) return 0;

  // Critical section protects indices and fill bookkeeping
  portENTER_CRITICAL(&a2dp_mux);

  // Re-check after taking the lock (disconnect might have flipped flags)
  if (!a2dp_audio_ready || !output_ready) {
    portEXIT_CRITICAL(&a2dp_mux);
    return 0;
  }

  const size_t cap = A2DP_BUFFER_SIZE;
  uint8_t* buf = a2dp_buffer;
  if (!buf || cap == 0) {
    portEXIT_CRITICAL(&a2dp_mux);
    return 0;
  }

  const size_t r0 = a2dp_read_index;
  const size_t w0 = a2dp_write_index;

  size_t avail = ring_dist(r0, w0, cap);
  if (avail == 0) {
    portEXIT_CRITICAL(&a2dp_mux);
    return 0;
  }

  // How much can we actually provide this call
  size_t to_copy = (size_t)len;
  if (to_copy > avail) to_copy = avail;

  // First contiguous run before wrap
  const size_t space_to_end = cap - r0;
  size_t first = (to_copy < space_to_end) ? to_copy : space_to_end;

  // Copy out while still under lock to avoid concurrent index changes
  memcpy(data, buf + r0, first);

  // Wrap if needed
  if (first < to_copy) {
    memcpy(data + first, buf, to_copy - first);
  }

  // Advance read index
  a2dp_read_index = (r0 + to_copy) % cap;

  // Recompute global fill vs the active slowest reader (still under lock)
  const size_t rmin = rmin_active_locked();
  a2dp_buffer_fill = ring_dist(rmin, a2dp_write_index, cap);

  portEXIT_CRITICAL(&a2dp_mux);

  return (int32_t)to_copy;
}

static void bt_app_a2dp_callback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t* param) {
  switch (event) {

    // ------------------------------------------------------------
    // LINK STATE (CONNECT / DISCONNECT / etc.)
    // ------------------------------------------------------------
    case ESP_A2D_CONNECTION_STATE_EVT: {
      esp_a2d_connection_state_t s = param->conn_stat.state;

      // Keep your UI/flags dispatcher
      onA2DPConnectState(s, nullptr);

      if (s == ESP_A2D_CONNECTION_STATE_CONNECTED) {
        // Update our connected state + friendly name (MAC immediately; async GAP name if enabled)
        onA2DPConnected(param->conn_stat.remote_bda);

        // Remember peer for future auto-reconnect
        memcpy(g_last_bda, param->conn_stat.remote_bda, 6);
        g_have_last_bda = true;
        nvs_save_last_bda(g_last_bda);

        // Ask controller if source is ready; we'll START when ACK says ready
        esp_err_t r = esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY);
        if (r != ESP_OK) {
          Serial.printf("A2DP media ctrl CHECK_SRC_RDY failed: %s\r\n", esp_err_to_name(r));
        }
      }
      else if (s == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
        // Stop + flush PCM ring so the next START begins cleanly
        esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_STOP);

        portENTER_CRITICAL(&a2dp_mux);
        a2dp_write_index     = 0;
        a2dp_read_index      = 0;
        a2dp_read_index_i2s  = 0;
        a2dp_buffer_fill     = 0;
        if (a2dp_buffer) memset(a2dp_buffer, 0, A2DP_BUFFER_SIZE);
        portEXIT_CRITICAL(&a2dp_mux);

        onA2DPDisconnected();

        // Auto-reconnect (if enabled and we have a target)
        if (g_auto_reconnect_enabled && g_have_last_bda) {
          stopAllDiscovery();  // scanning interferes with reconnect
          Serial.println("🔁 AutoRC: scheduling reconnect…");
          g_next_reconnect_ms = millis() + AUTORC_MIN_COOLDOWN_MS;
          a2dp_start_autorc_task();
          // UI remains in CONNECTING via onA2DPConnectState()
        }
      }
      else if (s == ESP_A2D_CONNECTION_STATE_CONNECTING) {
        Serial.println("A2DP: CONNECTING…");
      }
      else if (s == ESP_A2D_CONNECTION_STATE_DISCONNECTING) {
        Serial.println("A2DP: DISCONNECTING…");
      }
      break;
    }

    // ------------------------------------------------------------
    // AUDIO STREAM STATE (STARTED / STOPPED / REMOTE SUSPEND)
    // ------------------------------------------------------------
    case ESP_A2D_AUDIO_STATE_EVT: {
      onA2DPAudioState(param->audio_stat.state, nullptr);
      break;
    }

    // ------------------------------------------------------------
    // AUDIO CONFIG (derive sample rate / channels from SBC caps)
    // ------------------------------------------------------------
    case ESP_A2D_AUDIO_CFG_EVT: {
      if (param->audio_cfg.mcc.type == ESP_A2D_MCT_SBC) {
        // sbc[0] bits 7..4 = sample rate flags; bit1 often indicates mono
        const uint8_t oct0 = param->audio_cfg.mcc.cie.sbc[0];

        int sr = 44100;
        if      (oct0 & (1 << 4)) sr = 48000;   // 48 kHz
        else if (oct0 & (1 << 5)) sr = 44100;   // 44.1 kHz
        else if (oct0 & (1 << 6)) sr = 32000;   // 32 kHz
        else if (oct0 & (1 << 7)) sr = 16000;   // 16 kHz (rare)

        i2s_set_sample_rates(I2S_NUM_0, sr);
        Serial.printf("A2DP SRC: audio cfg → sr=%d Hz\r\n", sr);
      }
      break;
    }

    // ------------------------------------------------------------
    // MEDIA CONTROL ACKS (CHECK_SRC_RDY / START / STOP)
    // ------------------------------------------------------------
    case ESP_A2D_MEDIA_CTRL_ACK_EVT: {
      auto cmd = param->media_ctrl_stat.cmd;
      auto st  = param->media_ctrl_stat.status;

      if (cmd == ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY) {
        if (st == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS) {
          Serial.println("A2DP SRC: Source ready. Starting stream…");
          esp_err_t r = esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
          if (r != ESP_OK) {
            Serial.printf("A2DP media ctrl START failed: %s\r\n", esp_err_to_name(r));
          }
        } else {
          Serial.printf("A2DP SRC: CHECK_SRC_RDY not ready (ack=%d)\n", (int)st);
        }
      } else if (cmd == ESP_A2D_MEDIA_CTRL_START) {
        Serial.printf("A2DP SRC: START ack=%d\r\n", (int)st);
      } else if (cmd == ESP_A2D_MEDIA_CTRL_STOP) {
        Serial.printf("A2DP SRC: STOP ack=%d\r\n", (int)st);
      } else {
        Serial.printf("A2DP SRC: media ctrl cmd=%d ack=%d\r\n", (int)cmd, (int)st);
      }
      break;
    }

    default:
      // Uncomment for verbose tracing:
      // Serial.printf("A2DP evt=%d\n", (int)event);
      break;
  }
}


static inline bool any_output_enabled() {
  return i2s_output || a2dp_audio_ready;
}

static inline void refresh_output_ready_after_change() {
  // If at least one output is available and we’ve already primed enough frames,
  // keep (or make) output_ready true. Otherwise, drop it.
  if (any_output_enabled()) {
    //if (pcm_buffer_percent() >= 75) output_ready = true;
    // else: decodeTask will set it once primed
  } else {
    output_ready = false;
    decoder_paused = false;  // leave flow-control to decodeTask when outputs return
  }
}

// Update UI + pacing flags on connection state changes
void onA2DPConnectState(esp_a2d_connection_state_t state, void*) {
  static esp_a2d_connection_state_t prev = ESP_A2D_CONNECTION_STATE_DISCONNECTED;
  if (state == prev) {
    a2dp_connected = (state == ESP_A2D_CONNECTION_STATE_CONNECTED);
    return;
  }
  prev = state;

  switch (state) {
    case ESP_A2D_CONNECTION_STATE_CONNECTED:
      {
        a2dp_connected = true;
        g_a2dpConnected = true;     // <-- add
        g_connect_inflight = false;
        ui_state = UI_STREAMING;
        connect_deadline_ms = 0;

        if (g_autorc_task) { vTaskDelete(g_autorc_task); g_autorc_task = nullptr; }

        Serial.println("A2DP Connected");
        break;
      }

    case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
      {
        a2dp_connected = false;
        g_connect_inflight = false;
        a2dp_audio_ready = false;

        // Only park the A2DP reader; keep I²S data intact.
        portENTER_CRITICAL(&a2dp_mux);
        a2dp_read_index = a2dp_write_index;  // drop A2DP backlog only
        a2dp_buffer_fill = ring_dist(rmin_active_locked(), a2dp_write_index, A2DP_BUFFER_SIZE);
        portEXIT_CRITICAL(&a2dp_mux);

        // Keep output_ready if I²S is still on and we’re primed.
        refresh_output_ready_after_change();
 

        // Schedule AutoRC if enabled
        if (g_auto_reconnect_enabled && g_have_last_bda) {
          stopAllDiscovery();
          g_next_reconnect_ms = millis() + AUTORC_MIN_COOLDOWN_MS;
          a2dp_start_autorc_task();
        }

        if (ui_state == UI_CONNECTING) {
          ui_state = UI_MENU;
          connect_deadline_ms = 0;
        } else if (ui_state != UI_SCANNING) {
          ui_state = UI_IDLE;
        }
        break;
      }

    case ESP_A2D_CONNECTION_STATE_CONNECTING:
      {
        a2dp_connected = false;
        g_connect_inflight = true;
        ui_state = UI_CONNECTING;
        Serial.println("A2DP Connecting…");
        break;
      }

    case ESP_A2D_CONNECTION_STATE_DISCONNECTING:
      {
        a2dp_connected = false;
        g_connect_inflight = false;
        Serial.println("A2DP Disconnecting…");
        break;
      }

    default:
      {
        a2dp_connected = false;
        Serial.printf("A2DP state: %d\r\n", (int)state);
        break;
      }
  }
}

void onA2DPAudioState(esp_a2d_audio_state_t state, void*) {
  switch (state) {
    case ESP_A2D_AUDIO_STATE_STARTED:
      {
        a2dp_audio_ready = true;

        //esp_coex_preference_set(ESP_COEX_PREFER_BT);

        // If we were still priming, start A2DP at the primed head and exit priming
        portENTER_CRITICAL(&a2dp_mux);
        if (cold_prime_mode) {
          a2dp_read_index = cold_read_index;
          a2dp_buffer_fill = ring_dist(rmin_active_locked(), a2dp_write_index, A2DP_BUFFER_SIZE);
          cold_prime_mode = false;
        } else {
          a2dp_buffer_fill = ring_dist(rmin_active_locked(), a2dp_write_index, A2DP_BUFFER_SIZE);
        }
        portEXIT_CRITICAL(&a2dp_mux);

        //if (pcm_buffer_percent() >= 75) output_ready = true;

        Serial.println("🔊 A2DP Sink Ready — Audio streaming started!");
        break;
      }

#if defined(ESP_A2D_AUDIO_STATE_SUSPENDED)
    case ESP_A2D_AUDIO_STATE_SUSPENDED:
#elif defined(ESP_A2D_AUDIO_STATE_SUSPEND)
    case ESP_A2D_AUDIO_STATE_SUSPEND:
#endif
      {
        a2dp_audio_ready = false;
        portENTER_CRITICAL(&a2dp_mux);
        a2dp_read_index = a2dp_write_index;  // drop A2DP backlog only
        a2dp_buffer_fill = ring_dist(rmin_active_locked(), a2dp_write_index, A2DP_BUFFER_SIZE);
        portEXIT_CRITICAL(&a2dp_mux);
        refresh_output_ready_after_change();
        Serial.println("⏸️ A2DP suspended");
        break;
      }

    case ESP_A2D_AUDIO_STATE_STOPPED:
    default:
      {
        a2dp_audio_ready = false;

        //esp_coex_preference_set(ESP_COEX_PREFER_BALANCE);

        portENTER_CRITICAL(&a2dp_mux);
        a2dp_read_index = a2dp_write_index;
        a2dp_buffer_fill = ring_dist(rmin_active_locked(), a2dp_write_index, A2DP_BUFFER_SIZE);
        portEXIT_CRITICAL(&a2dp_mux);
        refresh_output_ready_after_change();
        Serial.println("⛔ A2DP stopped");
        break;
      }
  }
}

void avrc_target_callback(esp_avrc_tg_cb_event_t event, esp_avrc_tg_cb_param_t* param) {
  switch (event) {
    case ESP_AVRC_TG_CONNECTION_STATE_EVT:
      {
        ESP_LOGI("AVRCP", "AVRCP Target connected: %d", param->conn_stat.connected);

        if (param->conn_stat.connected) {
          // Enable all allowed passthrough commands
          esp_avrc_psth_bit_mask_t psth_set = { 0 };
          esp_err_t err = esp_avrc_tg_get_psth_cmd_filter(ESP_AVRC_PSTH_FILTER_ALLOWED_CMD, &psth_set);
          if (err == ESP_OK) {
            err = esp_avrc_tg_set_psth_cmd_filter(ESP_AVRC_PSTH_FILTER_SUPPORTED_CMD, &psth_set);
            if (err != ESP_OK) {
              ESP_LOGE("AVRCP", "Failed to set passthrough command filter: %s", esp_err_to_name(err));
            } else {
              ESP_LOGI("AVRCP", "Passthrough capabilities set successfully.");
            }
          } else {
            ESP_LOGE("AVRCP", "Failed to get allowed passthrough commands: %s", esp_err_to_name(err));
          }
        }
        break;
      }

    case ESP_AVRC_TG_PASSTHROUGH_CMD_EVT:
      {

        uint8_t key = param->psth_cmd.key_code;
        uint8_t state = param->psth_cmd.key_state;

        if (state == ESP_AVRC_PT_CMD_STATE_PRESSED) {
          const char* button_name = nullptr;

          switch (key) {
            case ESP_AVRC_PT_CMD_PLAY:
              button_name = "PLAY";
              //resumeDecoder();
              Serial.println("[AVRCP] Play");
              break;
            case ESP_AVRC_PT_CMD_PAUSE:
              button_name = "PAUSE";
              //pauseDecoder();
              Serial.println("[AVRCP] Pause");
              break;
            case ESP_AVRC_PT_CMD_STOP:
              button_name = "STOP";
              Serial.println("[AVRCP] Stop");
              break;
            case ESP_AVRC_PT_CMD_FORWARD:
              button_name = "NEXT";
              Serial.printf("\r\n[AVRCP] Next Track\r\n");
              player_next();
              break;
            case ESP_AVRC_PT_CMD_BACKWARD:
              button_name = "PREV";
              Serial.printf("\r\n[AVRCP] Prev Track\r\n");
              player_previous();
              break;
            case ESP_AVRC_PT_CMD_FAST_FORWARD: button_name = "FF"; break;
            case ESP_AVRC_PT_CMD_REWIND: button_name = "RW"; break;
            case ESP_AVRC_PT_CMD_VOL_UP: button_name = "VOLUP"; break;
            case ESP_AVRC_PT_CMD_VOL_DOWN: button_name = "VOLDOWN"; break;
            case ESP_AVRC_PT_CMD_MUTE: button_name = "MUTE"; break;
            default:
              {
                Serial.printf("AT+AVRCPBUTT=UNKNOWN,0x%02X\r\n", key);
                return;
              }
          }

          //Serial.printf("AT+AVRCPBUTT=%s\r\n", button_name);
        }
        break;
      }


    case ESP_AVRC_TG_REMOTE_FEATURES_EVT:
      {
        ESP_LOGI("AVRCP", "Remote features: feat_mask=0x%x, ct_feat_flag=0x%x",
                 param->rmt_feats.feat_mask,
                 param->rmt_feats.ct_feat_flag);
        break;
      }

    default:
      ESP_LOGW("AVRCP", "Unhandled AVRCP Target event: %d", event);
      break;
  }
}


// Return the index of a filled slot, or -1 if none.
// Does NOT clear the slot or advance netRead.
static inline int get_next_filled_slot() {
  // Fast path: current read cursor
  if (netBufFilled[netRead]) return netRead;

  // Scan forward once around the ring
  for (int i = 1; i < NUM_BUFFERS; ++i) {
    int j = (netRead + i) % NUM_BUFFERS;
    if (netBufFilled[j]) return j;
  }
}

// Runs on its own task: consumes NET ring slots, selects codec from gNet,
// trims with netOffset, and feeds Helix. Hysteresis: pause >=90%, resume <=50%
// Runs periodically: one decode step per tick, bounded work, no extra sleeps.
// Adaptive, periodic decoder: adjusts cadence/budget by buffer levels and NET backloggh
void decodeTask(void* /*param*/) {

  // ---- Hysteresis thresholds ----
  constexpr int HI_PCT    = 70;    // pause decode when >= 90%
  constexpr int LO_PCT    = 50;    // resume decode when <= 20%
  constexpr int PRIME_PCT = 80;    // NET must reach 80% before decoding

  const size_t HI_BYTES    = (A2DP_BUFFER_SIZE * HI_PCT) / 100;
  const size_t LO_BYTES    = (A2DP_BUFFER_SIZE * LO_PCT) / 100;
  const size_t PRIME_SLOTS = (NUM_BUFFERS      * PRIME_PCT) / 100;

  // ---- Period presets ----
  constexpr TickType_t PERIOD_FAST = pdMS_TO_TICKS(2);
  constexpr TickType_t PERIOD_NORM = pdMS_TO_TICKS(2);
  constexpr TickType_t PERIOD_SLOW = pdMS_TO_TICKS(5);

  constexpr int BUDGET_FAST = 2;
  constexpr int BUDGET_NORM = 2;
  constexpr int BUDGET_SLOW = 1;

  // ---- Decoder state ----
  CodecKind active_codec = CODEC_UNKNOWN;
  bool mp3_begun = false;
  bool aac_begun = false;

  // Force first session mismatch
  uint32_t last_session = 0xFFFFFFFFu;

  decoder_paused = false;
  output_ready   = true;

  bool priming = true;   // NET priming

  // ---- Helpers ----
  auto dist = [](size_t r, size_t w, size_t cap)->size_t {
    return (w >= r) ? (w - r) : (cap - (r - w));
  };

  auto tag_to_codec = [](uint8_t tag)->CodecKind {
    if (tag == SLOT_MP3) return CODEC_MP3;
    if (tag == SLOT_AAC) return CODEC_AAC;
    return CODEC_UNKNOWN;
  };

  auto begin_codec = [&](CodecKind k){
    if (k == CODEC_MP3 && !mp3_begun) { mp3.begin(); mp3_begun = true; }
    if (k == CODEC_AAC && !aac_begun) { aac.begin(); aac_begun = true; }
    active_codec = k;
  };

  auto end_codec = [&](){
    if (mp3_begun) { mp3.end(); mp3_begun = false; }
    if (aac_begun) { aac.end(); aac_begun = false; }
    active_codec = CODEC_UNKNOWN;
  };

  auto net_backlog = [&](){
    int c = 0;
    for (int i = 0; i < NUM_BUFFERS; ++i)
      if (netBufFilled[i]) c++;
    return c;
  };

  // ---- Scheduler ----
  TickType_t period    = PERIOD_NORM;
  TickType_t last_wake = xTaskGetTickCount();

  // ==========================================================================
  //                            MAIN LOOP
  // ==========================================================================
  for (;;) {
    vTaskDelayUntil(&last_wake, period);

    const int backlog = net_backlog();

    // ============================================================
    //                NET PRIMING BEFORE DECODING
    // ============================================================
    if (priming) {
      if (backlog >= PRIME_SLOTS) {
        priming = false;
        decoder_paused = false;
        Serial.printf("[DEC] NET primed (%d slots)\n", backlog);
      } else {
        continue;   // wait for NET fill
      }
    }

    // ---- Snapshot PCM indices ----
    size_t w, rA, rI;
    portENTER_CRITICAL(&a2dp_mux);
    w  = a2dp_write_index;
    rA = a2dp_read_index;
    rI = a2dp_read_index_i2s;
    portEXIT_CRITICAL(&a2dp_mux);

    const size_t dA = dist(rA, w, A2DP_BUFFER_SIZE);
    const size_t dI = dist(rI, w, A2DP_BUFFER_SIZE);

    // ============================================================
    //              OUTPUT-READY GATING (A2DP / I2S / BOTH)
    // ============================================================
    //
    // Decode ONLY when at least one output path can consume PCM.
    // i2s_output      -> I2S path enabled and always ready
    // a2dp_audio_ready -> A2DP Bluetooth audio active
    //
    // Unified condition:
    //       i2s_output OR a2dp_audio_ready
    //
    if (!(i2s_output || a2dp_audio_ready)) {
      period = PERIOD_NORM;
      continue;
    }

    // ============================================================
    //                PCM BUFFER HYSTERESIS
    // ============================================================
    if (!decoder_paused && (dA >= HI_BYTES || dI >= HI_BYTES))
      decoder_paused = true;

    if (decoder_paused && (dA <= LO_BYTES || dI <= LO_BYTES))
      decoder_paused = false;

    if (decoder_paused) {
      period = PERIOD_NORM;
      continue;
    }

    // ============================================================
    //     ADAPTIVE CADENCE BASED ON PCM + NET BACKLOG
    // ============================================================
    int slots_budget;
    if (dA < LO_BYTES || dI < LO_BYTES || backlog >= (NUM_BUFFERS * 3 / 4)) {
      period = PERIOD_FAST; slots_budget = BUDGET_FAST;
    } else if (dA > HI_BYTES * 9 / 10 && dI > HI_BYTES * 9 / 10) {
      period = PERIOD_SLOW; slots_budget = BUDGET_SLOW;
    } else {
      period = PERIOD_NORM; slots_budget = BUDGET_NORM;
    }

    // ============================================================
    //                     DECODE LOOP
    // ============================================================
    while (slots_budget-- > 0) {

      if (!netBufFilled[netRead])
        break;

      int slot = netRead;
      uint8_t* ptr    = netBuffers[slot];
      uint16_t bytes  = netSize[slot];
      uint32_t sess   = netSess[slot];
      uint8_t  tag    = netTag[slot];
      uint16_t offset = netOffset[slot];

      // ---- Session changed? ----
      if (sess != last_session) {
        priming = true;         // re-prime on new track
        end_codec();
        feed_codec = CODEC_UNKNOWN;
        last_session = sess;
        Serial.printf("[DEC] NEW SESSION %u\n", sess);
        break;
      }

      // ---- Codec selection ----
      CodecKind want = tag_to_codec(tag);
      if (want != active_codec && want != CODEC_UNKNOWN) {
        feed_codec = want;
        end_codec();
        begin_codec(want);
      }

      // ---- Apply audio header offset ----
      if (offset > 0 && offset < bytes) {
        ptr   += offset;
        bytes -= offset;
      }

      // ---- Feed codec ----
      if (bytes > 0) {
        if (active_codec == CODEC_MP3)      mp3.write(ptr, bytes);
        else if (active_codec == CODEC_AAC) aac.write(ptr, bytes);
      }

      // ---- Release NET slot ----
      netBufFilled[slot] = false;
      netSize[slot]      = 0;
      netRead = (slot + 1) % NUM_BUFFERS;
    }
  }

  TASK_NEVER_RETURN();
}

// ──────────────────────────────────────────────
// I²S playback: dual-reader on the shared PCM ring
// ───────────────────────────────────────────────
// Efficient 16-bit stereo I²S playback (4 bytes/frame), tuned to dma_buf_len=256
void i2sPlaybackTask(void* /*param*/) {
  // Match your I²S DMA frame length (keep this = i2s_config.dma_buf_len)
  constexpr size_t FRAMES_PER_CHUNK = 128;
  constexpr size_t BYTES_PER_FRAME  = 4;                  // L16 + R16
  constexpr size_t CHUNK_BYTES      = FRAMES_PER_CHUNK * BYTES_PER_FRAME;

  // Small staging buffer on stack (1 KB)
  uint8_t tempBuf[CHUNK_BYTES];

  for (;;) {
    // Fast exit if output is gated
    if (!i2s_output || !output_ready || g_pause == PAUSE_HELD) {
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }

    // Snapshot indices once; compute availability without holding the lock long
    size_t read_idx_snapshot, write_idx_snapshot, avail;
    portENTER_CRITICAL(&a2dp_mux);
    read_idx_snapshot  = a2dp_read_index_i2s;
    write_idx_snapshot = a2dp_write_index;
    avail = ring_dist(read_idx_snapshot, write_idx_snapshot, A2DP_BUFFER_SIZE);
    portEXIT_CRITICAL(&a2dp_mux);

    if (avail < CHUNK_BYTES) {
      // Not enough PCM yet; short, cooperative nap
      vTaskDelay(pdMS_TO_TICKS(2));
      continue;
    }

    // Copy CHUNK_BYTES from ring → tempBuf, handling wrap — outside the lock
    const size_t first = std::min(CHUNK_BYTES, A2DP_BUFFER_SIZE - read_idx_snapshot);
    memcpy(tempBuf, &a2dp_buffer[read_idx_snapshot], first);
    if (first < CHUNK_BYTES) {
      memcpy(tempBuf + first, &a2dp_buffer[0], CHUNK_BYTES - first);
    }

    // Now advance the I²S reader atomically and refresh shared fill
    portENTER_CRITICAL(&a2dp_mux);
    a2dp_read_index_i2s = (read_idx_snapshot + CHUNK_BYTES) % A2DP_BUFFER_SIZE;
    a2dp_buffer_fill    = ring_dist(rmin_active_locked(), a2dp_write_index, A2DP_BUFFER_SIZE);
    portEXIT_CRITICAL(&a2dp_mux);

    // Push exactly one DMA chunk; short timeout to stay responsive
    size_t written = 0;
    (void)i2s_write(I2S_NUM_0, tempBuf, CHUNK_BYTES, &written, pdMS_TO_TICKS(5));

    // If the driver returned early (rare), top off without blocking the system
    while (written < CHUNK_BYTES) {
      size_t w2 = 0;
      (void)i2s_write(I2S_NUM_0, tempBuf + written, CHUNK_BYTES - written, &w2, pdMS_TO_TICKS(5));
      written += w2;
      // yield a breath if DMA is momentarily saturated
      if (w2 == 0) vTaskDelay(pdMS_TO_TICKS(1));
    }


  }

  TASK_NEVER_RETURN();
}

static void bt_guard_start_classic() {
  // Release BLE RAM only BEFORE any btStart()
  esp_bt_controller_mem_release(ESP_BT_MODE_BLE);  // harmless if already released

  if (!btStarted()) {
    if (!btStart()) {
      Serial.println("[BT] btStart() failed");
      return;
    }
  }

  // Ensure Bluedroid is up
  auto bstat = esp_bluedroid_get_status();
  if (bstat == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
    ESP_ERROR_CHECK(esp_bluedroid_init());
  }
  if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
    ESP_ERROR_CHECK(esp_bluedroid_enable());
  }

  // Optional QoS preference
  (void)esp_coex_preference_set(ESP_COEX_PREFER_BT);
  //(void)esp_coex_preference_set(ESP_COEX_PREFER_BALANCE);

  // Safe to use GAP/A2DP now
  (void)esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
}

void start_bluetooth_classic() {
  bt_guard_start_classic();    // brings controller+bluedroid up (Classic)
  Serial.println("[BT] Classic stack READY");
}

void print_heaps() {
  size_t dram_free     = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  size_t dram_largest  = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  size_t psram_free    = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  size_t psram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
  Serial.printf("[HEAP]  DRAM free=%u  largest=%u | PSRAM free=%u  largest=%u\r\n",
                (unsigned)dram_free, (unsigned)dram_largest,
                (unsigned)psram_free, (unsigned)psram_largest);
}

void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      wifi_status.connected = false;
      wifi_status.ssid = WiFi.SSID();
      Serial.printf("[WIFI] Connected to AP '%s' (awaiting IP)...\r\n", wifi_status.ssid.c_str());
      break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      wifi_status.connected = true;
      wifi_status.ip       = WiFi.localIP().toString();
      wifi_status.gateway  = WiFi.gatewayIP().toString();
      wifi_status.subnet   = WiFi.subnetMask().toString();
      wifi_status.ssid     = WiFi.SSID();

      Serial.printf("[WIFI] ✅ Got IP: %s | GW: %s | MASK: %s\r\n",
                    wifi_status.ip.c_str(),
                    wifi_status.gateway.c_str(),
                    wifi_status.subnet.c_str());
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      wifi_status.connected = false;
      wifi_status.ip = wifi_status.gateway = wifi_status.subnet = "";
      //Serial.println("[WIFI] ❌ Disconnected");
      break;

    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);

  heap_caps_malloc_extmem_enable(0);

  setupTFT();
  lvgl_start_task();

  delay(1000);

  // Wi-Fi and HTTP producer
  WiFi.mode(WIFI_STA);
  WiFi.onEvent(WiFiEvent);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("[WIFI] Establishing Connection");
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t0) < 15000) {
    Serial.print(".");
    delay(250);
  }
  Serial.println("\r\n");

  g_lastNetWriteMs = millis();


  init_nvs_bt_once();

  start_bluetooth_classic();

  a2dp_buffer = (uint8_t*)heap_caps_malloc(A2DP_BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!a2dp_buffer) {
    Serial.println("Failed to allocate PCM buffer");
    while (true) delay(1000);
  }

  net_pool = (uint8_t*)heap_caps_malloc(NUM_BUFFERS * MAX_CHUNK_SIZE,
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!net_pool) {
    Serial.println("Failed to allocate net_pool");
    while (true) delay(1000);
  }
  for (int i = 0; i < NUM_BUFFERS; ++i) {
    netBuffers[i] = net_pool + i * MAX_CHUNK_SIZE;
  }


  setupI2S();
  clearAudioBuffers();

  esp_bt_dev_set_device_name("AT1053-Source");

  // A2DP: register first, then init
  ESP_ERROR_CHECK(esp_a2d_register_callback(bt_app_a2dp_callback));
  ESP_ERROR_CHECK(esp_a2d_source_register_data_callback(pcm_data_callback));
 
  // IMPORTANT: do NOT use ESP_ERROR_CHECK here; guard instead.
  esp_err_t r;

  r = esp_avrc_tg_register_callback(avrc_target_callback);
  if (r != ESP_OK && r != ESP_ERR_INVALID_STATE) {
    Serial.printf("esp_avrc_tg_register_callback err=%s\r\n", esp_err_to_name(r));
  }

  r = esp_avrc_tg_init();
  if (r != ESP_OK && r != ESP_ERR_INVALID_STATE) {
    Serial.printf("esp_avrc_tg_init err=%s\r\n", esp_err_to_name(r));
  }

  ESP_ERROR_CHECK(esp_a2d_source_init());

  // Helix callbacks (unchanged)
  mp3.setDataCallback(mp3dataCallback);
  aac.setDataCallback(aacDataCallback);
  
  // Auto-reconnect logic (unchanged)
  nvs_load_autorc(&g_auto_reconnect_enabled);
  g_have_last_bda = nvs_load_last_bda(g_last_bda);

  if (g_auto_reconnect_enabled && g_have_last_bda) {
    (void)a2dp_try_connect_last(1000);
    if (!a2dp_connected) a2dp_start_autorc_task();
  } 


  xTaskCreatePinnedToCore(httpFillTask, "HTTPFill", 8096, NULL, 1 , &httpTaskHandle, 1);
  xTaskCreatePinnedToCore(decodeTask, "MP3Decoder", 8096, NULL, 2, &decodeTaskHandle, 1);

  xTaskCreatePinnedToCore(i2sPlaybackTask, "I2SPlay", 4096, NULL, 3, &i2sTaskHandle, 1);
  vTaskSuspend(i2sTaskHandle);

  Serial.println("✅ Init OK\n▶️ Decoder Ready");

  //print_heaps();
}

// --- Stack + Heap Monitor Utilities ------------------------------------
extern TaskHandle_t httpTaskHandle;
extern TaskHandle_t decodeTaskHandle;
extern TaskHandle_t i2sTaskHandle;
extern TaskHandle_t tftUiTaskHandle;  // make sure it's assigned

static void dump_stacks() {
  // Pick correct FreeRTOS API for idle handles
  #if CONFIG_FREERTOS_NUMBER_OF_CORES > 1 && defined(xTaskGetIdleTaskHandleForCPU)
    TaskHandle_t idle0 = xTaskGetIdleTaskHandleForCPU(0);
    TaskHandle_t idle1 = xTaskGetIdleTaskHandleForCPU(1);
  #else
    TaskHandle_t idle0 = xTaskGetIdleTaskHandle();
    TaskHandle_t idle1 = nullptr;
  #endif

  struct { TaskHandle_t h; const char* n; } tasks[] = {
    { httpTaskHandle,   "HTTP"   },
    { decodeTaskHandle, "DECODE" },
    { i2sTaskHandle,    "I2S"    },
    { tftUiTaskHandle,  "TFTUI"  },
    { idle0,            "IDLE0"  },
    { idle1,            "IDLE1"  },
  };

  Serial.println(F("[STACK] High-water marks (bytes free @ worst):"));
  for (auto& t : tasks) {
    if (!t.h) continue;
    UBaseType_t words = uxTaskGetStackHighWaterMark(t.h);
    Serial.printf("  %-6s  min free %5u bytes  (prio=%u core=%d)\r\n",
                  t.n,
                  (unsigned)words * 4,
                  (unsigned)uxTaskPriorityGet(t.h),
                  (int)xTaskGetAffinity(t.h));
  }

  Serial.printf("[HEAP]  DRAM free=%u  largest=%u | PSRAM free=%u  largest=%u\r\n",
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
}

static inline void radio_poll_log_in_loop() {
  static uint32_t lastLogMs = 0;
  static uint32_t lastTick  = 0;
  static int      prevNetPct = 100;

  const uint32_t now = millis();
  if (now - lastLogMs < 1000) return;   // once per second
  lastLogMs = now;

  // Wi-Fi info
  int rssi   = WiFi.isConnected() ? WiFi.RSSI()   : 0;
  int ch     = WiFi.isConnected() ? WiFi.channel(): 0;

  int bw_mhz = 0;
#if defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR >= 4
  wifi_bandwidth_t bw;
  if (esp_wifi_get_bandwidth(WIFI_IF_STA, &bw) == ESP_OK)
    bw_mhz = (bw == WIFI_BW_HT40) ? 40 : 20;
#endif
  const char* bw_txt = (bw_mhz == 40 ? "HT40" : (bw_mhz == 20 ? "HT20" : "HT?"));

  // Buffers
  const int netPct = (int)((net_filled_slots() * 100U) / NUM_BUFFERS);
  const int pcmPct = pcm_buffer_percent();

  // HTTP throughput (kbps) since last tick
  const uint32_t tick = g_httpBytesTick;               // producer increments this
  const uint32_t deltaBytes = tick - lastTick;         // wrap-safe
  lastTick = tick;
  const uint32_t httpKbps = (deltaBytes * 8U) / 1000U;

  // Lag since last NET publish (wrap-safe; 0 means "unknown/not set yet")
  uint32_t netLag = 0;
  if (g_lastNetWriteMs != 0) netLag = now - g_lastNetWriteMs;

  // BT lag only if connected and we’ve ever pushed
  uint32_t btLag = 0;
  bool haveBtLag = false;
  if (g_a2dpConnected && g_lastA2dpPushMs != 0) {
    btLag = now - g_lastA2dpPushMs;  // wrap-safe
    haveBtLag = true;
  }

  // Heuristic “strain” flags
  const bool wifiHI =
      (netPct < 25) ||
      ((httpKbps < 80) && (netPct < prevNetPct)) ||
      ((netLag > 1500) && (netPct < 80));

  const bool btHI = (g_a2dpConnected && haveBtLag && btLag > 30);

  // Print safely, no temporary String().c_str() pointers
  if (haveBtLag) {
    Serial.printf("[RADIO] WiFi %ddBm ch%d %s strain=%s | BT %s strain=%s | NET=%d%% PCM=%d%% kbps=%u lag(net=%lums bt=%lums)\r\n",
      rssi, ch, bw_txt,
      (wifiHI ? "HI" : "OK"),
      (g_a2dpConnected ? "on" : "off"),
      (btHI ? "HI" : "OK"),
      netPct, pcmPct, httpKbps,
      (unsigned long)netLag, (unsigned long)btLag
    );
  } else {
    Serial.printf("[RADIO] WiFi %ddBm ch%d %s strain=%s | BT %s | NET=%d%% PCM=%d%% kbps=%u lag(net=%lums)\r\n",
      rssi, ch, bw_txt,
      (wifiHI ? "HI" : "OK"),
      (g_a2dpConnected ? "on" : "off"),
      netPct, pcmPct, httpKbps,
      (unsigned long)netLag
    );
  }

  prevNetPct = netPct;
}


void loop() {
  // ... your existing loop logic (tasks, handling, etc.) ...
 
  //lv_timer_handler();
  serial_process ();
  //lv_timer_handler();

  vTaskDelay(pdMS_TO_TICKS(20));  // low CPU usage
}
