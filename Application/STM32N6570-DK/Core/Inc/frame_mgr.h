/**
 * @file    frame_mgr.h
 * @brief   Tiny handle manager between camera_if and the processing loop.
 *
 *          camera_if fires a "frame ready" callback. frame_mgr stores that
 *          frame pointer into a slot; app_main polls frame_mgr_acquire() to
 *          consume it. Buffer bytes are owned by camera_if / DCMIPP; this
 *          module only manages handles and the single "one frame in flight"
 *          lock so the camera driver does not overwrite a frame currently
 *          under processing.
 */
#ifndef FRAME_MGR_H
#define FRAME_MGR_H

#include <stdint.h>
#include "app_types.h"

int  frame_mgr_init(void);

/**
 * @brief Called from the camera callback.
 *
 *        IMPORTANT: implementations MUST memcpy the frame_t struct into
 *        an internal slot before returning. Callers (including camera ISR
 *        context) are allowed to pass a stack-allocated frame_t; the
 *        pointer is not retained.
 *
 * @return 0 on success, negative if no slot is free (frame dropped).
 */
int  frame_mgr_push(const frame_t* f);

/**
 * @brief Non-blocking acquire. Returns NULL if no frame is waiting.
 *        Caller must call frame_mgr_release() before the next acquire.
 */
frame_t* frame_mgr_acquire(void);

/** Release the acquired frame so a new one can be pushed. */
void frame_mgr_release(frame_t* f);

#endif /* FRAME_MGR_H */
