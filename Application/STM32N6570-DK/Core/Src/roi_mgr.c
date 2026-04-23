/**
 * @file    roi_mgr.c
 * @brief   ROI policy driven by tracking state.
 *
 *          SEARCH   : centered fixed rectangle, CFG_ROI_SEARCH_W/H.
 *          TRACK    : tight square around last result, CFG_ROI_TRACK_W/H.
 *          RECOVERY : starts at CFG_ROI_RECOVERY_INIT_*, grows by
 *                     CFG_ROI_RECOVERY_GROW_STEP per missed frame,
 *                     capped at CFG_ROI_RECOVERY_MAX_*.
 *
 *          Every ROI is clamped to [0, frame_size) so the detector can
 *          index without bounds checks.
 */
#include "roi_mgr.h"
#include "pupil_config.h"

#include <stdint.h>

static uint16_t s_frame_w = CFG_FRAME_WIDTH;
static uint16_t s_frame_h = CFG_FRAME_HEIGHT;

int roi_mgr_init(uint16_t frame_w, uint16_t frame_h)
{
    if (frame_w == 0 || frame_h == 0) return -1;
    s_frame_w = frame_w;
    s_frame_h = frame_h;
    return 0;
}

/* Public: force ROI inside the frame and round width to an even number.
 * Even width keeps RGB565 halfword reads aligned (2 bytes / pixel) and
 * gives loop unrolled by 4 an exact match when w is a multiple of 4. */
void roi_mgr_clamp_and_align(roi_t* r)
{
    if (!r) return;
    if (r->x < 0) r->x = 0;
    if (r->y < 0) r->y = 0;
    if (r->w == 0) r->w = 2;
    if (r->h == 0) r->h = 2;
    if (r->w & 1u) r->w &= (uint16_t)~1u;   /* round down to even */
    if (r->x + r->w > s_frame_w) {
        r->w = (uint16_t)(s_frame_w - r->x);
        if (r->w & 1u) r->w &= (uint16_t)~1u;
    }
    if (r->y + r->h > s_frame_h) {
        r->h = (uint16_t)(s_frame_h - r->y);
    }
}

/* Place a rectangle of size (w,h) centered on (cx,cy), clamped. */
static void place_centered(int32_t cx, int32_t cy,
                           uint16_t w, uint16_t h,
                           roi_t* out)
{
    if (w > s_frame_w) w = s_frame_w;
    if (h > s_frame_h) h = s_frame_h;

    int32_t x = cx - (int32_t)w / 2;
    int32_t y = cy - (int32_t)h / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + w > s_frame_w) x = (int32_t)s_frame_w - w;
    if (y + h > s_frame_h) y = (int32_t)s_frame_h - h;

    out->x = (int16_t)x;
    out->y = (int16_t)y;
    out->w = w;
    out->h = h;

    /* Final pass: even width for aligned RGB565 reads. */
    roi_mgr_clamp_and_align(out);
}

void roi_mgr_compute(tracking_state_t       state,
                     const pupil_result_t*  last,
                     uint16_t               miss_streak,
                     roi_t*                 out)
{
    if (!out) return;

    int32_t cx = (int32_t)s_frame_w / 2;
    int32_t cy = (int32_t)s_frame_h / 2;
    if (last && last->valid) {
        cx = last->cx;
        cy = last->cy;
    }

    switch (state) {
        case TRK_TRACK:
            place_centered(cx, cy,
                           CFG_ROI_TRACK_W, CFG_ROI_TRACK_H, out);
            break;

        case TRK_RECOVERY: {
            uint32_t w = CFG_ROI_RECOVERY_INIT_W +
                         (uint32_t)miss_streak * CFG_ROI_RECOVERY_GROW_STEP;
            uint32_t h = CFG_ROI_RECOVERY_INIT_H +
                         (uint32_t)miss_streak * CFG_ROI_RECOVERY_GROW_STEP;
            if (w > CFG_ROI_RECOVERY_MAX_W) w = CFG_ROI_RECOVERY_MAX_W;
            if (h > CFG_ROI_RECOVERY_MAX_H) h = CFG_ROI_RECOVERY_MAX_H;
            place_centered(cx, cy, (uint16_t)w, (uint16_t)h, out);
            break;
        }

        case TRK_SEARCH:
        default:
            place_centered((int32_t)s_frame_w / 2,
                           (int32_t)s_frame_h / 2,
                           CFG_ROI_SEARCH_W, CFG_ROI_SEARCH_H, out);
            break;
    }
}
