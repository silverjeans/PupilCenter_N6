/**
 * @file    app_config.h
 * @brief   ST-reference compatible configuration knobs consumed by
 *          app_camerapipeline.c and main.c.
 *
 *          Higher-level pupil-center tuning lives in App/Core/include/config.h.
 *          This header keeps only what the ST example infrastructure still
 *          reads (aspect-ratio mode, camera flip, color swap, welcome text).
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "arm_math.h"

#define USE_DCACHE

/* Camera flip: CMW_MIRRORFLIP_NONE / FLIP / MIRROR / FLIP_MIRROR */
#define CAMERA_FLIP CMW_MIRRORFLIP_NONE

/* Aspect ratio handling between sensor and display/NN pipes. */
#define ASPECT_RATIO_CROP       (1)
#define ASPECT_RATIO_FIT        (2)
#define ASPECT_RATIO_FULLSCREEN (3)
#define ASPECT_RATIO_MODE       ASPECT_RATIO_CROP

/* DCMIPP pixel swap for the NN pipe. */
#define COLOR_BGR (0)
#define COLOR_RGB (1)
#define COLOR_MODE COLOR_RGB

/* Splash screen text. */
#define WELCOME_MSG_1 "Segmentation bring-up (stub mask)"
#define WELCOME_MSG_2 "Pupil center detection on STM32N6"

#endif /* APP_CONFIG_H */
