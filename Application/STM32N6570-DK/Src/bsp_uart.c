/**
 * @file    bsp_uart.c
 * @brief   Thin shim between result_output and the ST reference example's
 *          USART1 handle (huart1 @ 115200). Kept here instead of Core/ so
 *          Core stays HW-independent.
 */
#include <stddef.h>
#include <stdint.h>

#include "stm32n6xx_hal.h"

extern UART_HandleTypeDef huart1;

int bsp_uart_write(const uint8_t* data, size_t len)
{
    if (!data || len == 0) return 0;
    HAL_StatusTypeDef st = HAL_UART_Transmit(&huart1,
                                             (uint8_t*)data,
                                             (uint16_t)len,
                                             100u);
    return (st == HAL_OK) ? (int)len : -1;
}
