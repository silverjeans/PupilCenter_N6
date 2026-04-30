/**
 * @file    app_main.c
 * @brief   PupilCenter_N6 pipeline entry point.
 *
 *          Expected caller: the board-level main() in Src/main.c invokes
 *          app_main_run() *after* HAL / clocks / XSPI / LCD are ready. This
 *          function never returns.
 *
 *          Pipeline order, per frame:
 *            1. frame_mgr_acquire  - wait for a new frame from camera_if
 *            2. perf_monitor_begin - start latency stopwatch
 *            3. tracking_sm_get_state / roi_mgr_compute
 *            4. pupil_detect_run   - classical CV inside the ROI
 *            5. pupil_filter_update- EMA + jump reject
 *            6. tracking_sm_update - hit/miss streak bookkeeping
 *            7. result_output_emit - one-line UART log
 *            8. debug_overlay_*    - LCD visualization
 *            9. perf_monitor_end   - latency / fps / valid-ratio update
 *
 *          Every module is initialized once in app_main_init(); the loop
 *          itself is straight-line and allocation-free.
 */
#include "pupil_config.h"
#include "app_types.h"

#include "camera_if.h"
#include "frame_mgr.h"
#include "roi_mgr.h"
#include "pupil_detect.h"
#include "pupil_filter.h"
#include "tracking_sm.h"
#include "result_output.h"
#include "perf_monitor.h"
#include "debug_overlay.h"

#include <stddef.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * UART transport for result_output. Provided by a small BSP shim that wraps
 * HAL_UART_Transmit on the ST reference example's USART1 (115200 console).
 * Signature matches result_output_write_fn exactly so no cast is required.
 * The shim lives in Src/bsp_uart.c so this file stays HW-independent.
 * ------------------------------------------------------------------------- */
extern int bsp_uart_write(const uint8_t* data, size_t len);

/* Attach LTDC layer 1 to the camera buffer. Implemented in Src/bsp_lcd.c.
 * Kept out of Core/ because it pulls in BSP LCD headers. */
extern int bsp_lcd_attach_camera_layer(const uint8_t* bg,
                                       uint32_t bg_x, uint32_t bg_y,
                                       uint32_t bg_w, uint32_t bg_h);

/* Pull new camera frames into frame_mgr. Declared in camera_if.c; not in
 * the public camera_if.h because only app_main is allowed to pump. */
int camera_if_pump(void);



/* ---------------------------------------------------------------------------
 * Camera callback trampoline: push frames into frame_mgr. Runs from ISR
 * context (or near-ISR in polled mode), so it must not block.
 * ------------------------------------------------------------------------- */
static void on_camera_frame(const frame_t* f, void* user)
{
    (void)user;
    frame_mgr_push(f);
}

/* ---------------------------------------------------------------------------
 * One-shot init; logs and returns non-zero on fatal error so the outer
 * bring-up main can flash an LED.
 * ------------------------------------------------------------------------- */
static int app_main_init(void)
{
    /* Order matters: camera_if provides the us timer everyone else uses. */
    if (camera_if_init()  != 0) return -1;
    if (frame_mgr_init()  != 0) return -2;
    if (roi_mgr_init(CFG_FRAME_WIDTH, CFG_FRAME_HEIGHT) != 0) return -3;

    pupil_cfg_t pc;
    pc.dark_threshold    = CFG_DETECT_DARK_THRESHOLD;
    pc.min_area_px       = CFG_DETECT_MIN_AREA_PX;
    pc.max_area_px       = CFG_DETECT_MAX_AREA_PX;
    pc.center_bias_x100  = 35;                       /* prefer ROI center */
    pc.max_blobs         = CFG_DETECT_MAX_BLOBS;
    if (pupil_detect_init(&pc) != 0) return -4;

    if (pupil_filter_init() != 0) return -5;
    if (tracking_sm_init()  != 0) return -6;
    if (perf_monitor_init() != 0) return -7;

    if (result_output_init(bsp_uart_write) != 0) return -8;

    if (debug_overlay_init() != 0) return -9;

    /* Attach LCD layer 0 to the camera PSRAM buffer. Must happen after
     * camera_if_init (which fixes output width/height) but before
     * camera_if_start so the first frame already renders on the panel. */
    {
        uint16_t bw = 0, bh = 0, bs = 0;
        camera_if_get_geometry(&bw, &bh, &bs);
        const uint8_t* bg = camera_if_get_buffer();
        /* Center the square preview on the 800x480 landscape panel. */
        uint32_t bx = (800u > bw) ? ((800u - bw) / 2u) : 0u;
        uint32_t by = (480u > bh) ? ((480u - bh) / 2u) : 0u;
        bsp_lcd_attach_camera_layer(bg, bx, by, bw, bh);

        /* Teach roi_mgr about the real preview geometry (may differ from
         * the CFG_FRAME_* default). */
        roi_mgr_init(bw, bh);

        /* Wire LTDC Layer 2 (ARGB4444) to the overlay buffer.
         * The overlay sits on top of the camera layer at the same position. */
        debug_overlay_attach_layer(bx, by, bw, bh);
    }

    /* Fire the camera last so no frames arrive before the pipeline is ready. */
    if (camera_if_start(on_camera_frame, NULL) != 0) return -10;

    {
        uint16_t bw = 0, bh = 0, bs = 0;
        camera_if_get_geometry(&bw, &bh, &bs);
        result_output_emit_event("boot: preview=%ux%u stride=%u ROI_search=%ux%u ROI_track=%ux%u",
                                 (unsigned)bw, (unsigned)bh, (unsigned)bs,
                                 (unsigned)CFG_ROI_SEARCH_W, (unsigned)CFG_ROI_SEARCH_H,
                                 (unsigned)CFG_ROI_TRACK_W, (unsigned)CFG_ROI_TRACK_H);
    }
    return 0;
}

