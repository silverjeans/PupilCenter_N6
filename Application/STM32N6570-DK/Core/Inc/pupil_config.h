/**
 * @file    app_config.h
 * @brief   PupilCenter_N6 - project-wide tunable parameters.
 *
 * All magic numbers should live here. Other source files pull knobs from
 * this header only, so tuning happens in a single, reviewable location.
 *
 * This file is consumed by our own Core/ modules. The ST reference example
 * has its own app_config.h in Inc/ for camera / display plumbing; we keep
 * that file untouched.
 *
 * Include guard name is PUPIL_APP_CONFIG_H to avoid clashing with the ST
 * example's APP_CONFIG_H guard.
 */
#ifndef PUPIL_APP_CONFIG_H
#define PUPIL_APP_CONFIG_H

/* =========================================================================
 * 1. Frame geometry
 * ========================================================================= */

/** Full camera frame delivered to our pipeline (grayscale). */
#define CFG_FRAME_WIDTH              800u
#define CFG_FRAME_HEIGHT             480u

/** How many frame buffers frame_mgr maintains (ping-pong). */
#define CFG_FRAME_BUFFER_COUNT       2u

/* =========================================================================
 * 2. ROI policy
 * ========================================================================= */

/** SEARCH state ROI. Centered on the frame. Must stay <= detector's
 *  PUPIL_DETECT_MAX_ROI_W/H (256) so detection isn't short-circuited.
 *  With a 14 mm FOV and forehead-rest, the pupil is always near center
 *  so 256x256 is plenty. */
#define CFG_ROI_SEARCH_W             256u
#define CFG_ROI_SEARCH_H             256u

/** TRACK state ROI. Must fit the full pupil (~250 px diameter) plus slack
 *  for saccades; kept under detector's MAX_ROI (256). */
#define CFG_ROI_TRACK_W              240u
#define CFG_ROI_TRACK_H              240u

/** RECOVERY ROI starts at this size and grows each missed frame. */
#define CFG_ROI_RECOVERY_INIT_W      224u
#define CFG_ROI_RECOVERY_INIT_H      224u
#define CFG_ROI_RECOVERY_GROW_STEP   32u   /* add per failing frame */
#define CFG_ROI_RECOVERY_MAX_W       CFG_ROI_SEARCH_W
#define CFG_ROI_RECOVERY_MAX_H       CFG_ROI_SEARCH_H

/* =========================================================================
 * 3. Detection (classical CV)
 * ========================================================================= */

/** Fixed upper bound on dark-pixel gray value when thresholding the pupil.
 *  G channel is shifted up to 0..252 by g8_from_rgb565; a 14 mm FOV close-up
 *  pupil sits around 90-130 after ISP auto-exposure, so 130 lets the whole
 *  pupil through while excluding the iris. */
#define CFG_DETECT_DARK_THRESHOLD    130u

/** Area filter in pixels, applied in ROI coordinates. Eyelash / eyebrow
 *  shadow blobs measure ~20-25k on the 480x480 preview, so min_area is
 *  set above that floor to exclude them. The close-up pupil is 50-80k, so
 *  the lower bound still leaves comfortable margin. */
#define CFG_DETECT_MIN_AREA_PX       30000u
#define CFG_DETECT_MAX_AREA_PX       150000u

/** Minimum circularity (4*pi*area / perim^2) * 100. 0 disables the check. */
#define CFG_DETECT_MIN_CIRCULARITY_X100   30

/** Maximum labels the connected-components pass supports. The 2-pass
 *  algorithm uses a uint16 label map, but the union-find table has this
 *  static size - raise only if many dark spots coexist. */
#define CFG_DETECT_MAX_LABELS        256u

/* =========================================================================
 * 4. Filter
 * ========================================================================= */

/** EMA weight for the new sample, as a Q0.8 integer (0..256).
 *  alpha = CFG_FILTER_ALPHA_Q8 / 256.0 */
#define CFG_FILTER_ALPHA_Q8          90

/** Reject a new sample whose distance from the smoothed state exceeds
 *  this many pixels (treated as transient spike). 0 disables jump reject. */
#define CFG_FILTER_JUMP_LIMIT_PX     60

/* =========================================================================
 * 5. State machine thresholds
 * ========================================================================= */

#define CFG_SM_LOCK_HITS             3   /* SEARCH   -> TRACK    */
#define CFG_SM_TRACK_MISS_TO_RECOVER 1   /* TRACK    -> RECOVERY */
#define CFG_SM_RECOVER_HITS          2   /* RECOVERY -> TRACK    */
#define CFG_SM_RECOVER_MISS_TO_LOST  10  /* RECOVERY -> SEARCH   */

/* =========================================================================
 * 6. Output
 * ========================================================================= */

#define CFG_UART_BAUDRATE            115200u

/** 1 = ASCII human-readable log, 0 = binary frame. */
#define CFG_OUTPUT_ASCII_LOG         1

/** 1 = also emit binary frame regardless of ASCII setting. */
#define CFG_OUTPUT_BINARY_FRAME      0

/* =========================================================================
 * 7. Performance / logging
 * ========================================================================= */

/** Frames over which valid-ratio is averaged. Power of two for cheap mask. */
#define CFG_PERF_VALID_WINDOW        256u

/** Frames over which fps is averaged. */
#define CFG_PERF_FPS_WINDOW          32u

#endif /* PUPIL_APP_CONFIG_H */
