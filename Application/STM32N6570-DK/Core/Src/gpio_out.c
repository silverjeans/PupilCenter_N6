/**
 * @file    gpio_out.c
 * @brief   범용 GPIO Push-Pull 출력 드라이버.
 *
 *  OUT1  PD13   GPIO_PIN_13   GPIOD
 *  OUT2  PF1    GPIO_PIN_1    GPIOF
 *  OUT3  PB8    GPIO_PIN_8    GPIOB
 *  OUT4  PG9    GPIO_PIN_9    GPIOG
 */

#include "gpio_out.h"
#include "stm32n6xx_hal.h"

/* -------------------------------------------------------------------------
 * 핀 테이블 — 순서가 GPIO_OutPin_t enum과 일치해야 한다.
 * ---------------------------------------------------------------------- */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} PinMap_t;

static const PinMap_t PIN_MAP[] = {
    { GPIOD, GPIO_PIN_13 },   /* OUT1  PD13 */
    { GPIOF, GPIO_PIN_1  },   /* OUT2  PF1  */
    { GPIOB, GPIO_PIN_8  },   /* OUT3  PB8  */
    { GPIOG, GPIO_PIN_9  },   /* OUT4  PG9  */
};

/* =========================================================================
 * GPIO_Out_Init
 * ======================================================================= */
void GPIO_Out_Init(void)
{
    /* 클럭 활성화 */
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;

    /* PD13 */
    g.Pin = GPIO_PIN_13;
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_RESET);
    HAL_GPIO_Init(GPIOD, &g);

    /* PF1 */
    g.Pin = GPIO_PIN_1;
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_1, GPIO_PIN_RESET);
    HAL_GPIO_Init(GPIOF, &g);

    /* PB8 */
    g.Pin = GPIO_PIN_8;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
    HAL_GPIO_Init(GPIOB, &g);

    /* PG9 */
    g.Pin = GPIO_PIN_9;
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_9, GPIO_PIN_RESET);
    HAL_GPIO_Init(GPIOG, &g);
}

/* =========================================================================
 * Set / Reset / Toggle / Write / Read
 * ======================================================================= */
void GPIO_Out_Set(GPIO_OutPin_t pin)
{
    HAL_GPIO_WritePin(PIN_MAP[pin].port, PIN_MAP[pin].pin, GPIO_PIN_SET);
}

void GPIO_Out_Reset(GPIO_OutPin_t pin)
{
    HAL_GPIO_WritePin(PIN_MAP[pin].port, PIN_MAP[pin].pin, GPIO_PIN_RESET);
}

void GPIO_Out_Toggle(GPIO_OutPin_t pin)
{
    HAL_GPIO_TogglePin(PIN_MAP[pin].port, PIN_MAP[pin].pin);
}

void GPIO_Out_Write(GPIO_OutPin_t pin, uint8_t val)
{
    HAL_GPIO_WritePin(PIN_MAP[pin].port, PIN_MAP[pin].pin,
                      val ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

uint8_t GPIO_Out_Read(GPIO_OutPin_t pin)
{
    return (HAL_GPIO_ReadPin(PIN_MAP[pin].port, PIN_MAP[pin].pin) == GPIO_PIN_SET)
           ? 1u : 0u;
}
