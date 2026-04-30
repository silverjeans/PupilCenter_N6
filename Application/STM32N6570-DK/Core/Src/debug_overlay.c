/**
 * @file    debug_overlay.c
 * @brief   LTDC Layer 2 ARGB4444 overlay for the pupil crosshair.
 *
 *  Strategy
 *  --------
 *  A static PSRAM buffer (s_ov_buf, ARGB4444, same dimensions as the camera
 *  frame) is shown on LTDC Layer 2, hardware alpha-blended over the RGB565
 *  camera layer.  All pixels start transparent (0x0000).
 *
 *  Per-frame protocol:
 *    begin_frame() — erase the previous crosshair (write 0x0000 to those
 *                    pixels) and clean their D-cache lines to PSRAM so LTDC
 *                    immediately stops showing the old mark.
 *    draw()        — write 0xFF00 (opaque red) to the new crosshair pixels
 *                    and record the position for the next begin_frame().
 *    end_frame()   — clean the new crosshair pixels' D-cache lines so LTDC
 *                    sees the new mark.
 *
 *  D-cache clean cost (HALF = 20, frame width = 800):
 *    - Horizontal arm : 1 call covering (2*HALF+1)*2 = 82 bytes.
 *    - Vertical arm   : (2*HALF+1) = 41 calls, each 2 bytes → 1 cache line.
 *    Total flushed: ~1.4 KB, vs 65 KB for the previous in-place approach.
 *
 *  ARGB4444 encoding (uint16_t, little-endian in memory):
 *    bits [15:12] = Alpha,  [11:8] = Red,  [7:4] = Green,  [3:0] = Blue
 *    Transparent : 0x0000  (Alpha = 0x0)
 *    Opaque red  : 0xFF00  (Alpha = 0xF, Red = 0xF, G = 0, B = 0)
 */
#include "debug_overlay.h"
#include "pupil_config.h"

#include <string.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* BSP shim — Core/ must not include ST HAL directly.                 */
/* bsp_lcd_flush_dcache wraps SCB_CleanDCache_by_Addr.                */
/* bsp_lcd_init_overlay_layer wraps BSP_LCD_ConfigLayer(LTDC_LAYER_2) */
/* ------------------------------------------------------------------ */
extern void bsp_lcd_flush_dcache(const void* addr, uint32_t len);
extern int  bsp_lcd_init_overlay_layer(uint32_t x0, uint32_t y0,
                                       uint32_t w,  uint32_t h,
                                       const uint16_t* buf);

/* ------------------------------------------------------------------ */
/* ARGB4444 colour constants                                           */
/* ------------------------------------------------------------------ */
#define OV_TRANSPARENT  ((uint16_t)0x0000u)
#define OV_RED          ((uint16_t)0xFF00u)  /* A=F R=F G=0 B=0 */

/* ------------------------------------------------------------------ */
/* Overlay pixel buffer — placed in external PSRAM.                   */
/* Maximum frame size the DK camera can deliver: 800 x 480 pixels.   */
/* ARGB4444: 2 bytes/pixel → 800*480*2 = 768 KB.                     */
/* ------------------------------------------------------------------ */
static uint16_t s_ov_buf[800u * 480u] __attribute__((section(".psram_bss")));

/* ------------------------------------------------------------------ */
/* Module state                                                        */
/* ------------------------------------------------------------------ */
static struct {
    uint16_t w, h;              /* overlay / frame dimensions          */
    uint8_t  attached;          /* 1 after debug_overlay_attach_layer  */

    /* Previous crosshair position — erased in next begin_frame().    */
    int16_t  prev_cx, prev_cy;
    int8_t   prev_half;
    uint8_t  have_prev;

    /* Current crosshair position — set in draw(), flushed in end_frame(). */
    int16_t  cur_cx, cur_cy;
    int8_t   cur_half;
    uint8_t  have_cur;
} s;

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

