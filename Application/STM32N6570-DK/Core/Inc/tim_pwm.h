/**
 * @file    tim_pwm.h
 * @brief   듀얼 채널 20 kHz PWM 출력 드라이버.
 *
 *  핀 배치 (STM32N6570-DK):
 *    CH1 → PA3  (CN4 Pin 6 )  TIM16_CH1  AF1
 *    CH2 → PG2  (CN4 Pin 2 )  TIM14_CH1  AF11
 *
 *  각 채널은 별도 타이머를 사용하므로 독립 제어 가능.
 *  두 타이머 모두 APB2 클럭 200 MHz → PSC=0, ARR=9999 → 20 kHz.
 *
 *  변경 이력:
 *    CH1 PF6/TIM1_CH3 → PA3/TIM16_CH1 (TIM1을 인코더 2번으로 해방)
 *
 *  사용 예:
 *    TIM_PWM_Init();
 *    TIM_PWM_Start();
 *    TIM_PWM_SetDuty_CH1( TIM_PWM_DUTY_PCT(60) );  // PA3  60 %
 *    TIM_PWM_SetDuty_CH2( TIM_PWM_DUTY_PCT(40) );  // PG2  40 %
 */

#ifndef TIM_PWM_H
#define TIM_PWM_H

#include <stdint.h>

/* ---- 컴파일 타임 상수 -------------------------------------------------- */

/** TIM1 / TIM14 공통 입력 클럭 (APB prescaler = 1 → 타이머 = APB = 200 MHz) */
#define TIM_PWM_CLK_HZ    200000000UL

/** 목표 PWM 주파수 */
#define TIM_PWM_FREQ_HZ   20000UL

/**
 * ARR = CLK/FREQ - 1 = 200 000 000 / 20 000 - 1 = 9999
 * 주기 = ARR+1 = 10 000 틱 → 50 µs
 */
#define TIM_PWM_ARR       ((uint32_t)(TIM_PWM_CLK_HZ / TIM_PWM_FREQ_HZ - 1UL))

/** 한 주기의 틱 수 (= 분해능) = 10 000 */
#define TIM_PWM_PERIOD    (TIM_PWM_ARR + 1UL)

/**
 * 정수 퍼센트(0–100) → CCR 변환 매크로
 *   TIM_PWM_SetDuty_CH1( TIM_PWM_DUTY_PCT(75) );  // 75 %
 */
#define TIM_PWM_DUTY_PCT(pct)  ((uint32_t)((uint32_t)(pct) * TIM_PWM_PERIOD / 100UL))

/* ---- API --------------------------------------------------------------- */

/**
 * @brief  GPIO 및 타이머 초기화.
 *         CH1(TIM16_CH1 / PA3), CH2(TIM14_CH1 / PG2) 모두 0 % 로 시작.
 *         TIM_PWM_Start() 전에 호출.
 */
void TIM_PWM_Init(void);

/** @brief  두 채널 동시 출력 시작. */
void TIM_PWM_Start(void);

/** @brief  두 채널 출력 정지 (출력 LOW). */
void TIM_PWM_Stop(void);

/**
 * @brief  CH1(PA3) duty 설정.
 * @param  ccr  0 (0 %) … TIM_PWM_PERIOD (100 %).  TIM_PWM_DUTY_PCT() 사용 권장.
 */
void TIM_PWM_SetDuty_CH1(uint32_t ccr);

/**
 * @brief  CH2(PG2) duty 설정.
 * @param  ccr  CH1과 동일 범위.
 */
void TIM_PWM_SetDuty_CH2(uint32_t ccr);

#endif /* TIM_PWM_H */
