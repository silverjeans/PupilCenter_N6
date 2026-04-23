/**
 * @file    bsp_lcd.c
 * @brief   LCD layer-0 setup. Points LTDC layer 0 at the PSRAM camera
 *          buffer owned by camera_if.c so the RGB565 preview appears on
 *          the DK screen with zero additional copies.
 *
 *          The camera_if module owns the buffer; this file only teaches
 *          the LCD hardware where it lives.
 */
#include <stddef.h>
#include <stdint.h>

#include "stm32n6xx_hal.h"
#include "stm32n6570_discovery_lcd.h"
#include "stm32_lcd.h"

int bsp_lcd_attach_camera_layer(const uint8_t* bg_buffer,
                                uint32_t bg_x, uint32_t bg_y,
                                uint32_t bg_w, uint32_t bg_h)
{
    BSP_LCD_Init(0, LCD_ORIENTATION_LANDSCAPE);

    BSP_LCD_LayerConfig_t cfg = {0};
    cfg.X0          = bg_x;
    cfg.Y0          = bg_y;
    cfg.X1          = bg_x + bg_w;
    cfg.Y1          = bg_y + bg_h;
    cfg.PixelFormat = LCD_PIXEL_FORMAT_RGB565;
    cfg.Address     = (uint32_t)bg_buffer;

    BSP_LCD_ConfigLayer(0, LTDC_LAYER_1, &cfg);

    UTIL_LCD_SetFuncDriver(&LCD_Driver);
    UTIL_LCD_SetLayer(LTDC_LAYER_1);
    UTIL_LCD_Clear(0);
    return 0;
}

/* Flush D-cache lines that cover [addr, addr+len) so writes made by the
 * CPU (e.g. in-place overlay) are visible to LTDC, which reads PSRAM as
 * a bus master. Called from Core/ via extern so Core stays HW-free. */
void bsp_lcd_flush_dcache(const void* addr, uint32_t len)
{
    if (!addr || len == 0) return;
    SCB_CleanDCache_by_Addr((uint32_t*)(uintptr_t)addr, (int32_t)len);
}
