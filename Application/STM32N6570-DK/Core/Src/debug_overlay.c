/**
 * @file    debug_overlay.c
 * @brief   In-place RGB overlay (fast path) + LCD layer hooks (TODO).
 *
 *          The hot path is overlay_draw_cross(): it mutates a few pixels
 *          of the camera frame in place. No grayscale conversion; the RGB
 *          buffer shown on the LCD is preserved.
 */
#include "debug_overlay.h"

#include <string.h>

static debug_overlay_view_t s_view;

/* ------------------------------------------------------------------ */
/* HW-backed overlay layer hooks (to be wired to LTDC Layer 2)         */
/* ------------------------------------------------------------------ */
int debug_overlay_init(void)
{
    memset(&s_view, 0, sizeof s_view);
    return 0;
}
void debug_overlay_set_view(const debug_overlay_view_t* view)
{
    if (view) s_view = *view;
}
void debug_overlay_begin_frame(void)   { /* TODO(hw): clear ARGB overlay */ }
void debug_overlay_draw(const roi_t* roi, const pupil_result_t* det)
{
    (void)roi; (void)det;
    /* TODO(hw): draw ROI rect + crosshair + circle on the overlay layer. */
}
void debug_overlay_end_frame(void)     { /* TODO(hw): swap / cache flush */ }

/* ================================================================== */
/* overlay_draw_cross: in-place RGB crosshair.                        */
/*                                                                    */
/* Touches at most (4*half_len + 1) pixels. Each pixel costs one      */
/* store, no multiply inside the inner loops.                         */
/* ================================================================== */

static inline void plot_rgb565(uint8_t* px, uint16_t v)
{
    px[0] = (uint8_t)(v & 0xFFu);
    px[1] = (uint8_t)(v >> 8);
}
static inline void plot_rgb888(uint8_t* px, uint32_t v)
{
    px[0] = (uint8_t)(v >> 16);  /* R */
    px[1] = (uint8_t)(v >> 8);   /* G */
    px[2] = (uint8_t)(v);        /* B */
}

void overlay_draw_cross(frame_t* frame,
                        int16_t cx, int16_t cy,
                        uint8_t half_len,
                        uint32_t color)
{
    if (!frame || !frame->data || half_len == 0) return;
    if (cx < 0 || cy < 0) return;
    if (cx >= (int16_t)frame->width || cy >= (int16_t)frame->height) return;

    const uint16_t w = frame->width;
    const uint16_t h = frame->height;
    const uint16_t stride = frame->stride;

    int x0 = cx - half_len; if (x0 < 0) x0 = 0;
    int x1 = cx + half_len; if (x1 >= w) x1 = w - 1;
    int y0 = cy - half_len; if (y0 < 0) y0 = 0;
    int y1 = cy + half_len; if (y1 >= h) y1 = h - 1;

    if (frame->fmt == PIX_FMT_RGB565) {
        uint16_t c = (uint16_t)color;

        /* Horizontal arm */
        uint8_t* row = frame->data + (uint32_t)cy * stride + (uint32_t)x0 * 2u;
        for (int x = x0; x <= x1; ++x) {
            plot_rgb565(row, c);
            row += 2;
        }
        /* Vertical arm */
        uint8_t* col = frame->data + (uint32_t)y0 * stride + (uint32_t)cx * 2u;
        for (int y = y0; y <= y1; ++y) {
            plot_rgb565(col, c);
            col += stride;
        }
        return;
    }

    if (frame->fmt == PIX_FMT_RGB888) {
        /* Horizontal arm */
        uint8_t* row = frame->data + (uint32_t)cy * stride + (uint32_t)x0 * 3u;
        for (int x = x0; x <= x1; ++x) {
            plot_rgb888(row, color);
            row += 3;
        }
        /* Vertical arm */
        uint8_t* col = frame->data + (uint32_t)y0 * stride + (uint32_t)cx * 3u;
        for (int y = y0; y <= y1; ++y) {
            plot_rgb888(col, color);
            col += stride;
        }
        return;
    }

    if (frame->fmt == PIX_FMT_GRAY8) {
        uint8_t c = (uint8_t)color;
        uint8_t* row = frame->data + (uint32_t)cy * stride + x0;
        for (int x = x0; x <= x1; ++x) row[x - x0] = c;
        uint8_t* col = frame->data + (uint32_t)y0 * stride + cx;
        for (int y = y0; y <= y1; ++y) { *col = c; col += stride; }
        return;
    }
}
