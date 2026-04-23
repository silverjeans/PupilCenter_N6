/**
 * @file    pupil_filter.c
 * @brief   Integer EMA + jump reject for pupil center, radius, confidence.
 *
 *          EMA in Q0.8: new = (alpha_q8 * sample + (256 - alpha_q8) * state
 *                              + 128) >> 8
 *          Jump reject: if |raw - state| > CFG_FILTER_JUMP_LIMIT_PX the
 *          sample is treated as a transient spike - state is held and the
 *          returned record is marked invalid with confidence halved.
 */
#include "pupil_filter.h"
#include "pupil_config.h"

#include <string.h>

static struct {
    int initialized;
    int32_t  cx;
    int32_t  cy;
    int32_t  radius;
    int32_t  confidence;   /* 0..255 */
} s;

static inline int32_t abs_i32(int32_t v) { return v < 0 ? -v : v; }

static inline int32_t ema_u8(int32_t state, int32_t sample, int32_t alpha_q8)
{
    return (alpha_q8 * sample + (256 - alpha_q8) * state + 128) >> 8;
}

int pupil_filter_init(void)
{
    pupil_filter_reset();
    return 0;
}

void pupil_filter_reset(void)
{
    memset(&s, 0, sizeof s);
}

void pupil_filter_update(const pupil_result_t* raw, pupil_result_t* smoothed)
{
    if (!raw || !smoothed) return;

    *smoothed = *raw;

    if (!raw->valid) {
        /* No raw detection - hold the last smoothed pose but mark as stale. */
        if (s.initialized) {
            smoothed->cx         = (int16_t)s.cx;
            smoothed->cy         = (int16_t)s.cy;
            smoothed->radius_px  = (uint16_t)s.radius;
            smoothed->confidence = (uint8_t)(s.confidence >> 1);
            smoothed->valid      = 0;
        }
        return;
    }

    if (!s.initialized) {
        s.cx         = raw->cx;
        s.cy         = raw->cy;
        s.radius     = raw->radius_px;
        s.confidence = raw->confidence;
        s.initialized = 1;
    } else {
        /* Jump reject on (cx,cy) distance; use Chebyshev to stay int-only. */
        int32_t dx = abs_i32((int32_t)raw->cx - s.cx);
        int32_t dy = abs_i32((int32_t)raw->cy - s.cy);
        int32_t dmax = dx > dy ? dx : dy;
        if (CFG_FILTER_JUMP_LIMIT_PX > 0 && dmax > CFG_FILTER_JUMP_LIMIT_PX) {
            smoothed->cx         = (int16_t)s.cx;
            smoothed->cy         = (int16_t)s.cy;
            smoothed->radius_px  = (uint16_t)s.radius;
            smoothed->confidence = (uint8_t)(s.confidence >> 1);
            smoothed->valid      = 0;
            return;
        }

        const int32_t a = CFG_FILTER_ALPHA_Q8;
        s.cx         = ema_u8(s.cx,         raw->cx,         a);
        s.cy         = ema_u8(s.cy,         raw->cy,         a);
        s.radius     = ema_u8(s.radius,     raw->radius_px,  a);
        s.confidence = ema_u8(s.confidence, raw->confidence, a);
    }

    smoothed->cx         = (int16_t)s.cx;
    smoothed->cy         = (int16_t)s.cy;
    smoothed->radius_px  = (uint16_t)s.radius;
    smoothed->confidence = (uint8_t)s.confidence;
    smoothed->valid      = 1;
}
