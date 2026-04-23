/**
 * @file    roi_mgr.h
 * @brief   Computes the ROI rectangle the detector should process next,
 *          given the current tracking state and the last detection.
 *
 *          All ROIs are clamped to the full frame by this module. Callers
 *          may assume the returned roi is safe to index.
 */
#ifndef ROI_MGR_H
#define ROI_MGR_H

#include <stdint.h>
#include "app_types.h"
#include "tracking_sm.h"

int  roi_mgr_init(uint16_t frame_w, uint16_t frame_h);

/**
 * @brief Clamp and align a raw ROI to the frame.
 *        Width is forced even so RGB565 halfword access stays aligned
 *        and loop-unrolled inner loops see a multiple-of-4 pixel count
 *        when the input is well-sized.
 */
void roi_mgr_clamp_and_align(roi_t* roi);

void roi_mgr_compute(tracking_state_t       state,
                     const pupil_result_t*  last_result,
                     uint16_t               miss_streak,
                     roi_t*                 out);

#endif /* ROI_MGR_H */