/* ---------------------------------------------------------------------------
 * Main loop. Single-threaded, polled.
 * ------------------------------------------------------------------------- */
void app_main_run(void)
{
    int rc = app_main_init();
    if (rc != 0) {
        /* Error path: keep emitting the fault code so a host monitor
         * sees something. Caller's board main should drive an error LED
         * after this function returns... but app_main_run is non-
         * returning by contract, so loop here. */
        while (1) {
            result_output_emit_event("FATAL: app_main_init rc=%d", rc);
            for (volatile int i = 0; i < 2000000; ++i) { __asm("nop"); }
        }
    }

    /* Last accepted smoothed result - seeds ROI tracking and filter state. */
    pupil_result_t last_smooth;
    memset(&last_smooth, 0, sizeof last_smooth);

    while (1) {
        /* 0. Poll camera -> frame_mgr. Single-threaded, cheap when idle. */
        camera_if_pump();

        frame_t* f = frame_mgr_acquire();
        if (!f) {
            /* No frame yet. Optionally enter light sleep; bring-up polls. */
            continue;
        }

        perf_monitor_begin();

        /* 1. ROI selection driven by tracking state ------------------- */
        tracking_state_t state = tracking_sm_get_state();
        uint16_t         miss  = tracking_sm_miss_streak();
        roi_t            roi;
        roi_mgr_compute(state, &last_smooth, miss, &roi);

        /* 2. Classical-CV detection inside the ROI -------------------- */
        pupil_result_t raw;
        int hit = pupil_detect_run(f, &roi, &raw);

        /* 3. Temporal smoothing --------------------------------------- */
        pupil_result_t smoothed;
        pupil_filter_update(&raw, &smoothed);

        /* 4. State machine update ------------------------------------- */
        tracking_sm_update(hit && smoothed.valid, &smoothed);

        /* SEARCH 상태에서는 필터 내부 위치를 리셋한다.
         * 점프 거부(60 px)가 재락(re-lock)을 영구 차단하는 것을 막기 위함. */
        if (tracking_sm_get_state() == TRK_SEARCH)
            pupil_filter_reset();

        /* 5. One-line UART log (매 프레임 출력 - 필요시 주석 해제) ---- */
        const perf_stat_t* ps = perf_monitor_get();
        uint32_t lat_us = ps ? ps->last_latency_us : 0;
        /* result_output_emit(f->seq, f->ts_ms, &smoothed,
                           (uint8_t)tracking_sm_get_state(), lat_us); */

        /* Periodic diagnostic: detection + performance summary.        */
        if ((f->seq % 60u) == 0u) {
            if (ps) {
                result_output_emit_event(
                    "perf fps=%lu.%lu lat_avg=%luus lat_last=%luus "
                    "cpu=%lu.%02lu%% valid=%u%%",
                    (unsigned long)(ps->avg_fps_m / 1000u),
                    (unsigned long)(ps->avg_fps_m % 1000u) / 100u,
                    (unsigned long)ps->avg_latency_us,
                    (unsigned long)ps->last_latency_us,
                    (unsigned long)(ps->cpu_occ_x100 / 100u),
                    (unsigned long)(ps->cpu_occ_x100 % 100u),
                    (unsigned)ps->valid_ratio_x100);
            }
        }

        /* 6. LTDC Layer 2 ARGB4444 overlay — hardware crosshair. -------- */
        debug_overlay_begin_frame();
        debug_overlay_draw(&roi, &smoothed);
        debug_overlay_end_frame();

        /* 7. Bookkeeping ---------------------------------------------- */
        if (smoothed.valid) last_smooth = smoothed;
        perf_monitor_end(smoothed.valid ? 1 : 0);

        frame_mgr_release(f);
    }
}
