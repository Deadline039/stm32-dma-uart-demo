/**
 * @file    led.h
 * @author  Deadline039
 * @brief   On board LED.
 * @version 1.1
 * @date    2024-07-29
 */

#ifndef __LED_H
#define __LED_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "bsp_core.h"

#define LED0_GPIO_PORT     GPIOA
#define LED0_GPIO_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()
#define LED0_GPIO_PIN      GPIO_PIN_8

#define LED1_GPIO_PORT     GPIOD
#define LED1_GPIO_ENABLE() __HAL_RCC_GPIOD_CLK_ENABLE()
#define LED1_GPIO_PIN      GPIO_PIN_2

/* LED0 */
#define LED0(x)                                                                \
    x ? HAL_GPIO_WritePin(LED0_GPIO_PORT, LED0_GPIO_PIN, GPIO_PIN_SET)         \
      : HAL_GPIO_WritePin(LED0_GPIO_PORT, LED0_GPIO_PIN, GPIO_PIN_RESET)
#define LED0_ON()                                                              \
    HAL_GPIO_WritePin(LED0_GPIO_PORT, LED0_GPIO_PIN, GPIO_PIN_RESET)
#define LED0_OFF()                                                             \
    HAL_GPIO_WritePin(LED0_GPIO_PORT, LED0_GPIO_PIN, GPIO_PIN_SET)
#define LED0_TOGGLE() HAL_GPIO_TogglePin(LED0_GPIO_PORT, LED0_GPIO_PIN)

/* LED1 */
#define LED1(x)                                                                \
    x ? HAL_GPIO_WritePin(LED1_GPIO_PORT, LED1_GPIO_PIN, GPIO_PIN_SET)         \
      : HAL_GPIO_WritePin(LED1_GPIO_PORT, LED1_GPIO_PIN, GPIO_PIN_RESET)
#define LED1_ON()                                                              \
    HAL_GPIO_WritePin(LED1_GPIO_PORT, LED1_GPIO_PIN, GPIO_PIN_RESET)
#define LED1_OFF()                                                             \
    HAL_GPIO_WritePin(LED1_GPIO_PORT, LED1_GPIO_PIN, GPIO_PIN_SET)
#define LED1_TOGGLE() HAL_GPIO_TogglePin(LED1_GPIO_PORT, LED1_GPIO_PIN)

void led_init(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __LED_H */
