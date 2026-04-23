/**
 * @file    pupil_detect.h
 * @brief   Fast classical-CV pupil detector operating on the G channel
 *          of an RGB frame, ROI-local, with in-place threshold + flood-fill
 *          connected components. No grayscale conversion of the full frame.
 *
 * Pipeline (per pupil_detect_run):
 *   1. Walk ROI once; for every pixel read G directly from the RGB frame
 *      (RGB565 or RGB888) and write a binary dark-mask entry to the
 *      internal ROI-local buffer.
 *   2. Flood-fill from each unvisited dark pixel to extract up to
 *      CFG_DETECT_MAX_BLOBS blobs, accumulating area / bbox / moments.
 *   3. Score each blob by area range + proximity to the ROI center.
 *   4. Convert the winner's ROI-local moments to FULL-FRAME coordinates
 *      and fill pupil_result_t.
 *
 * Coordinate rule:
 *   - All internal stages use ROI-local (x, y).
 *   - pupil_result_t.cx / cy returned on the public API are FULL-FRAME.
 *     The offset add happens in one place, in pupil_detect_run().
 */
#ifndef PUPIL_DETECT_H
#define PUPIL_DETECT_H

#include <stdint.h>
#include "app_types.h"
#include "pupil_config.h"

/* Maximum supported ROI. All static buffers sized for this. */
#define PUPIL_DETECT_MAX_ROI_W   256
#define PUPIL_DETECT_MAX_ROI_H   256
#define PUPIL_DETECT_MAX_PIXELS  (PUPIL_DETECT_MAX_ROI_W * PUPIL_DETECT_MAX_ROI_H)

/* Number of blob candidates tracked simultaneously. Small on purpose. */
#ifndef CFG_DETECT_MAX_BLOBS
#define CFG_DETECT_MAX_BLOBS     10
#endif

typedef struct {
    uint8_t  dark_threshold;   /* G value (0..255) <= this is "dark" */
    uint32_t min_area_px;
    uint32_t max_area_px;
    uint16_t center_bias_x100; /* 0..100; 0 = no bias, 100 = area/dist only */
    uint16_t max_blobs;        /* <= CFG_DETECT_MAX_BLOBS */
} pupil_cfg_t;

/* ROI-local blob statistics. */
typedef struct {
    uint32_t area;
    uint32_t sum_x;
    uint32_t sum_y;
    uint16_t bbox_x0, bbox_y0;
    uint16_t bbox_x1, bbox_y1;   /* inclusive */
} blob_stat_t;

int  pupil_detect_init(const pupil_cfg_t* cfg);

/**
 * @brief Full ROI -> pupil center path, no full-frame conversion.
 *
 * @return 1 if a valid candidate was found, 0 otherwise. out is always
 *         populated (valid=0 and roi_used set on failure).
 */
int  pupil_detect_run(const frame_t* frame,
                      const roi_t*   roi,
                      pupil_result_t* out);

/* ------------------------------------------------------------------ */
/* Stage-level entry points - exposed for PC unit tests and for the   */
/* overlay module that reuses the mask. All ROI-local.                */
/* ------------------------------------------------------------------ */

/**
 * @brief Build the ROI-local dark mask by reading G directly from an RGB
 *        frame. No temporary grayscale buffer is created.
 *
 * @param frame     source frame (fmt must be RGB565 or RGB888)
 * @param roi       rectangle inside the frame (already clamped)
 * @param thr       G threshold, pixels with G <= thr become mask=1
 * @param dst_mask  output buffer of roi.w * roi.h bytes (0 or 1)
 * @return 0 on success, negative if fmt is unsupported.
 */
int  threshold_dark_region_g(const frame_t* frame,
                             const roi_t*   roi,
                             uint8_t        thr,
                             uint8_t*       dst_mask);

/**
 * @brief Flood-fill connected-component extractor for a 0/1 mask.
 *        Uses a small explicit stack; no recursion.
 *
 * @param mask       dark mask of size w*h (modified in place: visited=0)
 * @param w,h        mask dimensions (= ROI dimensions)
 * @param stats      output array, capacity >= max_blobs
 * @param max_blobs  upper bound on blobs to extract (<=CFG_DETECT_MAX_BLOBS)
 * @param n_blobs    number of blobs actually extracted
 * @return 0 on success, -1 if max_blobs was reached (remaining ignored).
 */
int  find_connected_components_floodfill(uint8_t* mask,
                                         uint16_t w, uint16_t h,
                                         blob_stat_t* stats,
                                         uint16_t max_blobs,
                                         uint16_t* n_blobs);

/**
 * @brief Pick the best pupil candidate by area + center proximity.
 *        score = area_weight(area) * center_weight(dist_to_center).
 *
 * @return 1 if a candidate survives the area gate, 0 otherwise.
 */
int  select_best_pupil_candidate(const blob_stat_t* stats,
                                 uint16_t n_blobs,
                                 uint16_t roi_w,
                                 uint16_t roi_h,
                                 const pupil_cfg_t* cfg,
                                 uint16_t* best_idx,
                                 uint8_t*  best_score);

/** Convert a blob's ROI-local moments into a full-frame pupil_result_t. */
void compute_centroid(const blob_stat_t* stat,
                      const roi_t*       roi,
                      uint8_t            score,
                      pupil_result_t*    out);

#endif /* PUPIL_DETECT_H */
