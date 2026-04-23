/*
 * sim_api.c - thin C ABI over pupil_detect for the Python simulator.
 *
 * Builds as a Windows DLL via MinGW. Exposes two symbols:
 *   sim_configure() - install cfg into pupil_detect_init
 *   sim_run_gray8() - run the full pipeline on an 8-bit grayscale frame
 *                     (Python converts IR JPEG -> uint8 once and passes
 *                      the raw buffer; we call pupil_detect_run with
 *                      PIX_FMT_GRAY8 so no RGB565 packing is needed)
 */
#include <stdint.h>
#include <string.h>

#include "../Application/STM32N6570-DK/Core/Inc/app_types.h"
#include "../Application/STM32N6570-DK/Core/Inc/pupil_detect.h"

#define SIM_EXPORT __declspec(dllexport)

SIM_EXPORT int sim_configure(uint8_t  dark_threshold,
                             uint32_t min_area_px,
                             uint32_t max_area_px,
                             uint8_t  center_bias_x100,
                             uint16_t max_blobs)
{
    pupil_cfg_t pc;
    pc.dark_threshold   = dark_threshold;
    pc.min_area_px      = min_area_px;
    pc.max_area_px      = max_area_px;
    pc.center_bias_x100 = center_bias_x100;
    pc.max_blobs        = max_blobs;
    return pupil_detect_init(&pc);
}

/* One-shot detection on a full-frame grayscale image.
 *
 * Inputs:
 *   data, w, h       - 8-bit grayscale frame (row-major, stride == w)
 *   roi_x..roi_h     - ROI in full-frame coords (caller-clamped)
 * Outputs (all pointers optional):
 *   out_cx, out_cy   - pupil center in full-frame pixels
 *   out_radius       - equivalent radius
 *   out_area         - winning blob area
 *   out_conf         - 0..255 score
 * Returns 1 on successful detection, 0 otherwise.
 */
SIM_EXPORT int sim_run_gray8(const uint8_t* data,
                             uint16_t       w,
                             uint16_t       h,
                             int16_t        roi_x,
                             int16_t        roi_y,
                             uint16_t       roi_w,
                             uint16_t       roi_h,
                             int16_t*       out_cx,
                             int16_t*       out_cy,
                             uint16_t*      out_radius,
                             uint32_t*      out_area,
                             uint8_t*       out_conf)
{
    frame_t f;
    f.data   = (uint8_t*)data;
    f.width  = w;
    f.height = h;
    f.stride = w;               /* GRAY8 = 1 byte/pixel */
    f.fmt    = PIX_FMT_GRAY8;
    f.ts_ms  = 0;
    f.seq    = 0;

    roi_t r = { roi_x, roi_y, roi_w, roi_h };

    pupil_result_t res;
    int hit = pupil_detect_run(&f, &r, &res);

    if (out_cx)     *out_cx     = res.cx;
    if (out_cy)     *out_cy     = res.cy;
    if (out_radius) *out_radius = res.radius_px;
    if (out_area)   *out_area   = res.area_px;
    if (out_conf)   *out_conf   = res.confidence;
    return (hit && res.valid) ? 1 : 0;
}
