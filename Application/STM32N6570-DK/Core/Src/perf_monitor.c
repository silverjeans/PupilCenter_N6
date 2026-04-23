/**
 * @file    perf_monitor.c
 * @brief   Minimal link-clean implementation. Tracks last_latency_us only.
 *          Replaced by the full version (EMA latency / sliding-window fps /
 *          valid-ratio ring buffer) in the next step.
 */
#include "perf_monitor.h"
#include "camera_if.h"

#include <string.h>

static perf_stat_t s_stat;
static uint32_t    s_begin_us;

int  perf_monitor_init(void)  { perf_monitor_reset(); return 0; }
void perf_monitor_reset(void) { memset(&s_stat, 0, sizeof s_stat); s_begin_us = 0; }

void perf_monitor_begin(void)
{
    s_begin_us = camera_if_now_us();
}

void perf_monitor_end(int frame_valid)
{
    uint32_t now = camera_if_now_us();
    s_stat.last_latency_us = now - s_begin_us;
    s_stat.frames_total++;
    if (frame_valid) s_stat.frames_valid++;
    /* TODO(full-impl): EMA avg_latency, sliding fps, ring-buffer valid_ratio. */
}

const perf_stat_t* perf_monitor_get(void) { return &s_stat; }
