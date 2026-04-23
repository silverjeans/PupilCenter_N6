/**
 * @file    app_types.h
 * @brief   Common data structures shared across PupilCenter modules.
 *
 * Coordinate convention (REPEATED HERE because it is easy to violate):
 *   Any (cx, cy) stored in pupil_result_t is ALWAYS in FULL-FRAME pixels.
 *   ROI-local coordinates only appear inside detector internals.
 *
 * Only the structs needed by pupil_detect are populated here; other modules
 * may add fields later. Keep this header free of HAL / CMSIS includes so
 * the file compiles on a host PC for unit tests.
 */
#ifndef PUPIL_APP_TYPES_H
#define PUPIL_APP_TYPES_H

#include <stdint.h>

/* =========================================================================
 * Pixel format of the camera buffer delivered to the pipeline.
 *   RGB565   : 2 bytes/pixel, low-endian per DCMIPP. G = bits 10..5 (6-bit).
 *   RGB888   : 3 bytes/pixel, byte order R,G,B.
 *   GRAY8    : 1 byte/pixel (legacy path, not used by the fast detector).
 * ========================================================================= */
typedef enum {
    PIX_FMT_RGB565 = 0,
    PIX_FMT_RGB888 = 1,
    PIX_FMT_GRAY8  = 2,
} pix_fmt_t;

/* =========================================================================
 * frame_t - one camera frame handed to the pipeline.
 * Only "data + geometry + timestamp" - no ownership semantics here;
 * frame_mgr is responsible for lifetime.
 * ========================================================================= */
typedef struct {
    uint8_t* data;      /* pointer to top-left pixel */
    uint16_t width;     /* full frame width  in pixels */
    uint16_t height;    /* full frame height in pixels */
    uint16_t stride;    /* bytes per row (may exceed width for alignment) */
    pix_fmt_t fmt;      /* pixel format of 'data' */
    uint32_t ts_ms;     /* capture timestamp in milliseconds */
    uint32_t seq;       /* monotonic frame number */
} frame_t;

/* =========================================================================
 * roi_t - rectangular region of interest in FULL-FRAME coordinates.
 * Always axis-aligned. Guaranteed by roi_mgr to fit within the frame.
 * ========================================================================= */
typedef struct {
    int16_t  x;         /* top-left x in full-frame pixels */
    int16_t  y;         /* top-left y */
    uint16_t w;
    uint16_t h;
} roi_t;

/* =========================================================================
 * pupil_result_t - output of one detection attempt.
 * valid=1 means cx/cy/radius are meaningful; otherwise treat them as stale.
 * ========================================================================= */
typedef struct {
    int16_t  cx;            /* pupil center x, FULL-FRAME coords */
    int16_t  cy;            /* pupil center y, FULL-FRAME coords */
    uint16_t radius_px;     /* equivalent radius in pixels */
    uint32_t area_px;       /* winning blob area */
    uint8_t  confidence;    /* 0..255; opaque score, higher = better */
    uint8_t  valid;         /* 0 or 1 */
    roi_t    roi_used;      /* ROI that produced this result */
} pupil_result_t;

#endif /* PUPIL_APP_TYPES_H */
