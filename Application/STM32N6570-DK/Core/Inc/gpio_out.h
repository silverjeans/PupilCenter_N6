/**
 * @file    gpio_out.h
 * @brief   범용 GPIO Push-Pull 출력 드라이버.
 *
 *  핀 배치:
 *    OUT1  PD13
 *    OUT2  PF1
 *    OUT3  PB8
 *    OUT4  PG9
 *
 *  사용 예:
 *    GPIO_Out_Init();
 *    GPIO_Out_Set(GPIO_OUT1);          // HIGH
 *    GPIO_Out_Reset(GPIO_OUT2);        // LOW
 *    GPIO_Out_Toggle(GPIO_OUT3);       // 토글
 *    GPIO_Out_Write(GPIO_OUT4, 1);     // 값으로 쓰기
 */

#ifndef GPIO_OUT_H
#define GPIO_OUT_H

#include <stdint.h>

/** 출력 핀 식별자 */
typedef enum {
    GPIO_OUT1 = 0,   /* PD13 */
    GPIO_OUT2 = 1,   /* PF1  */
    GPIO_OUT3 = 2,   /* PB8  */
    GPIO_OUT4 = 3,   /* PG9  */
} GPIO_OutPin_t;

/**
 * @brief  4개 핀 GPIO Push-Pull 출력 초기화 (초기값 LOW).
 *         다른 드라이버보다 먼저 호출 가능.
 */
void GPIO_Out_Init(void);

/** @brief  핀을 HIGH로 설정. */
void GPIO_Out_Set(GPIO_OutPin_t pin);

/** @brief  핀을 LOW로 설정. */
void GPIO_Out_Reset(GPIO_OutPin_t pin);

/** @brief  핀 레벨 토글. */
void GPIO_Out_Toggle(GPIO_OutPin_t pin);

/**
 * @brief  핀에 값 쓰기.
 * @param  val  0 = LOW, 그 외 = HIGH
 */
void GPIO_Out_Write(GPIO_OutPin_t pin, uint8_t val);

/** @brief  현재 출력 레벨 읽기. @return 0 또는 1 */
uint8_t GPIO_Out_Read(GPIO_OutPin_t pin);

#endif /* GPIO_OUT_H */
