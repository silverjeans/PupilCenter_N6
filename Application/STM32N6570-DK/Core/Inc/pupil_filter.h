/**
 * @file    pupil_filter.h
 * @brief   Temporal smoothing + spike rejection for the pupil center.
 *
 *          Takes a raw detection and produces a smoothed one. Smooths
 *          cx, cy and radius so overlays and downstream motor commands
 *          do not jitter; rejects samples whose distance from the current
 *          state exceeds CFG_FILTER_JUMP_LIMIT_PX.
 */
#ifndef PUPIL_FILTER_H
#define PUPIL_FILTER_H

#include <stdint.h>
#include "app_types.h"

int  pupil_filter_init(void);
void pupil_filter_reset(void);

/** Updates internal state with the raw result and returns the smoothed one.
 *  If the raw sample is rejected (spike or invalid), the returned record
 *  carries the last smoothed position with valid=0 and confidence reduced. */
void pupil_filter_update(const pupil_result_t* raw, pupil_result_t* smoothed);

#endif /* PUPIL_FILTER_H */
