/**
 * @file    perf_monitor.h
 * @brief   Lightweight pipeline-wide performance counters.
 *
 *          The pipeline calls perf_monitor_begin() at frame start and
 *          perf_monitor_end(valid) at frame end. The module maintains:
 *            - last_latency_us : end - begin
 *            - avg_latency_us  : EMA smoothed, integer only
 *            - avg_fps_m       : milli-Hz = fps * 1000, integer
 *            - valid_ratio_x100: sliding-window percentage of valid frames
 *
 *          No floating point internally.
 */
#ifndef PERF_MONITOR_H
#define PERF_MONITOR_H

#include <stdint.h>

typedef struct {
    uint32_t last_latency_us;
    uint32_t avg_latency_us;
    uint32_t avg_fps_m;       /* fps * 1000 */
    uint16_t valid_ratio_x100;/* 0..100 */
    uint32_t frames_total;
    uint32_t frames_valid;
} perf_stat_t;

int  perf_monitor_init(void);
void perf_monitor_reset(void);

void perf_monitor_begin(void);
void perf_monitor_end(int frame_valid);

const perf_stat_t* perf_monitor_get(void);

#endif /* PERF_MONITOR_H */
