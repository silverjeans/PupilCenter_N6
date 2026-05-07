/**
 * @file    encoder.c
 * @brief   TIM3 쿼드러처 엔코더 입력 드라이버 (엔코더 1번).
 *
 *  A상  PC6  (CN4 Pin 9 )   TIM3_CH1   AF2
 *  B상  PC7  (CN4 Pin 14)   TIM3_CH2   AF2
 *
 *  엔코더 모드 TI12:
 *    CH1(A상) 엣지 + CH2(B상) 엣지 양쪽을 모두 카운트 → ×4 분해능.
 *    예: 500 PPR 엔코더라면 회전당 2000 카운트.
 *
 *  16-bit 롤오버 처리:
 *    카운터가 0xFFFF → 0x0000 또는 0x0000 → 0xFFFF 로 롤오버될 때
 *    이전 값과의 차이를 int16_t 로 캐스트하면 올바른 부호 있는 델타를 얻는다.
 *
 *    예시:
 *      uint16_t prev = ENC1_GetRawCount();
 *      // ... 나중에 ...
 *      int16_t delta = (int16_t)(ENC1_GetRawCount() - prev);
 *      // delta > 0 : 정방향,  delta < 0 : 역방향
 *
 *  입력 필터 (IC1Filter / IC2Filter = 4):
 *    fSAMPLING = fTIM / 8,  N = 6 → 약 240 ns 디바운스
 *    고속 엔코더(예: 10 000 CPR 이상)를 쓰는 경우 0으로 낮출 것.
 */

#include "encoder.h"
#include "stm32n6xx_hal.h"

/* -------------------------------------------------------------------------
 * 모듈 전용 타이머 핸들
 * ---------------------------------------------------------------------- */
static TIM_HandleTypeDef htim3_enc;

/* =========================================================================
 * ENC1_Init
 * ======================================================================= */
void ENC1_Init(void)
{
    /* ------------------------------------------------------------------
     * GPIO  PC6(A상), PC7(B상) → TIM3_CH1/CH2  AF2
     * 엔코더 입력이므로 풀업으로 미정의 상태를 방지한다.
     * ---------------------------------------------------------------- */
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin       = GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_PULLUP;
    g.Speed     = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOC, &g);

    /* ------------------------------------------------------------------
     * TIM3 엔코더 모드 초기화
     * ARR = 0xFFFF (16-bit 최대) — 전체 카운터 범위 사용
     * ---------------------------------------------------------------- */
    __HAL_RCC_TIM3_CLK_ENABLE();

    htim3_enc.Instance               = TIM3;
    htim3_enc.Init.Prescaler         = 0;
    htim3_enc.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim3_enc.Init.Period            = 0xFFFF;   /* 16-bit 전범위 */
    htim3_enc.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim3_enc.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    TIM_Encoder_InitTypeDef enc = {0};
    enc.EncoderMode   = TIM_ENCODERMODE_TI12;        /* 양쪽 엣지 ×4 분해능 */

    enc.IC1Polarity   = TIM_ICPOLARITY_RISING;        /* A상 극성 */
    enc.IC1Selection  = TIM_ICSELECTION_DIRECTTI;
    enc.IC1Prescaler  = TIM_ICPSC_DIV1;
    enc.IC1Filter     = 4;                            /* 디바운스 필터 */

    enc.IC2Polarity   = TIM_ICPOLARITY_RISING;        /* B상 극성 */
    enc.IC2Selection  = TIM_ICSELECTION_DIRECTTI;
    enc.IC2Prescaler  = TIM_ICPSC_DIV1;
    enc.IC2Filter     = 4;

    HAL_TIM_Encoder_Init(&htim3_enc, &enc);
}

/* =========================================================================
 * ENC1_Start / Stop
 * ======================================================================= */
void ENC1_Start(void)
{
    /* 카운터를 0에서 시작 */
    __HAL_TIM_SET_COUNTER(&htim3_enc, 0);
    HAL_TIM_Encoder_Start(&htim3_enc, TIM_CHANNEL_ALL);
}

void ENC1_Stop(void)
{
    HAL_TIM_Encoder_Stop(&htim3_enc, TIM_CHANNEL_ALL);
}

/* =========================================================================
 * ENC1_GetRawCount
 * ======================================================================= */
uint16_t ENC1_GetRawCount(void)
{
    return (uint16_t)__HAL_TIM_GET_COUNTER(&htim3_enc);
}

/* =========================================================================
 * ENC1_ResetCount
 * ======================================================================= */
void ENC1_ResetCount(void)
{
    __HAL_TIM_SET_COUNTER(&htim3_enc, 0);
}

/* =========================================================================
 * ENC1_GetDirection
 *   TIM CR1 레지스터의 DIR 비트:
 *     0 = 업카운팅 (정방향)
 *     1 = 다운카운팅 (역방향)
 * ======================================================================= */
int8_t ENC1_GetDirection(void)
{
    if (__HAL_TIM_IS_TIM_COUNTING_DOWN(&htim3_enc)) {
        return -1;   /* 역방향 */
    }
    return 1;        /* 정방향 */
}
