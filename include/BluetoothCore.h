#pragma once
#include <Arduino.h>
#include <esp_bt.h>
#include <esp_gap_bt_api.h>
#include <esp_a2dp_api.h>
#include <esp_avrc_api.h>

#include "globals.h"

// -----------------------------
//  Bluetooth State Machine
// -----------------------------
enum UiState {
    UI_IDLE = 0,
    UI_MENU,
    UI_SCANNING,
    UI_CONNECTING,
    UI_STREAMING
};

// -----------------------------
//  Device Entry
// -----------------------------
struct BTDevice {
    uint8_t bda[6];
    char    name[64];
    int     rssi;
    bool    ever_emitted;
    int     last_emit_rssi;
};

// Max number of discovered devices
static const int MAX_BT_DEVICES = 32;


// ===============================
//   BLUETOOTH CORE - PUBLIC API
// ===============================
namespace BluetoothCore {

// ---- Device List API ----
void clear_devices();
int  get_device_count();
BTDevice get_device(int idx);

// ---- BT Classic Control ----
void start_bluetooth_classic();
void stop_bluetooth_classic();
void start_scan(uint32_t seconds = 8);
void stop_scan();

// ---- Connect / Reconnect ----
BtConnectionStatus connect_direct_mac(const uint8_t* mac);
bool a2dp_try_connect_last(uint32_t timeout_ms);

// ---- NVS helpers ----
bool nvs_load_last_bda(uint8_t out[6]);
bool nvs_save_last_bda(const uint8_t bda[6]);
void nvs_forget_last_bda();

bool nvs_load_autorc(bool* out);
void nvs_save_autorc(bool en);

// ---- Utility ----
bool parse_mac(const char* s, uint8_t out[6]);
void print_addr(const uint8_t bda[6]);

// ---- Auto-Reconnect ----
void a2dp_start_autorc_task();
void a2dp_stop_autorc_task();

// ---- Callbacks exposed for main.cpp ----
void raw_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param);
void onA2DPConnectState(esp_a2d_connection_state_t state, void*);

// ---- Worker Task (same behaviour as original) ----
void bt_worker_task(void* arg);

// Global UI state (exposed to main)
extern UiState ui_state;

// Device buffer exposed globally now (per your instruction)
extern BTDevice g_devices[MAX_BT_DEVICES];
extern int g_device_count;

} // namespace BluetoothCore
