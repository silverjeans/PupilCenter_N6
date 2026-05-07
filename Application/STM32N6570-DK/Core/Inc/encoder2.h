/**
 * @file    encoder2.h
 * @brief   TIM1 쿼드러처 엔코더 입력 드라이버 (엔코더 2번).
 *
 *  핀 배치 (STM32N6570-DK):
 *    A상 → PE9  (TIM1_CH1  AF1)
 *    B상 → PA9  (TIM1_CH2  AF1)
 *
 *  선정 이유:
 *    STM32N6570-DK 기본 구성(LCD + Camera + Ethernet + SDMMC + USART1 + I2C)
 *    에서 TIM2/4/5/8의 CH1+CH2 핀은 모두 LCD·ETH·SDMMC에 점령되어 있다.
 *    TIM1은 원래 PWM CH1(PF6/TIM1_CH3)에 사용되었으나,
 *    PWM CH1을 TIM16_CH1/PA3로 이전하면서 TIM1을 인코더로 해방함.
 *
 *  동작 방식:
 *    TIM_ENCODERMODE_TI12 — CH1·CH2 양쪽 엣지 모두 카운트 (×4 분해능).
 *    TIM1은 16-bit 카운터 (0x0000 ~ 0xFFFF).
 *    카운터는 정방향 회전 시 증가, 역방향 회전 시 감소하며
 *    0x0000/0xFFFF 경계에서 자연스럽게 롤오버된다.
 *
 *  델타 계산 예:
 *    int16_t delta = (int16_t)(ENC2_GetRawCount() - last_count);
 *    last_count = ENC2_GetRawCount();
 *    // int16_t 캐스트가 16-bit 롤오버를 자동으로 처리한다.
 */

#ifndef ENCODER2_H
#define ENCODER2_H

#include <stdint.h>

/**
 * @brief  GPIO 및 TIM1 엔코더 모드 초기화.
 *         ENC2_Start() 전에 호출.
 */
void ENC2_Init(void);

/** @brief  카운터 시작 (카운터는 0으로 리셋 후 시작). */
void ENC2_Start(void);

/** @brief  카운터 정지. */
void ENC2_Stop(void);

/**
 * @brief  현재 카운터 원시값 반환.
 * @return 0x0000 ~ 0xFFFF 범위의 16-bit 카운터.
 *         방향은 int16_t 캐스트로 이전 값과 빼서 판단.
 */
uint16_t ENC2_GetRawCount(void);

/** @brief  카운터를 0으로 리셋. */
void ENC2_ResetCount(void);

/**
 * @brief  현재 회전 방향 반환.
 * @return  1 = 정방향(CW),  -1 = 역방향(CCW).
 *         TIM1 CR1 레지스터의 DIR 비트를 읽는다.
 */
int8_t ENC2_GetDirection(void);

#endif /* ENCODER2_H */
