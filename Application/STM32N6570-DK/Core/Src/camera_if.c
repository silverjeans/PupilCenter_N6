/**
 * @file    camera_if.c
 * @brief   Camera interface bound to the ST DCMIPP PIPE1 (RGB565) display
 *          stream. Publishes a frame_t to the pipeline every time the
 *          capture-complete counter advances.
 *
 *          Hardware-specific bits are kept here; all upper layers see only
 *          frame_t and the us / ms timebase.
 */
#include "camera_if.h"
#include "pupil_config.h"

#include "stm32n6xx_hal.h"
#include "app_camerapipeline.h"     /* CameraPipeline_*  */
#include "cmw_camera.h"             /* CMW_MODE_CONTINUOUS */

/* PSRAM-resident camera / display buffer, exactly LCD preview sized.
 * Populated by DCMIPP, consumed by LCD layer 0 and by pupil_detect. */
__attribute__((section(".psram_bss"), aligned(32)))
static uint8_t s_bg_buffer[800 * 480 * 2];

static camera_if_frame_cb_t s_cb   = 0;
static void*                s_user = 0;

static uint32_t s_cycles_per_us = 1;
static uint32_t s_last_seen     = 0;      /* last camera_frame_count we served */
static uint32_t s_bg_width      = 0;
static uint32_t s_bg_height     = 0;

extern volatile uint32_t camera_frame_count;   /* from app_camerapipeline.c */

/* ------------------------------------------------------------------ */
/* us / ms timebase (DWT CYCCNT)                                       */
/* ------------------------------------------------------------------ */
int camera_if_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;

    extern uint32_t SystemCoreClock;
    s_cycles_per_us = SystemCoreClock / 1000000u;
    if (s_cycles_per_us == 0) s_cycles_per_us = 1;

    /* Initialise DCMIPP PIPE1 (RGB565, aspect-ratio cropped). */
    uint32_t pitch_nn_unused = 0;
    CameraPipeline_Init(&s_bg_width, &s_bg_height, &pitch_nn_unused);

    return 0;
}

uint32_t camera_if_now_us(void) { return DWT->CYCCNT / s_cycles_per_us; }
uint32_t camera_if_now_ms(void) { return HAL_GetTick(); }

const uint8_t* camera_if_get_buffer(void) { return s_bg_buffer; }

void camera_if_get_geometry(uint16_t* w, uint16_t* h, uint16_t* stride)
{
    if (w)      *w      = (uint16_t)s_bg_width;
    if (h)      *h      = (uint16_t)s_bg_height;
    if (stride) *stride = (uint16_t)(s_bg_width * 2u);
}

/* ------------------------------------------------------------------ */
/* Start / Stop                                                        */
/* ------------------------------------------------------------------ */
int camera_if_start(camera_if_frame_cb_t cb, void* user)
{
    s_cb      = cb;
    s_user    = user;
    s_last_seen = camera_frame_count;

    CameraPipeline_DisplayPipe_Start(s_bg_buffer, CMW_MODE_CONTINUOUS);
    return 0;
}

void camera_if_stop(void)
{
    CameraPipeline_DisplayPipe_Stop();
    s_cb = 0;
    s_user = 0;
}

/* ------------------------------------------------------------------ */
/* ROI-only DCache invalidation                                        */
/*                                                                     */
/* DCMIPP writes the full frame to PSRAM (bypasses DCache). The CPU   */
/* only reads pixels inside the ROI for detection. Invalidating only  */
/* those rows reduces the per-frame cost from 768 KB to <= 50 KB.     */
/*                                                                     */
/* Not in camera_if.h intentionally — only app_main calls this.       */
/* ------------------------------------------------------------------ */
void camera_if_invalidate_region(int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0) return;
    const uint32_t stride   = s_bg_width * 2u;
    const uint8_t* row      = s_bg_buffer + (uint32_t)y * stride + (uint32_t)x * 2u;
    const int32_t  row_bytes = (int32_t)(w * 2);
    for (int r = 0; r < h; ++r) {
        SCB_InvalidateDCache_by_Addr((uint32_t*)(uintptr_t)row, row_bytes);
        row += stride;
    }
}

/* ------------------------------------------------------------------ */
/* Pump - called by frame_mgr / app_main. Serves one frame per new    */
/* DCMIPP delivery. Non-blocking.                                     */
/* ------------------------------------------------------------------ */
/**
 * Not declared in camera_if.h on purpose: this is a polling hook that
 * lives inside the camera_if implementation and is only called from
 * frame_mgr (via weak linkage in a future refactor). For now we invoke
 * it directly from app_main.
 *
 * Returns 1 if a frame was dispatched, 0 otherwise.
 */
int camera_if_pump(void)
{
    if (!s_cb) return 0;

    /* ISR updates continuous; the counter may advance more than once if
     * we fell behind. We still serve at most one frame per pump to keep
     * latency low and bounded. */
    uint32_t cur = camera_frame_count;
    if (cur == s_last_seen) return 0;

    /* Let the ISP tune itself (auto exposure etc.) regularly. */
    CameraPipeline_IspUpdate();

    s_last_seen = cur;

    /* Cache flush: DCMIPP wrote to PSRAM; CPU must see the new bytes. */
    SCB_InvalidateDCache_by_Addr((uint32_t*)s_bg_buffer, sizeof s_bg_buffer);

    frame_t f;
    f.data   = s_bg_buffer;
    f.width  = (uint16_t)s_bg_width;
    f.height = (uint16_t)s_bg_height;
    f.stride = (uint16_t)(s_bg_width * 2u);
    f.fmt    = PIX_FMT_RGB565;
    f.ts_ms  = camera_if_now_ms();
    f.seq    = cur;

    s_cb(&f, s_user);
    return 1;
}
