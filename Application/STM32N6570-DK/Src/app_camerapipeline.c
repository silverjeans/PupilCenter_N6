/**
 * @file    app_camerapipeline.c
 * @brief   Single-pipe camera bring-up (DCMIPP PIPE1, RGB565 for both LCD
 *          display and pupil detection).
 *
 *          Classical-CV detector runs directly on the display buffer, so
 *          there is no second pipe. Output geometry matches the LCD size
 *          (<= 800x480) determined at runtime from the sensor capabilities.
 *
 *          Frame arrival is signalled via an incrementing counter updated
 *          in the DCMIPP frame-event ISR. camera_if.c reads that counter.
 */
#include <assert.h>
#include <stdint.h>

#include "cmw_camera.h"
#include "app_camerapipeline.h"
#include "app_config.h"

/* Let the CMW driver pick the native sensor resolution. */
#define CAMERA_WIDTH   0
#define CAMERA_HEIGHT  0
#define CAMERA_FPS     30

/* Updated from ISR every time a PIPE1 frame is delivered. */
volatile uint32_t camera_frame_count = 0;
volatile uint32_t camera_pipe1_errors = 0;

static void DCMIPP_PipeInitDisplay(CMW_CameraInit_t* cam,
                                   uint32_t* bg_width,
                                   uint32_t* bg_height)
{
    CMW_DCMIPP_Conf_t dcmipp_conf = {0};
    CMW_Aspect_Ratio_Mode_t ar = CMW_Aspect_ratio_crop;

    if (ASPECT_RATIO_MODE == ASPECT_RATIO_FIT)        ar = CMW_Aspect_ratio_fit;
    if (ASPECT_RATIO_MODE == ASPECT_RATIO_FULLSCREEN) ar = CMW_Aspect_ratio_fullscreen;

    int w, h;
    h = (cam->height <= SCREEN_HEIGHT) ? cam->height : SCREEN_HEIGHT;
#if ASPECT_RATIO_MODE == ASPECT_RATIO_FULLSCREEN
    w = ((cam->width * h) / cam->height);
    w -= w % 16;   /* DCMIPP requires width to be a multiple of 16 bytes */
#else
    w = h;         /* square preview for CROP / FIT */
#endif

    *bg_width  = (uint32_t)w;
    *bg_height = (uint32_t)h;

    dcmipp_conf.output_width  = w;
    dcmipp_conf.output_height = h;
    dcmipp_conf.output_format = DCMIPP_PIXEL_PACKER_FORMAT_RGB565_1;
    dcmipp_conf.output_bpp    = 2;
    dcmipp_conf.mode          = ar;
    dcmipp_conf.enable_gamma_conversion = 0;

    uint32_t pitch = 0;
    int ret = CMW_CAMERA_SetPipeConfig(DCMIPP_PIPE1, &dcmipp_conf, &pitch);
    assert(ret == HAL_OK);
    assert(dcmipp_conf.output_width * dcmipp_conf.output_bpp == pitch);
}

void CameraPipeline_Init(uint32_t* lcd_bg_width,
                         uint32_t* lcd_bg_height,
                         uint32_t* pitch_nn)
{
    CMW_CameraInit_t cam_conf;
    cam_conf.width       = CAMERA_WIDTH;
    cam_conf.height      = CAMERA_HEIGHT;
    cam_conf.fps         = CAMERA_FPS;
    cam_conf.mirror_flip = CAMERA_FLIP;

    int ret = CMW_CAMERA_Init(&cam_conf, NULL);
    assert(ret == CMW_ERROR_NONE);

    DCMIPP_PipeInitDisplay(&cam_conf, lcd_bg_width, lcd_bg_height);

    if (pitch_nn) *pitch_nn = 0;  /* no NN pipe in this build */
}

void CameraPipeline_DeInit(void)
{
    int ret = CMW_CAMERA_DeInit();
    assert(ret == CMW_ERROR_NONE);
}

void CameraPipeline_DisplayPipe_Start(uint8_t* dst, uint32_t mode)
{
    int ret = CMW_CAMERA_Start(DCMIPP_PIPE1, dst, mode);
    assert(ret == CMW_ERROR_NONE);
}

void CameraPipeline_DisplayPipe_Stop(void)
{
    int ret = CMW_CAMERA_Suspend(DCMIPP_PIPE1);
    assert(ret == CMW_ERROR_NONE);
}

/* PIPE2 accessors kept as no-ops so linker does not break if old code
 * references them; they will be deleted once all call sites are gone. */
void CameraPipeline_NNPipe_Start(uint8_t* dst, uint32_t mode)
{
    (void)dst; (void)mode;
}

void CameraPipeline_IspUpdate(void)
{
    (void)CMW_CAMERA_Run();
}

int CMW_CAMERA_PIPE_FrameEventCallback(uint32_t pipe)
{
    if (pipe == DCMIPP_PIPE1) {
        camera_frame_count++;
    }
    return 0;
}

void CMW_CAMERA_PIPE_ErrorCallback(uint32_t pipe)
{
    if (pipe == DCMIPP_PIPE1) camera_pipe1_errors++;
}
