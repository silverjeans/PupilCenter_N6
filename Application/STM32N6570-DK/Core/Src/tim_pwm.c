/**
 * @file    tim_pwm.c
 * @brief   듀얼 채널 20 kHz PWM 출력 드라이버.
 *
 *  CH1  PA3  (CN4 Pin 6)   TIM16_CH1   AF1
 *  CH2  PG2  (CN4 Pin 2)   TIM14_CH1   AF11
 *
 *  변경 이력:
 *    CH1 PF6/TIM1_CH3(AF13) → PA3/TIM16_CH1(AF1)
 *    TIM1이 인코더 2번(PE9+PA9)에 할당되면서 PWM CH1을 TIM16으로 이동.
 *
 *  클럭 체인 (main.c SystemClock_Config 기준):
 *    PLL1 = 800 MHz  →  SYSCLK(IC2) = 400 MHz
 *    HCLK = 200 MHz  (AHB /2)
 *    APB2 = 200 MHz  (APB2 prescaler = 1)
 *    TIM14_CLK = APB1 × 1 = 200 MHz
 *    TIM16_CLK = APB2 × 1 = 200 MHz
 *
 *  타이머 설정:
 *    PSC = 0, ARR = 9999  →  200 MHz / 10 000 = 20 kHz
 *    CCR 범위: 0 (0 %) … 10 000 (100 %)
 *
 *  주의: TIM16은 complementary 출력이 있는 GP 타이머이므로
 *        HAL_TIM_PWM_Start() 시 내부적으로 MOE 비트가 자동으로 설정된다.
 */

#include "tim_pwm.h"
#include "stm32n6xx_hal.h"

/* -------------------------------------------------------------------------
 * 모듈 전용 타이머 핸들
 * ---------------------------------------------------------------------- */
static TIM_HandleTypeDef htim16;
static TIM_HandleTypeDef htim14;

/* =========================================================================
 * 내부 헬퍼: 공통 PWM1 모드 OC 설정
 * ======================================================================= */
static void config_oc_channel(TIM_HandleTypeDef *htim,
                               uint32_t channel,
                               uint32_t pulse)
{
    TIM_OC_InitTypeDef oc = {0};
    oc.OCMode       = TIM_OCMODE_PWM1;
    oc.Pulse        = pulse;
    oc.OCPolarity   = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode   = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(htim, &oc, channel);
}

/* =========================================================================
 * 내부 헬퍼: time-base 공통 초기화 (PSC=0, ARR=9999)
 * ======================================================================= */
static void init_timebase(TIM_HandleTypeDef *htim, TIM_TypeDef *instance)
{
    htim->Instance               = instance;
    htim->Init.Prescaler         = 0;
    htim->Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim->Init.Period            = TIM_PWM_ARR;   /* 9999 */
    htim->Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(htim);
}

/* =========================================================================
 * TIM_PWM_Init
 * ======================================================================= */
void TIM_PWM_Init(void)
{
    /* ------------------------------------------------------------------
     * GPIO
     * ---------------------------------------------------------------- */
    /* PA3 → TIM16_CH1  AF1 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    {
        GPIO_InitTypeDef g = {0};
        g.Pin       = GPIO_PIN_3;
        g.Mode      = GPIO_MODE_AF_PP;
        g.Pull      = GPIO_NOPULL;
        g.Speed     = GPIO_SPEED_FREQ_HIGH;
        g.Alternate = GPIO_AF1_TIM16;
        HAL_GPIO_Init(GPIOA, &g);
    }

    /* PG2 → TIM14_CH1  AF11 */
    __HAL_RCC_GPIOG_CLK_ENABLE();
    {
        GPIO_InitTypeDef g = {0};
        g.Pin       = GPIO_PIN_2;
        g.Mode      = GPIO_MODE_AF_PP;
        g.Pull      = GPIO_NOPULL;
        g.Speed     = GPIO_SPEED_FREQ_HIGH;
        g.Alternate = GPIO_AF11_TIM14;
        HAL_GPIO_Init(GPIOG, &g);
    }

    /* ------------------------------------------------------------------
     * TIM16  CH1 (PA3)
     * ---------------------------------------------------------------- */
    __HAL_RCC_TIM16_CLK_ENABLE();
    init_timebase(&htim16, TIM16);
    config_oc_channel(&htim16, TIM_CHANNEL_1, 0);

    /* ------------------------------------------------------------------
     * TIM14  CH1 (PG2)
     * ---------------------------------------------------------------- */
    __HAL_RCC_TIM14_CLK_ENABLE();
    init_timebase(&htim14, TIM14);
    config_oc_channel(&htim14, TIM_CHANNEL_1, 0);
}

/* =========================================================================
 * TIM_PWM_Start / Stop
 * ======================================================================= */
void TIM_PWM_Start(void)
{
    HAL_TIM_PWM_Start(&htim16, TIM_CHANNEL_1);   /* PA3 */
    HAL_TIM_PWM_Start(&htim14, TIM_CHANNEL_1);   /* PG2 */
}

void TIM_PWM_Stop(void)
{
    HAL_TIM_PWM_Stop(&htim16, TIM_CHANNEL_1);
    HAL_TIM_PWM_Stop(&htim14, TIM_CHANNEL_1);
}

/* =========================================================================
 * TIM_PWM_SetDuty_CH1 / CH2
 * ======================================================================= */
void TIM_PWM_SetDuty_CH1(uint32_t ccr)   /* PA3  — TIM16_CH1 */
{
    __HAL_TIM_SET_COMPARE(&htim16, TIM_CHANNEL_1, ccr);
}

void TIM_PWM_SetDuty_CH2(uint32_t ccr)   /* PG2  — TIM14_CH1 */
{
    __HAL_TIM_SET_COMPARE(&htim14, TIM_CHANNEL_1, ccr);
}
