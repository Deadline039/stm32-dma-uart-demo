/**
 * @file    main.c
 * @author  Deadline039
 * @brief   主函数
 * @version 1.0
 * @date    2024-07-29
 */

#include "includes.h"

uint8_t buffer[200];

/**
 * @brief 主函数
 *
 * @return 退出码
 */
int main(void) {
    bsp_init();
    uint32_t len = 0;
    while (1) {
        len = uart_dmarx_read(&uart4_handle, buffer, sizeof(buffer));
        uart_dmatx_send(&uart4_handle);
        if (len > 0) {
            uart_dmatx_write(&uart4_handle, buffer, len);
        }
    }
}