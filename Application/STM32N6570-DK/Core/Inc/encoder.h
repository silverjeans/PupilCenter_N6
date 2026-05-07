/**
 * @file    encoder.h
 * @brief   TIM3 쿼드러처 엔코더 입력 드라이버 (엔코더 1번).
 *
 *  핀 배치 (CN4, STM32N6570-DK):
 *    A상 → PC6  (CN4 Pin 9 )  TIM3_CH1  AF2
 *    B상 → PC7  (CN4 Pin 14)  TIM3_CH2  AF2
 *
 *  동작 방식:
 *    TIM_ENCODERMODE_TI12 — CH1·CH2 양쪽 엣지 모두 카운트 (×4 분해능).
 *    TIM3는 16-bit 카운터 (0x0000 ~ 0xFFFF).
 *    카운터는 정방향 회전 시 증가, 역방향 회전 시 감소하며
 *    0x0000/0xFFFF 경계에서 자연스럽게 롤오버된다.
 *
 *  델타 계산 예:
 *    int16_t delta = (int16_t)(ENC1_GetRawCount() - last_count);
 *    last_count = ENC1_GetRawCount();
 *    // int16_t 캐스트가 16-bit 롤오버를 자동으로 처리한다.
 */

#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>

/**
 * @brief  GPIO 및 TIM3 엔코더 모드 초기화.
 *         ENC1_Start() 전에 호출.
 */
void ENC1_Init(void);

/** @brief  카운터 시작 (카운터는 0으로 리셋 후 시작). */
void ENC1_Start(void);

/** @brief  카운터 정지. */
void ENC1_Stop(void);

/**
 * @brief  현재 카운터 원시값 반환.
 * @return 0x0000 ~ 0xFFFF 범위의 16-bit 카운터.
 *         방향은 int16_t 캐스트로 이전 값과 빼서 판단.
 */
uint16_t ENC1_GetRawCount(void);

/** @brief  카운터를 0으로 리셋. */
void ENC1_ResetCount(void);

/**
 * @brief  현재 회전 방향 반환.
 * @return  1 = 정방향(CW),  -1 = 역방향(CCW),  0 = 정지.
 *         TIM3 CR1 레지스터의 DIR 비트를 읽는다.
 */
int8_t ENC1_GetDirection(void);

#endif /* ENCODER_H */
