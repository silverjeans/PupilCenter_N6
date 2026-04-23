/**
 * @file    tracking_sm.h
 * @brief   SEARCH / TRACK / RECOVERY state machine.
 *
 *          Consumes per-frame hit/miss + the current detection and advances
 *          internal counters. roi_mgr reads the state; result_output
 *          annotates each log line with it.
 */
#ifndef TRACKING_SM_H
#define TRACKING_SM_H

#include <stdint.h>
#include "app_types.h"

typedef enum {
    TRK_SEARCH   = 0,
    TRK_TRACK    = 1,
    TRK_RECOVERY = 2,
} tracking_state_t;

int               tracking_sm_init(void);
void              tracking_sm_reset(void);

/** Call once per frame after filtering. det_valid = 1 if pupil is locked. */
void              tracking_sm_update(int det_valid, const pupil_result_t* det);

tracking_state_t  tracking_sm_get_state(void);

/** Number of consecutive misses / hits in the current state - useful for
 *  roi_mgr to grow RECOVERY ROI proportionally. */
uint16_t          tracking_sm_miss_streak(void);
uint16_t          tracking_sm_hit_streak(void);

#endif /* TRACKING_SM_H */
