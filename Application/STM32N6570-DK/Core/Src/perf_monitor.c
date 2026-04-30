/**
 * @file    perf_monitor.c
 * @brief   Full implementation: EMA latency, sliding-window FPS,
 *          ring-buffer valid-ratio, CPU occupancy estimate.
 *
 *  All arithmetic is integer-only (no FPU used here).
 *
 *  FPS is derived from the inter-frame wall-clock period (end of one frame
 *  to end of the next), stored as milli-Hz (fps * 1000) so a single uint32
 *  covers 0..4294967 mHz without overflow.
 *
 *  CPU occupancy estimate:
 *      occ_x100 = avg_latency_us * avg_fps_m / 10000
 *  (= avg_latency_us / frame_period_us * 100, rearranged to stay integer.)
 *  Capped at 100 % (10000 in x100 units).
 *
 *  Valid-ratio uses a power-of-two ring buffer (CFG_PERF_VALID_WINDOW)
 *  with a running sum so the per-frame cost is O(1).
 */
#include "perf_monitor.h"
#include "pupil_config.h"
#include "camera_if.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/* EMA alpha (Q0.8).  16/256 ≈ 0.063 → ~16-frame time constant.      */
/* ------------------------------------------------------------------ */
#define EMA_ALPHA_Q8  16u

static uint32_t ema_u32(uint32_t state, uint32_t sample, uint32_t alpha)
{
    return (alpha * sample + (256u - alpha) * state + 128u) >> 8;
}

/* ------------------------------------------------------------------ */
/* State                                                               */
/* ------------------------------------------------------------------ */
static perf_stat_t s_stat;

static uint32_t s_begin_us;         /* timestamp at perf_monitor_begin() */
static uint32_t s_last_end_us;      /* timestamp at previous perf_monitor_end() */
static int      s_have_prev;        /* 0 until the second frame */

/* Ring buffer for valid-frame ratio (power-of-two size). */
static uint8_t  s_vring[CFG_PERF_VALID_WINDOW];
static uint32_t s_vring_head;
static uint32_t s_vring_sum;        /* running count of '1' entries */

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */
int perf_monitor_init(void)
{
    perf_monitor_reset();
    return 0;
}

void perf_monitor_reset(void)
{
    memset(&s_stat,  0, sizeof s_stat);
    memset(s_vring,  0, sizeof s_vring);
    s_vring_head  = 0;
    s_vring_sum   = 0;
    s_begin_us    = 0;
    s_last_end_us = 0;
    s_have_prev   = 0;
}

void perf_monitor_begin(void)
{
    s_begin_us = camera_if_now_us();
}

void perf_monitor_end(int frame_valid)
{
    uint32_t now = camera_if_now_us();

    /* --- Latency --------------------------------------------------- */
    uint32_t lat = now - s_begin_us;

    /* DWT CYCCNT / s_cycles_per_us wraps at ~7.16 s on a 600 MHz core.
     * A wrap makes (now - s_begin_us) appear as ~4 billion µs, polluting
     * the EMA for several seconds.  Any value above 1 s is impossible for
     * a real frame; skip the sample and reset the FPS baseline. */
    if (lat > 1000000u) {
        s_have_prev   = 0;
        s_last_end_us = now;
        s_stat.frames_total++;
        if (frame_valid) s_stat.frames_valid++;
        return;
    }

    s_stat.last_latency_us = lat;
    s_stat.avg_latency_us  =
        (s_stat.frames_total == 0)
            ? lat
            : ema_u32(s_stat.avg_latency_us, lat, EMA_ALPHA_Q8);

    /* --- FPS (inter-frame period) ----------------------------------- */
    if (s_have_prev) {
        uint32_t period_us = now - s_last_end_us;
        if (period_us > 0u && period_us <= 1000000u) {
            /* fps_m = fps * 1000 = 1e9 / period_us */
            uint32_t fps_m = 1000000000u / period_us;
            s_stat.avg_fps_m =
                (s_stat.frames_total == 1u)
                    ? fps_m
                    : ema_u32(s_stat.avg_fps_m, fps_m, EMA_ALPHA_Q8);
        }
    }
    s_have_prev   = 1;
    s_last_end_us = now;

    /* --- Valid-ratio ring buffer ------------------------------------ */
    uint32_t idx = s_vring_head & (CFG_PERF_VALID_WINDOW - 1u);
    s_vring_sum -= s_vring[idx];
    s_vring[idx] = frame_valid ? 1u : 0u;
    s_vring_sum += s_vring[idx];
    s_vring_head++;

    /* Use actual window size until the ring is full. */
    uint32_t window = (s_stat.frames_total + 1u < (uint32_t)CFG_PERF_VALID_WINDOW)
                      ? (s_stat.frames_total + 1u)
                      : (uint32_t)CFG_PERF_VALID_WINDOW;
    s_stat.valid_ratio_x100 = (uint16_t)(s_vring_sum * 100u / window);

    /* --- CPU occupancy --------------------------------------------- */
    /* occ_x100 = avg_latency_us * avg_fps_m / 10000                  */
    /* Derivation: occ% = lat_us / period_us * 100                    */
    /*             period_us = 1e9 / fps_m                            */
    /*             occ% = lat_us * fps_m / 1e7                        */
    /* *100 for two decimal places: lat_us * fps_m / 100000           */
    if (s_stat.avg_fps_m > 0u) {
        uint32_t occ = s_stat.avg_latency_us / 100u * s_stat.avg_fps_m / 1000u;
        if (occ > 10000u) occ = 10000u;   /* cap at 100.00 % */
        s_stat.cpu_occ_x100 = (uint16_t)occ;
    }

    /* --- Totals ---------------------------------------------------- */
    s_stat.frames_total++;
    if (frame_valid) s_stat.frames_valid++;
}

const perf_stat_t* perf_monitor_get(void) { return &s_stat; }
