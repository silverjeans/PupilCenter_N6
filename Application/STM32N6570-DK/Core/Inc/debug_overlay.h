/**
 * @file    debug_overlay.h
 * @brief   LCD overlay interface for development. Draws the current ROI,
 *          pupil center crosshair and radius circle.
 *
 *          Callers must bracket per-frame drawing with begin_frame() and
 *          end_frame() - the underlying layer is double-buffered, and the
 *          wrapper uses those calls to flip.
 *
 *          Coordinate inputs are FULL-FRAME pixels. This module remaps them
 *          to LCD screen pixels internally using the view transform set by
 *          debug_overlay_set_view().
 */
#ifndef DEBUG_OVERLAY_H
#define DEBUG_OVERLAY_H

#include <stdint.h>
#include "app_types.h"

typedef struct {
    uint16_t frame_w;     /* source frame width in pixels */
    uint16_t frame_h;     /* source frame height in pixels */
    uint16_t screen_x0;   /* top-left of preview on the LCD */
    uint16_t screen_y0;
    uint16_t screen_w;    /* preview size on the LCD */
    uint16_t screen_h;
} debug_overlay_view_t;

int  debug_overlay_init(void);

/**
 * @brief Wire LTDC Layer 2 to the PSRAM overlay buffer and record geometry.
 *
 *        Call once, after bsp_lcd_attach_camera_layer() and after
 *        camera_if_get_geometry() has returned the real frame dimensions.
 *
 * @param screen_x0  Left edge of the camera preview on the LCD panel.
 * @param screen_y0  Top  edge of the camera preview on the LCD panel.
 * @param frame_w    Camera frame width  in pixels (= overlay buffer width).
 * @param frame_h    Camera frame height in pixels (= overlay buffer height).
 * @return 0 on success, negative on error.
 */
int  debug_overlay_attach_layer(uint32_t screen_x0, uint32_t screen_y0,
                                uint32_t frame_w,   uint32_t frame_h);

/** Update the frame <-> screen mapping used by all subsequent draw calls. */
void debug_overlay_set_view(const debug_overlay_view_t* view);

/** Start a new overlay frame (clears the drawing layer). */
void debug_overlay_begin_frame(void);

/** Draw ROI rectangle, pupil crosshair and radius circle. */
void debug_overlay_draw(const roi_t* roi, const pupil_result_t* det);

/** Publish the current overlay frame (layer swap / cache flush). */
void debug_overlay_end_frame(void);

/* ----------------------------------------------------------------------- */
/* In-place draw helpers that mutate the RGB camera frame directly.        */
/* Use this path when there is no separate overlay layer and speed matters */
/* more than preserving pixel data. Only a handful of pixels are touched.  */
/* ----------------------------------------------------------------------- */

/**
 * @brief Draw a small crosshair centered on (cx,cy) straight into the RGB
 *        frame buffer.
 *
 *        RGB565 and RGB888 are supported. "color" is encoded per format:
 *          - RGB565: 16-bit packed value (little-endian in memory).
 *          - RGB888: 0x00RRGGBB (low 24 bits used).
 *
 * @param frame       destination frame; its pixel buffer is mutated.
 * @param cx,cy       center in FULL-FRAME pixels. Out-of-range is skipped.
 * @param half_len    arm length in pixels (total arm = 2*half_len + 1).
 * @param color       packed color value, see above.
 */
void overlay_draw_cross(frame_t* frame,
                        int16_t cx, int16_t cy,
                        uint8_t half_len,
                        uint32_t color);

#endif /* DEBUG_OVERLAY_H */
