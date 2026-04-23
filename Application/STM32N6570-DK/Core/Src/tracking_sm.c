/**
 * @file    tracking_sm.c
 * @brief   SEARCH / TRACK / RECOVERY state machine, driven by streak counters.
 *
 *          SEARCH   -> TRACK     : hit_streak  >= CFG_SM_LOCK_HITS
 *          TRACK    -> RECOVERY  : miss_streak >= CFG_SM_TRACK_MISS_TO_RECOVER
 *          RECOVERY -> TRACK     : hit_streak  >= CFG_SM_RECOVER_HITS
 *          RECOVERY -> SEARCH    : miss_streak >= CFG_SM_RECOVER_MISS_TO_LOST
 *
 *          State transitions are emitted as one-shot UART event lines so
 *          the host log shows boundaries clearly.
 */
#include "tracking_sm.h"
#include "pupil_config.h"
#include "result_output.h"

static tracking_state_t s_state;
static uint16_t         s_hit_streak;
static uint16_t         s_miss_streak;

static const char* state_name(tracking_state_t s)
{
    switch (s) {
        case TRK_SEARCH:   return "SEARCH";
        case TRK_TRACK:    return "TRACK";
        case TRK_RECOVERY: return "RECOVERY";
        default:           return "?";
    }
}

static void enter(tracking_state_t ns)
{
    if (ns == s_state) return;
    result_output_emit_event("STATE: %s -> %s", state_name(s_state), state_name(ns));
    s_state       = ns;
    s_hit_streak  = 0;
    s_miss_streak = 0;
}

int tracking_sm_init(void)
{
    tracking_sm_reset();
    return 0;
}

void tracking_sm_reset(void)
{
    s_state       = TRK_SEARCH;
    s_hit_streak  = 0;
    s_miss_streak = 0;
}

void tracking_sm_update(int det_valid, const pupil_result_t* det)
{
    (void)det;

    if (det_valid) {
        if (s_hit_streak  < 0xFFFF) s_hit_streak++;
        s_miss_streak = 0;
    } else {
        if (s_miss_streak < 0xFFFF) s_miss_streak++;
        s_hit_streak = 0;
    }

    switch (s_state) {
        case TRK_SEARCH:
            if (s_hit_streak >= CFG_SM_LOCK_HITS) enter(TRK_TRACK);
            break;
        case TRK_TRACK:
            if (s_miss_streak >= CFG_SM_TRACK_MISS_TO_RECOVER) enter(TRK_RECOVERY);
            break;
        case TRK_RECOVERY:
            if (s_hit_streak  >= CFG_SM_RECOVER_HITS)          enter(TRK_TRACK);
            else if (s_miss_streak >= CFG_SM_RECOVER_MISS_TO_LOST) enter(TRK_SEARCH);
            break;
        default:
            enter(TRK_SEARCH);
            break;
    }
}

tracking_state_t tracking_sm_get_state(void) { return s_state; }
uint16_t         tracking_sm_miss_streak(void) { return s_miss_streak; }
uint16_t         tracking_sm_hit_streak(void) { return s_hit_streak; }
