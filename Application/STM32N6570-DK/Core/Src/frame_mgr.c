/**
 * @file    frame_mgr.c
 * @brief   Single-slot frame handle manager.
 *
 *          Real implementation uses two slots (ping-pong); this bring-up
 *          version uses one and drops incoming frames while processing is
 *          busy. That is acceptable while the classical CV pipeline is
 *          being tuned - latency dominates over frame rate.
 *
 *          Thread-safety: push() may be called from a camera ISR while the
 *          consumer runs in main context. We use volatile busy flags and
 *          disable/restore PRIMASK around the short critical section.
 */
#include "frame_mgr.h"

#include <string.h>

#if defined(__ARM_ARCH)
#include "cmsis_compiler.h"
#endif

static volatile uint8_t s_busy;     /* 1 while a frame is held by consumer */
static volatile uint8_t s_ready;    /* 1 when a frame is waiting           */
static frame_t          s_slot;     /* full copy of the pushed frame       */

static inline uint32_t enter_crit(void)
{
#if defined(__ARM_ARCH)
    uint32_t prim = __get_PRIMASK();
    __disable_irq();
    return prim;
#else
    return 0;
#endif
}
static inline void leave_crit(uint32_t prim)
{
#if defined(__ARM_ARCH)
    if (!prim) __enable_irq();
#else
    (void)prim;
#endif
}

int frame_mgr_init(void)
{
    s_busy = 0;
    s_ready = 0;
    memset((void*)&s_slot, 0, sizeof s_slot);
    return 0;
}

int frame_mgr_push(const frame_t* f)
{
    if (!f) return -1;

    uint32_t p = enter_crit();
    /* Drop if a consumer still holds the previous frame or one is waiting. */
    if (s_busy || s_ready) {
        leave_crit(p);
        return -2;
    }
    s_slot  = *f;       /* byte copy of the handle; pixel buffer is aliased */
    s_ready = 1;
    leave_crit(p);
    return 0;
}

frame_t* frame_mgr_acquire(void)
{
    frame_t* out = 0;

    uint32_t p = enter_crit();
    if (s_ready && !s_busy) {
        s_ready = 0;
        s_busy  = 1;
        out = &s_slot;
    }
    leave_crit(p);
    return out;
}

void frame_mgr_release(frame_t* f)
{
    (void)f;
    uint32_t p = enter_crit();
    s_busy = 0;
    leave_crit(p);
}
