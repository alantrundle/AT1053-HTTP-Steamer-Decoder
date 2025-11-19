#include "albumart.h"
#include <TJpg_Decoder.h>

// 100x100 RGB565 buffer (20 KB)
static uint16_t s_albumart_pixels[ALBUMART_W * ALBUMART_H];

// LVGL image descriptor pointing at that buffer
static lv_image_dsc_t s_albumart_img;

// TJpg_Decoder output callback – must match SketchCallback:
// bool (*)(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
static bool albumart_tjpg_output(
    int16_t x, int16_t y,
    uint16_t w, uint16_t h,
    uint16_t* bitmap);

// -----------------------------------------------------------------------------
// Init
// -----------------------------------------------------------------------------
void albumart_init() {
    // Clear buffer to black
    memset(s_albumart_pixels, 0, sizeof(s_albumart_pixels));

    // Init LVGL image descriptor
    memset(&s_albumart_img, 0, sizeof(s_albumart_img));
    s_albumart_img.header.cf = LV_COLOR_FORMAT_RGB565;
    s_albumart_img.header.w  = ALBUMART_W;
    s_albumart_img.header.h  = ALBUMART_H;
    s_albumart_img.data_size = sizeof(s_albumart_pixels);
    s_albumart_img.data      = reinterpret_cast<const uint8_t*>(s_albumart_pixels);

    // TJpg_Decoder setup
    TJpgDec.setCallback(albumart_tjpg_output);
    TJpgDec.setJpgScale(1);   // default; we override per image
}

// -----------------------------------------------------------------------------
// Public getter
// -----------------------------------------------------------------------------
const lv_image_dsc_t* albumart_get_lvimg() {
    return &s_albumart_img;
}

// -----------------------------------------------------------------------------
// Decode from memory buffer
// -----------------------------------------------------------------------------
bool albumart_decode_from_memory(const uint8_t* jpeg, size_t len) {
    if (!jpeg || len == 0) {
        // Clear to black if no data
        memset(s_albumart_pixels, 0, sizeof(s_albumart_pixels));
        return false;
    }

    uint16_t w = 0, h = 0;
    if (!TJpgDec.getJpgSize(&w, &h, jpeg, (uint32_t)len)) {
        // Not a valid JPEG
        memset(s_albumart_pixels, 0, sizeof(s_albumart_pixels));
        return false;
    }

    // Decide a scale so that decoded size <= 100x100
    // Valid scale factors: 1, 2, 4, 8
    uint8_t scale = 1;
    while (((w / scale) > ALBUMART_W || (h / scale) > ALBUMART_H) && scale < 8) {
        scale *= 2;
    }
    TJpgDec.setJpgScale(scale);

    // Clear buffer (in case JPEG smaller than 100x100)
    memset(s_albumart_pixels, 0, sizeof(s_albumart_pixels));

    // Decode from memory using our callback into s_albumart_pixels
    TJpgDec.drawJpg(
        0, 0,                // x,y (ignored by our callback’s placement logic)
        jpeg,
        (uint32_t)len
    );

    return true;
}

// -----------------------------------------------------------------------------
// TJpg_Decoder output callback
// Called for each block of decoded RGB565 pixels.
// We copy/clip into the 100x100 buffer.
// -----------------------------------------------------------------------------
static bool albumart_tjpg_output(
    int16_t x, int16_t y,
    uint16_t w, uint16_t h,
    uint16_t* bitmap)
{
    // Convert to signed for math
    int16_t iw = (int16_t)w;
    int16_t ih = (int16_t)h;

    // Clip against our 100x100 target
    if (x >= ALBUMART_W || y >= ALBUMART_H) return true;
    if (x + iw <= 0 || y + ih <= 0)        return true;

    int16_t x_start = x < 0 ? 0 : x;
    int16_t y_start = y < 0 ? 0 : y;
    int16_t x_end   = (x + iw > ALBUMART_W)  ? ALBUMART_W  : (x + iw);
    int16_t y_end   = (y + ih > ALBUMART_H) ? ALBUMART_H : (y + ih);

    int16_t copy_w = x_end - x_start;
    int16_t copy_h = y_end - y_start;

    // Offset into source bitmap (bitmap is w*h, row-major)
    int16_t src_x_offset = x_start - x;
    int16_t src_y_offset = y_start - y;

    for (int16_t row = 0; row < copy_h; ++row) {
        int16_t src_row = src_y_offset + row;
        uint16_t* src = bitmap + (src_row * iw) + src_x_offset;

        int16_t dst_y = y_start + row;
        uint16_t* dst = s_albumart_pixels + (dst_y * ALBUMART_W) + x_start;

        memcpy(dst, src, (size_t)copy_w * sizeof(uint16_t));
    }

    return true; // continue decoding
}
