/**
 * @file    camera_if.h
 * @brief   Abstract camera interface. Wraps the ST DCMIPP / IMX335 pipeline.
 *          Only this module knows about BSP / HAL; everything above is HW
 *          independent.
 */
#ifndef CAMERA_IF_H
#define CAMERA_IF_H

#include <stdint.h>
#include "app_types.h"

/**
 * Callback fired when a new frame is ready. The caller owns the buffer
 * until the callback returns. Do NOT store the pointer.
 */
typedef void (*camera_if_frame_cb_t)(const frame_t* f, void* user);

int  camera_if_init(void);
int  camera_if_start(camera_if_frame_cb_t cb, void* user);
void camera_if_stop(void);

/**
 * @brief Expose the pixel buffer and geometry to board glue (LCD attach).
 *        All pointers are valid after camera_if_init() returns 0.
 */
const uint8_t* camera_if_get_buffer(void);
void           camera_if_get_geometry(uint16_t* w, uint16_t* h, uint16_t* stride);

/** Monotonic microsecond counter used across the whole pipeline. */
uint32_t camera_if_now_us(void);

/** Monotonic millisecond counter. */
uint32_t camera_if_now_ms(void);

#endif /* CAMERA_IF_H */
