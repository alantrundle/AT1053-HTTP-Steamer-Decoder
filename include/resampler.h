#pragma once
#include <stdint.h>
#include <stddef.h>

/**
 * Stateful stereo linear resampler for converting any input sample rate
 * (typically 32000, 44100, 48000) into fixed 48000 Hz output.
 *
 * This version keeps fractional position and previous samples between calls
 * so the stream remains continuous and stable.
 */
typedef struct {
    float pos;      // fractional input index
    float lastL;    // last left sample from previous block (for continuity)
    float lastR;    // last right sample
    bool  primed;   // have we received the first block?
} PCM48ResamplerState;

/**
 * Reset the state before starting a new track/session.
 */
static inline void pcm48_resampler_reset(PCM48ResamplerState *rs) {
    rs->pos   = 0.0f;
    rs->lastL = 0.0f;
    rs->lastR = 0.0f;
    rs->primed = false;
}

/**
 * Perform stereo linear resampling to 48k output.
 *
 * @param rs   Pointer to state
 * @param in   Input stereo PCM (int16, interleaved)
 * @param in_frames Number of input FRAMES (not samples)
 * @param in_sr Source sample rate
 * @param out  Output stereo PCM (int16, interleaved)
 * @param out_cap_frames Max output frames available
 *
 * @return Number of output frames produced
 */
size_t pcm48_resample_block(
    PCM48ResamplerState *rs,
    const int16_t *in,
    size_t in_frames,
    uint32_t in_sr,
    int16_t *out,
    size_t out_cap_frames
);
