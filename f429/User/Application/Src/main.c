/**
 * @file    main.c
 * @author  Deadline039
 * @brief   主函数
 * @version 1.0
 * @date    2024-07-29
 */

#include "includes.h"

static uint8_t buf1[300];
static uint8_t buf2[300];
static uint8_t buf3[300];
static uint8_t buf4[300];
static uint8_t buf5[300];
static uint8_t buf6[300];

/**
 * @brief 主函数
 *
 * @return 退出码
 */
int main(void) {
    bsp_init();

    /**
     * 串口回环
     * 电脑 TX      ----> 串口1 RX
     * 串口1 TX     ----> 串口2 RX
     * 串口2 TX     ----> 串口3 RX
     * 串口3 TX     ----> 串口4 RX
     * 串口4 TX     ----> 串口5 RX
     * 串口5 TX     ----> 串口6 RX
     * 串口6 TX     ----> 电脑 RX
     */

    /**
     * 接线:
     * PC TX    ---->   PA10
     * PA9      ---->   PA3
     * PA2      ---->   PB11
     * PB10     ---->   PC11
     * PC10     ---->   PD2
     * PC12     ---->   PC7
     * PC6      ---->   PC RX
     */

    uint32_t len1;
    uint32_t len2;
    uint32_t len3;
    uint32_t len4;
    uint32_t len5;
    uint32_t len6;

    while (1) {

        uart_dmatx_send(&usart1_handle);
        len1 = uart_dmarx_read(&usart1_handle, buf1, sizeof(buf1));
        if (len1 > 0) {
            uart_dmatx_write(&usart1_handle, buf1, len1);
        }

        uart_dmatx_send(&usart2_handle);
        len2 = uart_dmarx_read(&usart2_handle, buf2, sizeof(buf2));
        if (len2 > 0) {
            uart_dmatx_write(&usart2_handle, buf2, len2);
        }

        uart_dmatx_send(&usart3_handle);
        len3 = uart_dmarx_read(&usart3_handle, buf3, sizeof(buf3));
        if (len3 > 0) {
            uart_dmatx_write(&usart3_handle, buf3, len3);
        }

        uart_dmatx_send(&uart4_handle);
        len4 = uart_dmarx_read(&uart4_handle, buf4, sizeof(buf4));
        if (len4 > 0) {
            uart_dmatx_write(&uart4_handle, buf4, len4);
        }

        uart_dmatx_send(&uart5_handle);
        len5 = uart_dmarx_read(&uart5_handle, buf5, sizeof(buf5));
        if (len5 > 0) {
            uart_dmatx_write(&uart5_handle, buf5, len5);
        }

        uart_dmatx_send(&usart6_handle);
        len6 = uart_dmarx_read(&usart6_handle, buf6, sizeof(buf6));
        if (len6 > 0) {
            uart_dmatx_write(&usart6_handle, buf6, len6);
        }
    }
}