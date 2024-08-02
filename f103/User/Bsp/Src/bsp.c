/**
 * @file    bsp.c
 * @author  Deadline039
 * @brief   bsp层配置程序
 * @version 0.1
 * @date    2024-07-29
 */

#include "bsp.h"

static void system_clock_config(void) {
    HAL_StatusTypeDef ret = HAL_OK;
    RCC_OscInitTypeDef rcc_osc_init = {0};
    RCC_ClkInitTypeDef rcc_clk_init = {0};

    rcc_osc_init.OscillatorType = RCC_OSCILLATORTYPE_HSE; /* 选择要配置HSE */
    rcc_osc_init.HSEState = RCC_HSE_ON;                   /* 打开HSE */
    rcc_osc_init.HSEPredivValue = RCC_HSE_PREDIV_DIV1; /* HSE预分频系数 */
    rcc_osc_init.PLL.PLLState = RCC_PLL_ON;            /* 打开PLL */
    rcc_osc_init.PLL.PLLSource = RCC_PLLSOURCE_HSE; /* PLL时钟源选择HSE */
    rcc_osc_init.PLL.PLLMUL = RCC_PLL_MUL9;         /* PLL倍频系数 */
    ret = HAL_RCC_OscConfig(&rcc_osc_init);         /* 初始化 */

    if (ret != HAL_OK) {
        /* 时钟初始化失败,
         * 之后的程序将可能无法正常执行, 可以在这里加入自己的处理 */
        while (1)
            ;
    }

    /* 选中PLL作为系统时钟源并且配置HCLK,PCLK1和PCLK2*/
    rcc_clk_init.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                              RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
    rcc_clk_init.SYSCLKSource =
        RCC_SYSCLKSOURCE_PLLCLK; /* 设置系统时钟来自PLL */
    rcc_clk_init.AHBCLKDivider = RCC_SYSCLK_DIV1; /* AHB分频系数为1 */
    rcc_clk_init.APB1CLKDivider = RCC_HCLK_DIV2;  /* APB1分频系数为2 */
    rcc_clk_init.APB2CLKDivider = RCC_HCLK_DIV1;  /* APB2分频系数为1 */
    ret = HAL_RCC_ClockConfig(
        &rcc_clk_init,
        FLASH_LATENCY_2); /* 同时设置FLASH延时周期为2WS, 也就是3个CPU周期。 */

    if (ret != HAL_OK) {
        /* 时钟初始化失败,
         * 之后的程序将可能无法正常执行, 可以在这里加入自己的处理 */
        while (1)
            ;
    }
}

/**
 * @brief bsp层驱动初始化
 *
 */
void bsp_init(void) {
    HAL_Init();
    system_clock_config();
    led_init();
    uart_init(&usart1_handle, 1000000, UART_WORDLENGTH_8B, UART_STOPBITS_1,
              UART_PARITY_NONE, UART_HWCONTROL_NONE, UART_MODE_TX_RX);
    uart_init(&usart2_handle, 1000000, UART_WORDLENGTH_8B, UART_STOPBITS_1,
              UART_PARITY_NONE, UART_HWCONTROL_NONE, UART_MODE_TX_RX);
    uart_init(&usart3_handle, 1000000, UART_WORDLENGTH_8B, UART_STOPBITS_1,
              UART_PARITY_NONE, UART_HWCONTROL_NONE, UART_MODE_TX_RX);
    uart_init(&uart4_handle, 1000000, UART_WORDLENGTH_8B, UART_STOPBITS_1,
              UART_PARITY_NONE, UART_HWCONTROL_NONE, UART_MODE_TX_RX);
    key_init();
}

/**
 * @brief bsp层初始化错误处理
 *
 */
void bsp_error_handle(void) {
#ifdef DEBUG
    __asm("BKPT 0");
#endif /* DEBUG */

    while (1)
        ;
}
