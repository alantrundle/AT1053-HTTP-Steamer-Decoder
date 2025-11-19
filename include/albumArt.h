#pragma once

#include <Arduino.h>
#include <lvgl.h>

// Fixed album art size
constexpr int ALBUMART_W = 100;
constexpr int ALBUMART_H = 100;

/**
 * Initialise JPEG decoder for album art.
 * Call once from setup() after LVGL init.
 */
void albumart_init();

/**
 * Decode a JPEG (e.g. ID3 APIC image) into the internal 100x100 buffer.
 *
 * @param jpeg     Pointer to JPEG bytes
 * @param len      Length of JPEG data
 * @return true on success, false on decode failure
 */
bool albumart_decode_from_memory(const uint8_t* jpeg, size_t len);

/**
 * Get LVGL image descriptor for the current album art buffer.
 * You can pass this directly to lv_image_set_src().
 */
const lv_image_dsc_t* albumart_get_lvimg();