/** Write @p color to crosshair pixels centred on (cx,cy) with arm half-length. */
static void ov_paint_cross(int16_t cx, int16_t cy, int8_t half, uint16_t color)
{
    if (half <= 0) return;

    const uint16_t W = s.w;
    const uint16_t H = s.h;

    int x0 = cx - half; if (x0 < 0)        x0 = 0;
    int x1 = cx + half; if (x1 >= (int)W)  x1 = (int)W - 1;
    int y0 = cy - half; if (y0 < 0)        y0 = 0;
    int y1 = cy + half; if (y1 >= (int)H)  y1 = (int)H - 1;

    /* Horizontal arm — contiguous in memory */
    uint16_t* row = &s_ov_buf[(uint32_t)cy * W + (uint32_t)x0];
    for (int x = x0; x <= x1; ++x) *row++ = color;

    /* Vertical arm — stride = W pixels per row */
    uint16_t* col = &s_ov_buf[(uint32_t)y0 * W + (uint32_t)cx];
    for (int y = y0; y <= y1; ++y) { *col = color; col += W; }
}

/** Clean D-cache lines for crosshair pixels so LTDC reads updated PSRAM.
 *  Horizontal arm: single contiguous flush.
 *  Vertical arm  : one flush per row (rows are W*2 bytes apart → separate
 *                  cache lines, each flush hits exactly 1 cache line).    */
