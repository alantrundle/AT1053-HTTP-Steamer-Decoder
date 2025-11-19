#include "resampler.h"
#include <math.h>
#include <string.h>

//
// NOTE ABOUT RATIO:
//
// To convert → 48 kHz:
//
//     ratio = input_sr / 48000.0f
//
// Example:
//     44100 → 48000 = 0.91875
//     32000 → 48000 = 0.66666
//     48000 → 48000 = 1.0
//

size_t pcm48_resample_block(
    PCM48ResamplerState *rs,
    const int16_t *in,
    size_t in_frames,
    uint32_t in_sr,
    int16_t *out,
    size_t out_cap_frames
){
    if (!in || !out || in_frames < 1) return 0;

    const float ratio = (float)in_sr / 48000.0f;
    //const float ratio = 48000.0f / (float)in_sr;
    float p = rs->pos;
    size_t out_frames = 0;

    // Handle first-call priming with lastL/lastR
    float prevL = rs->lastL;
    float prevR = rs->lastR;

    // Loop until we fill output or exhaust input
    while (out_frames < out_cap_frames) {

        size_t idx = (size_t)p;
        float frac = p - (float)idx;

        float L, R;

        if (!rs->primed) {
            // First output frame uses previous-sample continuity
            if (idx >= in_frames) break;
            float curL = (float)in[idx * 2];
            float curR = (float)in[idx * 2 + 1];

            L = prevL + (curL - prevL) * frac;
            R = prevR + (curR - prevR) * frac;
        } 
        else {
            if (idx + 1 >= in_frames) break;

            float aL = (float)in[ idx    * 2 ];
            float bL = (float)in[(idx+1) * 2 ];
            float aR = (float)in[ idx    * 2 + 1 ];
            float bR = (float)in[(idx+1) * 2 + 1 ];

            L = aL + (bL - aL) * frac;
            R = aR + (bR - aR) * frac;
        }

        out[out_frames * 2]     = (int16_t)L;
        out[out_frames * 2 + 1] = (int16_t)R;

        out_frames++;
        p += ratio;
    }

    // Save fractional position
    rs->pos = p - (float)in_frames;

    // Save last samples for continuity
    if (in_frames > 0) {
        rs->lastL = (float)in[(in_frames - 1) * 2];
        rs->lastR = (float)in[(in_frames - 1) * 2 + 1];
    }

    rs->primed = true;
    return out_frames;
}