static void ov_flush_cross(int16_t cx, int16_t cy, int8_t half)
{
    if (half <= 0) return;

    const uint16_t W = s.w;
    const uint16_t H = s.h;

    int x0 = cx - half; if (x0 < 0)        x0 = 0;
    int x1 = cx + half; if (x1 >= (int)W)  x1 = (int)W - 1;
    int y0 = cy - half; if (y0 < 0)        y0 = 0;
    int y1 = cy + half; if (y1 >= (int)H)  y1 = (int)H - 1;

    /* Horizontal arm: flush [x0..x1] on row cy */
    bsp_lcd_flush_dcache(&s_ov_buf[(uint32_t)cy * W + (uint32_t)x0],
                         (uint32_t)(x1 - x0 + 1) * 2u);

    /* Vertical arm: flush one pixel per row */
    for (int y = y0; y <= y1; ++y) {
        bsp_lcd_flush_dcache(&s_ov_buf[(uint32_t)y * W + (uint32_t)cx], 2u);
    }
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

int debug_overlay_init(void)
{
    memset(&s, 0, sizeof s);
    return 0;
}

int debug_overlay_attach_layer(uint32_t screen_x0, uint32_t screen_y0,
                               uint32_t frame_w,   uint32_t frame_h)
{
    if (frame_w == 0 || frame_h == 0)           return -1;
    if (frame_w > 800u || frame_h > 480u)       return -2;

    s.w        = (uint16_t)frame_w;
    s.h        = (uint16_t)frame_h;
    s.have_prev = 0;
    s.have_cur  = 0;

    /* The buffer lives in .psram_bss (zero-initialised at startup).
     * Explicitly push zeros to PSRAM so LTDC sees a clean transparent
     * layer from the very first frame (guards against warm resets). */
    memset(s_ov_buf, 0, (size_t)frame_w * frame_h * 2u);
    bsp_lcd_flush_dcache(s_ov_buf, (uint32_t)frame_w * frame_h * 2u);

    int rc = bsp_lcd_init_overlay_layer(screen_x0, screen_y0,
                                        frame_w, frame_h,
                                        s_ov_buf);
    if (rc == 0) s.attached = 1;
    return rc;
}

void debug_overlay_set_view(const debug_overlay_view_t* view)
{
    /* View struct kept for API compatibility; geometry comes from attach. */
    (void)view;
}

void debug_overlay_begin_frame(void)
{
    if (!s.attached || !s.have_prev) return;

    /* Erase the crosshair drawn in the previous frame. */
    ov_paint_cross(s.prev_cx, s.prev_cy, s.prev_half, OV_TRANSPARENT);
    ov_flush_cross(s.prev_cx, s.prev_cy, s.prev_half);

    s.have_prev = 0;
}

void debug_overlay_draw(const roi_t* roi, const pupil_result_t* det)
{
    (void)roi;  /* ROI rect drawing reserved for future use */

    if (!s.attached) return;
    if (!det || !det->valid) {
        s.have_cur = 0;
        return;
    }

    const int8_t HALF = 20;

    ov_paint_cross(det->cx, det->cy, HALF, OV_RED);

    /* Record for end_frame() flush and next begin_frame() erase. */
    s.cur_cx   = det->cx;
    s.cur_cy   = det->cy;
    s.cur_half = HALF;
    s.have_cur = 1;
}

void debug_overlay_end_frame(void)
{
    if (!s.attached || !s.have_cur) return;

    /* Push the newly painted crosshair pixels to PSRAM. */
    ov_flush_cross(s.cur_cx, s.cur_cy, s.cur_half);

    /* Promote current → previous for next begin_frame(). */
    s.prev_cx   = s.cur_cx;
    s.prev_cy   = s.cur_cy;
    s.prev_half = s.cur_half;
    s.have_prev = 1;
    s.have_cur  = 0;
}

/* ================================================================== */
/* overlay_draw_cross: legacy in-place RGB crosshair (kept for        */
/* compatibility; not called when the ARGB overlay is active).        */
/* ================================================================== */

static inline void plot_rgb565(uint8_t* px, uint16_t v)
{
    px[0] = (uint8_t)(v & 0xFFu);
    px[1] = (uint8_t)(v >> 8);
}
static inline void plot_rgb888(uint8_t* px, uint32_t v)
{
    px[0] = (uint8_t)(v >> 16);
    px[1] = (uint8_t)(v >> 8);
    px[2] = (uint8_t)(v);
}

void overlay_draw_cross(frame_t* frame,
                        int16_t cx, int16_t cy,
                        uint8_t half_len,
                        uint32_t color)
{
    if (!frame || !frame->data || half_len == 0) return;
    if (cx < 0 || cy < 0) return;
    if (cx >= (int16_t)frame->width || cy >= (int16_t)frame->height) return;

    const uint16_t w      = frame->width;
    const uint16_t h      = frame->height;
    const uint16_t stride = frame->stride;

    int x0 = cx - half_len; if (x0 < 0)       x0 = 0;
    int x1 = cx + half_len; if (x1 >= (int)w) x1 = (int)w - 1;
    int y0 = cy - half_len; if (y0 < 0)       y0 = 0;
    int y1 = cy + half_len; if (y1 >= (int)h) y1 = (int)h - 1;

    if (frame->fmt == PIX_FMT_RGB565) {
        uint16_t c = (uint16_t)color;
        uint8_t* row = frame->data + (uint32_t)cy * stride + (uint32_t)x0 * 2u;
        for (int x = x0; x <= x1; ++x) { plot_rgb565(row, c); row += 2; }
        uint8_t* col = frame->data + (uint32_t)y0 * stride + (uint32_t)cx * 2u;
        for (int y = y0; y <= y1; ++y) { plot_rgb565(col, c); col += stride; }
        return;
    }
    if (frame->fmt == PIX_FMT_RGB888) {
        uint8_t* row = frame->data + (uint32_t)cy * stride + (uint32_t)x0 * 3u;
        for (int x = x0; x <= x1; ++x) { plot_rgb888(row, color); row += 3; }
        uint8_t* col = frame->data + (uint32_t)y0 * stride + (uint32_t)cx * 3u;
        for (int y = y0; y <= y1; ++y) { plot_rgb888(col, color); col += stride; }
        return;
    }
    if (frame->fmt == PIX_FMT_GRAY8) {
        uint8_t c = (uint8_t)color;
        uint8_t* row = frame->data + (uint32_t)cy * stride + x0;
        for (int x = x0; x <= x1; ++x) row[x - x0] = c;
        uint8_t* col = frame->data + (uint32_t)y0 * stride + cx;
        for (int y = y0; y <= y1; ++y) { *col = c; col += stride; }
    }
}
