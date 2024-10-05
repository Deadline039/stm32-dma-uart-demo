/**
 * @file    dma_uart.c
 * @author  Deadline039
 * @brief   使用DMA+半满中断+满中断+空闲中断实现高可靠串口数据收发
 * @version 1.1
 * @date    2024-01-18
 * @note    stm32f429串口DMA配置文件
 * @ref     https://github.com/Prry/stm32-uart-dma
 *          https://gitee.com/wei513723/stm32-stable-uart-transmit-receive
 */

#include "bsp_core.h"
#include "bsp_uart.h"
#include "ring_fifo.h"

#include <string.h>

void uart_dmatx_clear_tc_flag(UART_HandleTypeDef *huart);

void uart_dmarx_halfdone_callback(UART_HandleTypeDef *huart);
void uart_dmarx_done_callback(UART_HandleTypeDef *huart);

/**
 * @brief 串口发送缓冲区
 */
typedef struct {
    uint8_t *send_buf;     /*!< 发送数据缓冲区 */
    uint32_t head_ptr;     /*!< 位置指针, 用来控制DMA传输的长度 */
    size_t send_buf_size;  /*!< 缓冲区大小, 避免溢出 */
    __IO uint32_t tc_flag; /*!< 是否发送完成, 0-未完成; 1-完成 */
} uart_tx_buf_t;

/**
 * @brief 串口接收缓冲区
 *
 */
typedef struct {
    ring_fifo_t *rx_fifo; /*!< 接收FIFO */
    uint8_t *rx_fifo_buf; /*!< FIFO数据存储区 */
    uint8_t *recv_buf;    /*!< DMA接收数据缓冲区 */
    uint32_t head_ptr;    /*!< 位置指针, 用来控制半满和溢出 */
} uart_rx_fifo_t;

#if (USART1_ENABLE == 1)

#if (USART1_USE_DMA_TX == 1)

static DMA_HandleTypeDef usart1_dmatx_handle;
static uart_tx_buf_t usart1_tx_buf;

/**
 * @brief 串口1发送DMA初始化
 *
 */
void usart1_dmatx_init(void) {
    HAL_StatusTypeDef res = HAL_OK;

    usart1_dmatx_handle.Instance = DMA2_Stream7;
    usart1_dmatx_handle.Init.Channel = DMA_CHANNEL_4;
    usart1_dmatx_handle.Init.Direction = DMA_MEMORY_TO_PERIPH;
    usart1_dmatx_handle.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    usart1_dmatx_handle.Init.MemInc = DMA_MINC_ENABLE;
    usart1_dmatx_handle.Init.Mode = DMA_NORMAL;
    usart1_dmatx_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    usart1_dmatx_handle.Init.PeriphInc = DMA_PINC_DISABLE;
    usart1_dmatx_handle.Init.Priority = USART1_DMA_TX_PRIORITY;

    usart1_tx_buf.send_buf =
        (uint8_t *)malloc(sizeof(uint8_t) * USART1_TX_BUF_SIZE);
    usart1_tx_buf.send_buf_size = USART1_TX_BUF_SIZE;
#ifdef DEBUG
    assert(usart1_tx_buf.send_buf != NULL);
#endif /* DEBUG */

    usart1_tx_buf.tc_flag = 1;

    __HAL_RCC_DMA2_CLK_ENABLE();
    res = HAL_DMA_Init(&usart1_dmatx_handle);
#ifdef DEBUG
    assert(res == HAL_OK);
#endif /* DEBUG */

    __HAL_LINKDMA(&usart1_handle, hdmatx, usart1_dmatx_handle);

    HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, USART1_DMA_TX_IT_PREEMPT,
                         USART1_DMA_TX_IT_SUB);
    HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);

#if (USE_HAL_UART_REGISTER_CALLBACKS == 1)
    HAL_UART_RegisterCallback(&usart1_handle, HAL_UART_TX_COMPLETE_CB_ID,
                              uart_dmatx_clear_tc_flag);
#endif /* USE_HAL_UART_REGISTER_CALLBACKS == 1 */
}

/**
 * @brief 串口1发送中断句柄
 *
 */
void DMA2_Stream7_IRQHandler(void) {
    HAL_DMA_IRQHandler(&usart1_dmatx_handle);
}
#endif /* USART1_USE_DMA_TX == 1 */

#if (USART1_USE_DMA_RX == 1)

static DMA_HandleTypeDef usart1_dmarx_handle;
static uart_rx_fifo_t usart1_rx_fifo;

/**
 * @brief 串口1接收DMA初始化
 *
 */
void usart1_dmarx_init(void) {
    HAL_StatusTypeDef res = HAL_OK;

    usart1_dmarx_handle.Instance = DMA2_Stream5;
    usart1_dmarx_handle.Init.Channel = DMA_CHANNEL_4;
    usart1_dmarx_handle.Init.Direction = DMA_PERIPH_TO_MEMORY;
    usart1_dmarx_handle.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    usart1_dmarx_handle.Init.MemInc = DMA_MINC_ENABLE;
    usart1_dmarx_handle.Init.Mode = DMA_CIRCULAR;
    usart1_dmarx_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    usart1_dmarx_handle.Init.PeriphInc = DMA_PINC_DISABLE;
    usart1_dmarx_handle.Init.Priority = USART1_DMA_RX_PRIORITY;

    usart1_rx_fifo.head_ptr = 0;
    usart1_rx_fifo.recv_buf =
        (uint8_t *)malloc(sizeof(uint8_t) * USART1_RX_BUF_SIZE);
#ifdef DEBUG
    assert(usart1_rx_fifo.recv_buf != NULL);
#endif /* DEBUG */

    usart1_rx_fifo.rx_fifo_buf =
        (uint8_t *)malloc(sizeof(uint8_t) * USART1_RX_FIFO_SZIE);
#ifdef DEBUG
    assert(usart1_rx_fifo.rx_fifo_buf != NULL);
#endif /* DEBUG */

    usart1_rx_fifo.rx_fifo =
        ring_fifo_init(usart1_rx_fifo.rx_fifo_buf,
                       sizeof(uint8_t) * USART1_RX_FIFO_SZIE, RF_TYPE_STREAM);
#ifdef DEBUG
    assert(usart1_rx_fifo.rx_fifo != NULL);
#endif /* DEBUG */

    __HAL_RCC_DMA2_CLK_ENABLE();
    res = HAL_DMA_Init(&usart1_dmarx_handle);
#ifdef DEBUG
    assert(res == HAL_OK);
#endif /* DEBUG */

    __HAL_LINKDMA(&usart1_handle, hdmarx, usart1_dmarx_handle);

    HAL_NVIC_SetPriority(DMA2_Stream5_IRQn, USART1_DMA_RX_IT_PREEMPT,
                         USART1_DMA_RX_IT_SUB);
    HAL_NVIC_EnableIRQ(DMA2_Stream5_IRQn);

#if USART1_USE_IDLE_IT
    __HAL_UART_ENABLE_IT(&usart1_handle, UART_IT_IDLE);
    __HAL_UART_CLEAR_IDLEFLAG(&usart1_handle);
#endif /* USART1_USE_IDLE_IT */
    HAL_UART_Receive_DMA(&usart1_handle, (uint8_t *)usart1_rx_fifo.recv_buf,
                         USART1_RX_BUF_SIZE);

#if (USE_HAL_UART_REGISTER_CALLBACKS == 1)
    HAL_UART_RegisterCallback(&usart1_handle, HAL_UART_RX_HALFCOMPLETE_CB_ID,
                              uart_dmarx_halfdone_callback);
    HAL_UART_RegisterCallback(&usart1_handle, HAL_UART_RX_COMPLETE_CB_ID,
                              uart_dmarx_done_callback);
#endif /* USE_HAL_UART_REGISTER_CALLBACKS == 1 */
}

/**
 * @brief 串口1接收中断句柄
 *
 */
void DMA2_Stream5_IRQHandler(void) {
    HAL_DMA_IRQHandler(&usart1_dmarx_handle);
}
#endif /* USART1_USE_DMA_RX == 1 */

#endif /* USART1_ENABLE == 1 */

#if (USART2_ENABLE == 1)

#if (USART2_USE_DMA_TX == 1)

static DMA_HandleTypeDef usart2_dmatx_handle;
static uart_tx_buf_t usart2_tx_buf;

/**
 * @brief 串口2发送DMA初始化
 *
 */
void usart2_dmatx_init(void) {
    HAL_StatusTypeDef res = HAL_OK;

    usart2_dmatx_handle.Instance = DMA1_Stream6;
    usart2_dmatx_handle.Init.Channel = DMA_CHANNEL_4;
    usart2_dmatx_handle.Init.Direction = DMA_MEMORY_TO_PERIPH;
    usart2_dmatx_handle.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    usart2_dmatx_handle.Init.MemInc = DMA_MINC_ENABLE;
    usart2_dmatx_handle.Init.Mode = DMA_NORMAL;
    usart2_dmatx_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    usart2_dmatx_handle.Init.PeriphInc = DMA_PINC_DISABLE;
    usart2_dmatx_handle.Init.Priority = USART2_DMA_TX_PRIORITY;

    usart2_tx_buf.send_buf =
        (uint8_t *)malloc(sizeof(uint8_t) * USART2_TX_BUF_SIZE);
    usart2_tx_buf.send_buf_size = USART2_TX_BUF_SIZE;
#ifdef DEBUG
    assert(usart2_tx_buf.send_buf != NULL);
#endif /* DEBUG */

    usart2_tx_buf.tc_flag = 1;

    __HAL_RCC_DMA1_CLK_ENABLE();
    res = HAL_DMA_Init(&usart2_dmatx_handle);
#ifdef DEBUG
    assert(res == HAL_OK);
#endif /* DEBUG */

    __HAL_LINKDMA(&usart2_handle, hdmatx, usart2_dmatx_handle);

    HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, USART2_DMA_TX_IT_PREEMPT,
                         USART2_DMA_TX_IT_SUB);
    HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);

#if (USE_HAL_UART_REGISTER_CALLBACKS == 1)
    HAL_UART_RegisterCallback(&usart2_handle, HAL_UART_TX_COMPLETE_CB_ID,
                              uart_dmatx_clear_tc_flag);
#endif /* USE_HAL_UART_REGISTER_CALLBACKS == 1 */
}

/**
 * @brief 串口2发送中断句柄
 *
 */
void DMA1_Stream6_IRQHandler(void) {
    HAL_DMA_IRQHandler(&usart2_dmatx_handle);
}
#endif /* USART2_USE_DMA_TX == 1 */

#if (USART2_USE_DMA_RX == 1)

static DMA_HandleTypeDef usart2_dmarx_handle;
static uart_rx_fifo_t usart2_rx_fifo;

/**
 * @brief 串口2接收DMA初始化
 *
 */
void usart2_dmarx_init(void) {
    HAL_StatusTypeDef res = HAL_OK;

    usart2_dmarx_handle.Instance = DMA1_Stream5;
    usart2_dmarx_handle.Init.Channel = DMA_CHANNEL_4;
    usart2_dmarx_handle.Init.Direction = DMA_PERIPH_TO_MEMORY;
    usart2_dmarx_handle.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    usart2_dmarx_handle.Init.MemInc = DMA_MINC_ENABLE;
    usart2_dmarx_handle.Init.Mode = DMA_CIRCULAR;
    usart2_dmarx_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    usart2_dmarx_handle.Init.PeriphInc = DMA_PINC_DISABLE;
    usart2_dmarx_handle.Init.Priority = USART2_DMA_RX_PRIORITY;

    usart2_rx_fifo.head_ptr = 0;
    usart2_rx_fifo.recv_buf =
        (uint8_t *)malloc(sizeof(uint8_t) * USART2_RX_BUF_SIZE);
#ifdef DEBUG
    assert(usart2_rx_fifo.recv_buf != NULL);
#endif /* DEBUG */

    usart2_rx_fifo.rx_fifo_buf =
        (uint8_t *)malloc(sizeof(uint8_t) * USART2_RX_FIFO_SZIE);
#ifdef DEBUG
    assert(usart2_rx_fifo.rx_fifo_buf != NULL);
#endif /* DEBUG */

    usart2_rx_fifo.rx_fifo =
        ring_fifo_init(usart2_rx_fifo.rx_fifo_buf,
                       sizeof(uint8_t) * USART2_RX_FIFO_SZIE, RF_TYPE_STREAM);
#ifdef DEBUG
    assert(usart2_rx_fifo.rx_fifo != NULL);
#endif /* DEBUG */

    __HAL_RCC_DMA1_CLK_ENABLE();
    res = HAL_DMA_Init(&usart2_dmarx_handle);
#ifdef DEBUG
    assert(res == HAL_OK);
#endif /* DEBUG */

    __HAL_LINKDMA(&usart2_handle, hdmarx, usart2_dmarx_handle);

    HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, USART2_DMA_RX_IT_PREEMPT,
                         USART2_DMA_RX_IT_SUB);
    HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);

#if USART2_USE_IDLE_IT
    __HAL_UART_ENABLE_IT(&usart2_handle, UART_IT_IDLE);
    __HAL_UART_CLEAR_IDLEFLAG(&usart2_handle);
#endif /* USART2_USE_IDLE_IT */
    HAL_UART_Receive_DMA(&usart2_handle, (uint8_t *)usart2_rx_fifo.recv_buf,
                         USART2_RX_BUF_SIZE);

#if (USE_HAL_UART_REGISTER_CALLBACKS == 1)
    HAL_UART_RegisterCallback(&usart2_handle, HAL_UART_RX_HALFCOMPLETE_CB_ID,
                              uart_dmarx_halfdone_callback);
    HAL_UART_RegisterCallback(&usart2_handle, HAL_UART_RX_COMPLETE_CB_ID,
                              uart_dmarx_done_callback);
#endif /* USE_HAL_UART_REGISTER_CALLBACKS == 1 */
}

/**
 * @brief 串口2接收中断句柄
 *
 */
void DMA1_Stream5_IRQHandler(void) {
    HAL_DMA_IRQHandler(&usart2_dmarx_handle);
}
#endif /* USART2_USE_DMA_RX == 1 */

#endif /* USART2_ENABLE == 1 */

#if (USART3_ENABLE == 1)

#if (USART3_USE_DMA_TX == 1)

static DMA_HandleTypeDef usart3_dmatx_handle;
static uart_tx_buf_t usart3_tx_buf;

/**
 * @brief 串口3发送DMA初始化
 *
 */
void usart3_dmatx_init(void) {
    HAL_StatusTypeDef res = HAL_OK;

    usart3_dmatx_handle.Instance = DMA1_Stream3;
    usart3_dmatx_handle.Init.Channel = DMA_CHANNEL_4;
    usart3_dmatx_handle.Init.Direction = DMA_MEMORY_TO_PERIPH;
    usart3_dmatx_handle.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    usart3_dmatx_handle.Init.MemInc = DMA_MINC_ENABLE;
    usart3_dmatx_handle.Init.Mode = DMA_NORMAL;
    usart3_dmatx_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    usart3_dmatx_handle.Init.PeriphInc = DMA_PINC_DISABLE;
    usart3_dmatx_handle.Init.Priority = USART3_DMA_TX_PRIORITY;

    usart3_tx_buf.send_buf =
        (uint8_t *)malloc(sizeof(uint8_t) * USART3_TX_BUF_SIZE);
    usart3_tx_buf.send_buf_size = USART3_TX_BUF_SIZE;
#ifdef DEBUG
    assert(usart3_tx_buf.send_buf != NULL);
#endif /* DEBUG */

    usart3_tx_buf.tc_flag = 1;

    __HAL_RCC_DMA2_CLK_ENABLE();
    res = HAL_DMA_Init(&usart3_dmatx_handle);
#ifdef DEBUG
    assert(res == HAL_OK);
#endif /* DEBUG */

    __HAL_LINKDMA(&usart3_handle, hdmatx, usart3_dmatx_handle);

    HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, USART3_DMA_TX_IT_PREEMPT,
                         USART3_DMA_TX_IT_SUB);
    HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);

#if (USE_HAL_UART_REGISTER_CALLBACKS == 1)
    HAL_UART_RegisterCallback(&usart3_handle, HAL_UART_TX_COMPLETE_CB_ID,
                              uart_dmatx_clear_tc_flag);
#endif /* USE_HAL_UART_REGISTER_CALLBACKS == 1 */
}

/**
 * @brief 串口3发送中断句柄
 *
 */
void DMA1_Stream3_IRQHandler(void) {
    HAL_DMA_IRQHandler(&usart3_dmatx_handle);
}
#endif /* USART3_USE_DMA_TX == 1 */

#if (USART3_USE_DMA_RX == 1)

static DMA_HandleTypeDef usart3_dmarx_handle;
static uart_rx_fifo_t usart3_rx_fifo;

/**
 * @brief 串口3接收DMA初始化
 *
 */
void usart3_dmarx_init(void) {
    HAL_StatusTypeDef res = HAL_OK;

    usart3_dmarx_handle.Instance = DMA1_Stream1;
    usart3_dmarx_handle.Init.Channel = DMA_CHANNEL_4;
    usart3_dmarx_handle.Init.Direction = DMA_PERIPH_TO_MEMORY;
    usart3_dmarx_handle.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    usart3_dmarx_handle.Init.MemInc = DMA_MINC_ENABLE;
    usart3_dmarx_handle.Init.Mode = DMA_CIRCULAR;
    usart3_dmarx_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    usart3_dmarx_handle.Init.PeriphInc = DMA_PINC_DISABLE;
    usart3_dmarx_handle.Init.Priority = USART3_DMA_RX_PRIORITY;

    usart3_rx_fifo.head_ptr = 0;
    usart3_rx_fifo.recv_buf =
        (uint8_t *)malloc(sizeof(uint8_t) * USART3_RX_BUF_SIZE);
#ifdef DEBUG
    assert(usart3_rx_fifo.recv_buf != NULL);
#endif /* DEBUG */

    usart3_rx_fifo.rx_fifo_buf =
        (uint8_t *)malloc(sizeof(uint8_t) * USART3_RX_FIFO_SZIE);
#ifdef DEBUG
    assert(usart3_rx_fifo.rx_fifo_buf != NULL);
#endif /* DEBUG */

    usart3_rx_fifo.rx_fifo =
        ring_fifo_init(usart3_rx_fifo.rx_fifo_buf,
                       sizeof(uint8_t) * USART3_RX_FIFO_SZIE, RF_TYPE_STREAM);
#ifdef DEBUG
    assert(usart3_rx_fifo.rx_fifo != NULL);
#endif /* DEBUG */

    __HAL_RCC_DMA2_CLK_ENABLE();
    res = HAL_DMA_Init(&usart3_dmarx_handle);
#ifdef DEBUG
    assert(res == HAL_OK);
#endif /* DEBUG */

    __HAL_LINKDMA(&usart3_handle, hdmarx, usart3_dmarx_handle);

    HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, USART3_DMA_RX_IT_PREEMPT,
                         USART3_DMA_RX_IT_SUB);
    HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);

#if USART3_USE_IDLE_IT
    __HAL_UART_ENABLE_IT(&usart3_handle, UART_IT_IDLE);
    __HAL_UART_CLEAR_IDLEFLAG(&usart3_handle);
#endif /* USART3_USE_IDLE_IT */
    HAL_UART_Receive_DMA(&usart3_handle, (uint8_t *)usart3_rx_fifo.recv_buf,
                         USART3_RX_BUF_SIZE);

#if (USE_HAL_UART_REGISTER_CALLBACKS == 1)
    HAL_UART_RegisterCallback(&usart3_handle, HAL_UART_RX_HALFCOMPLETE_CB_ID,
                              uart_dmarx_halfdone_callback);
    HAL_UART_RegisterCallback(&usart3_handle, HAL_UART_RX_COMPLETE_CB_ID,
                              uart_dmarx_done_callback);
#endif /* USE_HAL_UART_REGISTER_CALLBACKS == 1 */
}

/**
 * @brief 串口3接收中断句柄
 *
 */
void DMA1_Stream1_IRQHandler(void) {
    HAL_DMA_IRQHandler(&usart3_dmarx_handle);
}
#endif /* USART3_USE_DMA_RX == 1 */

#endif /* USART3_ENABLE == 1 */

#if (UART4_ENABLE == 1)

#if (UART4_USE_DMA_TX == 1)

static DMA_HandleTypeDef uart4_dmatx_handle;
static uart_tx_buf_t uart4_tx_buf;

/**
 * @brief 串口4发送DMA初始化
 *
 */
void uart4_dmatx_init(void) {
    HAL_StatusTypeDef res = HAL_OK;

    uart4_dmatx_handle.Instance = DMA1_Stream4;
    uart4_dmatx_handle.Init.Channel = DMA_CHANNEL_4;
    uart4_dmatx_handle.Init.Direction = DMA_MEMORY_TO_PERIPH;
    uart4_dmatx_handle.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    uart4_dmatx_handle.Init.MemInc = DMA_MINC_ENABLE;
    uart4_dmatx_handle.Init.Mode = DMA_NORMAL;
    uart4_dmatx_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    uart4_dmatx_handle.Init.PeriphInc = DMA_PINC_DISABLE;
    uart4_dmatx_handle.Init.Priority = UART4_DMA_TX_PRIORITY;

    uart4_tx_buf.send_buf =
        (uint8_t *)malloc(sizeof(uint8_t) * UART4_TX_BUF_SIZE);
    uart4_tx_buf.send_buf_size = UART4_TX_BUF_SIZE;
#ifdef DEBUG
    assert(uart4_tx_buf.send_buf != NULL);
#endif /* DEBUG */

    uart4_tx_buf.tc_flag = 1;

    __HAL_RCC_DMA1_CLK_ENABLE();
    res = HAL_DMA_Init(&uart4_dmatx_handle);
#ifdef DEBUG
    assert(res == HAL_OK);
#endif /* DEBUG */

    __HAL_LINKDMA(&uart4_handle, hdmatx, uart4_dmatx_handle);

    HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, UART4_DMA_TX_IT_PREEMPT,
                         UART4_DMA_TX_IT_SUB);
    HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);

#if (USE_HAL_UART_REGISTER_CALLBACKS == 1)
    HAL_UART_RegisterCallback(&uart4_handle, HAL_UART_TX_COMPLETE_CB_ID,
                              uart_dmatx_clear_tc_flag);
#endif /* USE_HAL_UART_REGISTER_CALLBACKS == 1 */
}

/**
 * @brief 串口4发送中断句柄
 *
 */
void DMA1_Stream4_IRQHandler(void) {
    HAL_DMA_IRQHandler(&uart4_dmatx_handle);
}
#endif /* UART4_USE_DMA_TX == 1 */

#if (UART4_USE_DMA_RX == 1)

static DMA_HandleTypeDef uart4_dmarx_handle;
static uart_rx_fifo_t uart4_rx_fifo;

/**
 * @brief 串口4接收DMA初始化
 *
 */
void uart4_dmarx_init(void) {
    HAL_StatusTypeDef res = HAL_OK;

    uart4_dmarx_handle.Instance = DMA1_Stream2;
    uart4_dmarx_handle.Init.Channel = DMA_CHANNEL_4;
    uart4_dmarx_handle.Init.Direction = DMA_PERIPH_TO_MEMORY;
    uart4_dmarx_handle.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    uart4_dmarx_handle.Init.MemInc = DMA_MINC_ENABLE;
    uart4_dmarx_handle.Init.Mode = DMA_CIRCULAR;
    uart4_dmarx_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    uart4_dmarx_handle.Init.PeriphInc = DMA_PINC_DISABLE;
    uart4_dmarx_handle.Init.Priority = UART4_DMA_RX_PRIORITY;

    uart4_rx_fifo.head_ptr = 0;
    uart4_rx_fifo.recv_buf =
        (uint8_t *)malloc(sizeof(uint8_t) * UART4_RX_BUF_SIZE);
#ifdef DEBUG
    assert(uart4_rx_fifo.recv_buf != NULL);
#endif /* DEBUG */

    uart4_rx_fifo.rx_fifo_buf =
        (uint8_t *)malloc(sizeof(uint8_t) * UART4_RX_FIFO_SZIE);
#ifdef DEBUG
    assert(uart4_rx_fifo.rx_fifo_buf != NULL);
#endif /* DEBUG */

    uart4_rx_fifo.rx_fifo =
        ring_fifo_init(uart4_rx_fifo.rx_fifo_buf,
                       sizeof(uint8_t) * UART4_RX_FIFO_SZIE, RF_TYPE_STREAM);
#ifdef DEBUG
    assert(uart4_rx_fifo.rx_fifo != NULL);
#endif /* DEBUG */

    __HAL_RCC_DMA1_CLK_ENABLE();
    res = HAL_DMA_Init(&uart4_dmarx_handle);
#ifdef DEBUG
    assert(res == HAL_OK);
#endif /* DEBUG */

    __HAL_LINKDMA(&uart4_handle, hdmarx, uart4_dmarx_handle);

    HAL_NVIC_SetPriority(DMA1_Stream2_IRQn, UART4_DMA_RX_IT_PREEMPT,
                         UART4_DMA_RX_IT_SUB);
    HAL_NVIC_EnableIRQ(DMA1_Stream2_IRQn);

#if UART4_USE_IDLE_IT
    __HAL_UART_ENABLE_IT(&uart4_handle, UART_IT_IDLE);
    __HAL_UART_CLEAR_IDLEFLAG(&uart4_handle);
#endif /* UART4_USE_IDLE_IT */
    HAL_UART_Receive_DMA(&uart4_handle, (uint8_t *)uart4_rx_fifo.recv_buf,
                         UART4_RX_BUF_SIZE);

#if (USE_HAL_UART_REGISTER_CALLBACKS == 1)
    HAL_UART_RegisterCallback(&uart4_handle, HAL_UART_RX_HALFCOMPLETE_CB_ID,
                              uart_dmarx_halfdone_callback);
    HAL_UART_RegisterCallback(&uart4_handle, HAL_UART_RX_COMPLETE_CB_ID,
                              uart_dmarx_done_callback);
#endif /* USE_HAL_UART_REGISTER_CALLBACKS == 1 */
}

/**
 * @brief 串口4接收中断句柄
 *
 */
void DMA1_Stream2_IRQHandler(void) {
    HAL_DMA_IRQHandler(&uart4_dmarx_handle);
}
#endif /* UART4_USE_DMA_RX == 1 */

#endif /* UART4_ENABLE == 1 */

#if (UART5_ENABLE == 1)

#if (UART5_USE_DMA_TX == 1)

static DMA_HandleTypeDef uart5_dmatx_handle;
static uart_tx_buf_t uart5_tx_buf;

/**
 * @brief 串口5发送DMA初始化
 *
 */
void uart5_dmatx_init(void) {
    HAL_StatusTypeDef res = HAL_OK;

    uart5_dmatx_handle.Instance = DMA1_Stream7;
    uart5_dmatx_handle.Init.Channel = DMA_CHANNEL_4;
    uart5_dmatx_handle.Init.Direction = DMA_MEMORY_TO_PERIPH;
    uart5_dmatx_handle.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    uart5_dmatx_handle.Init.MemInc = DMA_MINC_ENABLE;
    uart5_dmatx_handle.Init.Mode = DMA_NORMAL;
    uart5_dmatx_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    uart5_dmatx_handle.Init.PeriphInc = DMA_PINC_DISABLE;
    uart5_dmatx_handle.Init.Priority = UART5_DMA_TX_PRIORITY;

    uart5_tx_buf.send_buf =
        (uint8_t *)malloc(sizeof(uint8_t) * UART5_TX_BUF_SIZE);
    uart5_tx_buf.send_buf_size = UART5_TX_BUF_SIZE;
#ifdef DEBUG
    assert(uart5_tx_buf.send_buf != NULL);
#endif /* DEBUG */

    uart5_tx_buf.tc_flag = 1;

    __HAL_RCC_DMA1_CLK_ENABLE();
    res = HAL_DMA_Init(&uart5_dmatx_handle);
#ifdef DEBUG
    assert(res == HAL_OK);
#endif /* DEBUG */

    __HAL_LINKDMA(&uart5_handle, hdmatx, uart5_dmatx_handle);

    HAL_NVIC_SetPriority(DMA1_Stream7_IRQn, UART5_DMA_TX_IT_PREEMPT,
                         UART5_DMA_TX_IT_SUB);
    HAL_NVIC_EnableIRQ(DMA1_Stream7_IRQn);

#if (USE_HAL_UART_REGISTER_CALLBACKS == 1)
    HAL_UART_RegisterCallback(&uart5_handle, HAL_UART_TX_COMPLETE_CB_ID,
                              uart_dmatx_clear_tc_flag);
#endif /* USE_HAL_UART_REGISTER_CALLBACKS == 1 */
}

/**
 * @brief 串口5发送中断句柄
 *
 */
void DMA1_Stream7_IRQHandler(void) {
    HAL_DMA_IRQHandler(&uart5_dmatx_handle);
}
#endif /* UART5_USE_DMA_TX == 1 */

#if (UART5_USE_DMA_RX == 1)

static DMA_HandleTypeDef uart5_dmarx_handle;
static uart_rx_fifo_t uart5_rx_fifo;

/**
 * @brief 串口5接收DMA初始化
 *
 */
void uart5_dmarx_init(void) {
    HAL_StatusTypeDef res = HAL_OK;

    uart5_dmarx_handle.Instance = DMA1_Stream0;
    uart5_dmarx_handle.Init.Channel = DMA_CHANNEL_4;
    uart5_dmarx_handle.Init.Direction = DMA_PERIPH_TO_MEMORY;
    uart5_dmarx_handle.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    uart5_dmarx_handle.Init.MemInc = DMA_MINC_ENABLE;
    uart5_dmarx_handle.Init.Mode = DMA_CIRCULAR;
    uart5_dmarx_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    uart5_dmarx_handle.Init.PeriphInc = DMA_PINC_DISABLE;
    uart5_dmarx_handle.Init.Priority = UART5_DMA_RX_PRIORITY;

    uart5_rx_fifo.head_ptr = 0;
    uart5_rx_fifo.recv_buf =
        (uint8_t *)malloc(sizeof(uint8_t) * UART5_RX_BUF_SIZE);
#ifdef DEBUG
    assert(uart5_rx_fifo.recv_buf != NULL);
#endif /* DEBUG */

    uart5_rx_fifo.rx_fifo_buf =
        (uint8_t *)malloc(sizeof(uint8_t) * UART5_RX_FIFO_SZIE);
#ifdef DEBUG
    assert(uart5_rx_fifo.rx_fifo_buf != NULL);
#endif /* DEBUG */

    uart5_rx_fifo.rx_fifo =
        ring_fifo_init(uart5_rx_fifo.rx_fifo_buf,
                       sizeof(uint8_t) * UART5_RX_FIFO_SZIE, RF_TYPE_STREAM);
#ifdef DEBUG
    assert(uart5_rx_fifo.rx_fifo != NULL);
#endif /* DEBUG */

    __HAL_RCC_DMA1_CLK_ENABLE();
    res = HAL_DMA_Init(&uart5_dmarx_handle);
#ifdef DEBUG
    assert(res == HAL_OK);
#endif /* DEBUG */

    __HAL_LINKDMA(&uart5_handle, hdmarx, uart5_dmarx_handle);

    HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, UART5_DMA_RX_IT_PREEMPT,
                         UART5_DMA_RX_IT_SUB);
    HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);

#if UART5_USE_IDLE_IT
    __HAL_UART_ENABLE_IT(&uart5_handle, UART_IT_IDLE);
    __HAL_UART_CLEAR_IDLEFLAG(&uart5_handle);
#endif /* UART5_USE_IDLE_IT */
    HAL_UART_Receive_DMA(&uart5_handle, (uint8_t *)uart5_rx_fifo.recv_buf,
                         UART5_RX_BUF_SIZE);

#if (USE_HAL_UART_REGISTER_CALLBACKS == 1)
    HAL_UART_RegisterCallback(&uart5_handle, HAL_UART_RX_HALFCOMPLETE_CB_ID,
                              uart_dmarx_halfdone_callback);
    HAL_UART_RegisterCallback(&uart5_handle, HAL_UART_RX_COMPLETE_CB_ID,
                              uart_dmarx_done_callback);
#endif /* USE_HAL_UART_REGISTER_CALLBACKS == 1 */
}

/**
 * @brief 串口5接收中断句柄
 *
 */
void DMA1_Stream0_IRQHandler(void) {
    HAL_DMA_IRQHandler(&uart5_dmarx_handle);
}
#endif /* UART5_USE_DMA_RX == 1 */

#endif /* UART5_ENABLE == 1 */

#if (USART6_ENABLE == 1)

#if (USART6_USE_DMA_TX == 1)

static DMA_HandleTypeDef usart6_dmatx_handle;
static uart_tx_buf_t usart6_tx_buf;

/**
 * @brief 串口6发送DMA初始化
 *
 */
void usart6_dmatx_init(void) {
    HAL_StatusTypeDef res = HAL_OK;

    usart6_dmatx_handle.Instance = DMA2_Stream6;
    usart6_dmatx_handle.Init.Channel = DMA_CHANNEL_5;
    usart6_dmatx_handle.Init.Direction = DMA_MEMORY_TO_PERIPH;
    usart6_dmatx_handle.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    usart6_dmatx_handle.Init.MemInc = DMA_MINC_ENABLE;
    usart6_dmatx_handle.Init.Mode = DMA_NORMAL;
    usart6_dmatx_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    usart6_dmatx_handle.Init.PeriphInc = DMA_PINC_DISABLE;
    usart6_dmatx_handle.Init.Priority = USART6_DMA_TX_PRIORITY;

    usart6_tx_buf.send_buf =
        (uint8_t *)malloc(sizeof(uint8_t) * USART6_TX_BUF_SIZE);
    usart6_tx_buf.send_buf_size = USART6_TX_BUF_SIZE;
#ifdef DEBUG
    assert(usart6_tx_buf.send_buf != NULL);
#endif /* DEBUG */

    usart6_tx_buf.tc_flag = 1;

    __HAL_RCC_DMA2_CLK_ENABLE();
    res = HAL_DMA_Init(&usart6_dmatx_handle);
#ifdef DEBUG
    assert(res == HAL_OK);
#endif /* DEBUG */

    __HAL_LINKDMA(&usart6_handle, hdmatx, usart6_dmatx_handle);

    HAL_NVIC_SetPriority(DMA2_Stream6_IRQn, USART6_DMA_TX_IT_PREEMPT,
                         USART6_DMA_TX_IT_SUB);
    HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);

#if (USE_HAL_UART_REGISTER_CALLBACKS == 1)
    HAL_UART_RegisterCallback(&usart6_handle, HAL_UART_TX_COMPLETE_CB_ID,
                              uart_dmatx_clear_tc_flag);
#endif /* USE_HAL_UART_REGISTER_CALLBACKS == 1 */
}

/**
 * @brief 串口6发送中断句柄
 *
 */
void DMA2_Stream6_IRQHandler(void) {
    HAL_DMA_IRQHandler(&usart6_dmatx_handle);
}
#endif /* USART6_USE_DMA_TX == 1 */

#if (USART6_USE_DMA_RX == 1)

static DMA_HandleTypeDef usart6_dmarx_handle;
static uart_rx_fifo_t usart6_rx_fifo;

/**
 * @brief 串口6接收DMA初始化
 *
 */
void usart6_dmarx_init(void) {
    HAL_StatusTypeDef res = HAL_OK;

    usart6_dmarx_handle.Instance = DMA2_Stream1;
    usart6_dmarx_handle.Init.Channel = DMA_CHANNEL_5;
    usart6_dmarx_handle.Init.Direction = DMA_PERIPH_TO_MEMORY;
    usart6_dmarx_handle.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    usart6_dmarx_handle.Init.MemInc = DMA_MINC_ENABLE;
    usart6_dmarx_handle.Init.Mode = DMA_CIRCULAR;
    usart6_dmarx_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    usart6_dmarx_handle.Init.PeriphInc = DMA_PINC_DISABLE;
    usart6_dmarx_handle.Init.Priority = USART6_DMA_RX_PRIORITY;

    usart6_rx_fifo.head_ptr = 0;
    usart6_rx_fifo.recv_buf =
        (uint8_t *)malloc(sizeof(uint8_t) * USART6_RX_BUF_SIZE);
#ifdef DEBUG
    assert(usart6_rx_fifo.recv_buf != NULL);
#endif /* DEBUG */

    usart6_rx_fifo.rx_fifo_buf =
        (uint8_t *)malloc(sizeof(uint8_t) * USART6_RX_FIFO_SZIE);
#ifdef DEBUG
    assert(usart6_rx_fifo.rx_fifo_buf != NULL);
#endif /* DEBUG */

    usart6_rx_fifo.rx_fifo =
        ring_fifo_init(usart6_rx_fifo.rx_fifo_buf,
                       sizeof(uint8_t) * USART6_RX_FIFO_SZIE, RF_TYPE_STREAM);
#ifdef DEBUG
    assert(usart6_rx_fifo.rx_fifo != NULL);
#endif /* DEBUG */

    __HAL_RCC_DMA2_CLK_ENABLE();
    res = HAL_DMA_Init(&usart6_dmarx_handle);
#ifdef DEBUG
    assert(res == HAL_OK);
#endif /* DEBUG */

    __HAL_LINKDMA(&usart6_handle, hdmarx, usart6_dmarx_handle);

    HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, USART6_DMA_RX_IT_PREEMPT,
                         USART6_DMA_RX_IT_SUB);
    HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);

#if USART6_USE_IDLE_IT
    __HAL_UART_ENABLE_IT(&usart6_handle, UART_IT_IDLE);
    __HAL_UART_CLEAR_IDLEFLAG(&usart6_handle);
#endif /* USART6_USE_IDLE_IT */
    HAL_UART_Receive_DMA(&usart6_handle, (uint8_t *)usart6_rx_fifo.recv_buf,
                         USART6_RX_BUF_SIZE);

#if (USE_HAL_UART_REGISTER_CALLBACKS == 1)
    HAL_UART_RegisterCallback(&usart6_handle, HAL_UART_RX_HALFCOMPLETE_CB_ID,
                              uart_dmarx_halfdone_callback);
    HAL_UART_RegisterCallback(&usart6_handle, HAL_UART_RX_COMPLETE_CB_ID,
                              uart_dmarx_done_callback);
#endif /* USE_HAL_UART_REGISTER_CALLBACKS == 1 */
}

/**
 * @brief 串口6接收中断句柄
 *
 */
void DMA2_Stream1_IRQHandler(void) {
    HAL_DMA_IRQHandler(&usart6_dmarx_handle);
}
#endif /* USART6_USE_DMA_RX == 1 */

#endif /* USART6_ENABLE == 1 */

#if (UART7_ENABLE == 1)

#if (UART7_USE_DMA_TX == 1)

static DMA_HandleTypeDef uart7_dmatx_handle;
static uart_tx_buf_t uart7_tx_buf;

/**
 * @brief 串口7发送DMA初始化
 *
 */
void uart7_dmatx_init(void) {
    HAL_StatusTypeDef res = HAL_OK;

    uart7_dmatx_handle.Instance = DMA1_Stream1;
    uart7_dmatx_handle.Init.Channel = DMA_CHANNEL_5;
    uart7_dmatx_handle.Init.Direction = DMA_MEMORY_TO_PERIPH;
    uart7_dmatx_handle.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    uart7_dmatx_handle.Init.MemInc = DMA_MINC_ENABLE;
    uart7_dmatx_handle.Init.Mode = DMA_NORMAL;
    uart7_dmatx_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    uart7_dmatx_handle.Init.PeriphInc = DMA_PINC_DISABLE;
    uart7_dmatx_handle.Init.Priority = UART7_DMA_TX_PRIORITY;

    uart7_tx_buf.send_buf =
        (uint8_t *)malloc(sizeof(uint8_t) * UART7_TX_BUF_SIZE);
    uart7_tx_buf.send_buf_size = UART7_TX_BUF_SIZE;
#ifdef DEBUG
    assert(uart7_tx_buf.send_buf != NULL);
#endif /* DEBUG */

    uart7_tx_buf.tc_flag = 1;

    __HAL_RCC_DMA1_CLK_ENABLE();
    res = HAL_DMA_Init(&uart7_dmatx_handle);
#ifdef DEBUG
    assert(res == HAL_OK);
#endif /* DEBUG */

    __HAL_LINKDMA(&uart7_handle, hdmatx, uart7_dmatx_handle);

    HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, UART7_DMA_TX_IT_PREEMPT,
                         UART7_DMA_TX_IT_SUB);
    HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);

#if (USE_HAL_UART_REGISTER_CALLBACKS == 1)
    HAL_UART_RegisterCallback(&uart7_handle, HAL_UART_TX_COMPLETE_CB_ID,
                              uart_dmatx_clear_tc_flag);
#endif /* USE_HAL_UART_REGISTER_CALLBACKS == 1 */
}

/**
 * @brief 串口7发送中断句柄
 *
 */
void DMA1_Stream1_IRQHandler(void) {
    HAL_DMA_IRQHandler(&uart7_dmatx_handle);
}
#endif /* UART7_USE_DMA_TX == 1 */

#if (UART7_USE_DMA_RX == 1)

static DMA_HandleTypeDef uart7_dmarx_handle;
static uart_rx_fifo_t uart7_rx_fifo;

/**
 * @brief 串口7接收DMA初始化
 *
 */
void uart7_dmarx_init(void) {
    HAL_StatusTypeDef res = HAL_OK;

    uart7_dmarx_handle.Instance = DMA1_Stream3;
    uart7_dmarx_handle.Init.Channel = DMA_CHANNEL_5;
    uart7_dmarx_handle.Init.Direction = DMA_PERIPH_TO_MEMORY;
    uart7_dmarx_handle.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    uart7_dmarx_handle.Init.MemInc = DMA_MINC_ENABLE;
    uart7_dmarx_handle.Init.Mode = DMA_CIRCULAR;
    uart7_dmarx_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    uart7_dmarx_handle.Init.PeriphInc = DMA_PINC_DISABLE;
    uart7_dmarx_handle.Init.Priority = UART7_DMA_RX_PRIORITY;

    uart7_rx_fifo.head_ptr = 0;
    uart7_rx_fifo.recv_buf =
        (uint8_t *)malloc(sizeof(uint8_t) * UART7_RX_BUF_SIZE);
#ifdef DEBUG
    assert(uart7_rx_fifo.recv_buf != NULL);
#endif /* DEBUG */

    uart7_rx_fifo.rx_fifo_buf =
        (uint8_t *)malloc(sizeof(uint8_t) * UART7_RX_FIFO_SZIE);
#ifdef DEBUG
    assert(uart7_rx_fifo.rx_fifo_buf != NULL);
#endif /* DEBUG */

    uart7_rx_fifo.rx_fifo =
        ring_fifo_init(uart7_rx_fifo.rx_fifo_buf,
                       sizeof(uint8_t) * UART7_RX_FIFO_SZIE, RF_TYPE_STREAM);
#ifdef DEBUG
    assert(uart7_rx_fifo.rx_fifo != NULL);
#endif /* DEBUG */

    __HAL_RCC_DMA1_CLK_ENABLE();
    res = HAL_DMA_Init(&uart7_dmarx_handle);
#ifdef DEBUG
    assert(res == HAL_OK);
#endif /* DEBUG */

    __HAL_LINKDMA(&uart7_handle, hdmarx, uart7_dmarx_handle);

    HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, UART7_DMA_RX_IT_PREEMPT,
                         UART7_DMA_RX_IT_SUB);
    HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);

#if UART7_USE_IDLE_IT
    __HAL_UART_ENABLE_IT(&uart7_handle, UART_IT_IDLE);
    __HAL_UART_CLEAR_IDLEFLAG(&uart7_handle);
#endif /* UART7_USE_IDLE_IT */
    HAL_UART_Receive_DMA(&uart7_handle, (uint8_t *)uart7_rx_fifo.recv_buf,
                         UART7_RX_BUF_SIZE);

#if (USE_HAL_UART_REGISTER_CALLBACKS == 1)
    HAL_UART_RegisterCallback(&uart7_handle, HAL_UART_RX_HALFCOMPLETE_CB_ID,
                              uart_dmarx_halfdone_callback);
    HAL_UART_RegisterCallback(&uart7_handle, HAL_UART_RX_COMPLETE_CB_ID,
                              uart_dmarx_done_callback);
#endif /* USE_HAL_UART_REGISTER_CALLBACKS == 1 */
}

/**
 * @brief 串口7接收中断句柄
 *
 */
void DMA1_Stream3_IRQHandler(void) {
    HAL_DMA_IRQHandler(&uart7_dmarx_handle);
}
#endif /* UART7_USE_DMA_RX == 1 */

#endif /* UART7_ENABLE == 1 */

#if (UART7_ENABLE == 1)

#if (UART7_USE_DMA_TX == 1)

static DMA_HandleTypeDef uart7_dmatx_handle;
static uart_tx_buf_t uart7_tx_buf;

/**
 * @brief 串口7发送DMA初始化
 *
 */
void uart7_dmatx_init(void) {
    HAL_StatusTypeDef res = HAL_OK;

    uart7_dmatx_handle.Instance = DMA1_Stream1;
    uart7_dmatx_handle.Init.Channel = DMA_CHANNEL_5;
    uart7_dmatx_handle.Init.Direction = DMA_MEMORY_TO_PERIPH;
    uart7_dmatx_handle.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    uart7_dmatx_handle.Init.MemInc = DMA_MINC_ENABLE;
    uart7_dmatx_handle.Init.Mode = DMA_NORMAL;
    uart7_dmatx_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    uart7_dmatx_handle.Init.PeriphInc = DMA_PINC_DISABLE;
    uart7_dmatx_handle.Init.Priority = UART7_DMA_TX_PRIORITY;

    uart7_tx_buf.send_buf =
        (uint8_t *)malloc(sizeof(uint8_t) * UART7_TX_BUF_SIZE);
    uart7_tx_buf.send_buf_size = UART7_TX_BUF_SIZE;
#ifdef DEBUG
    assert(uart7_tx_buf.send_buf != NULL);
#endif /* DEBUG */

    uart7_tx_buf.tc_flag = 1;

    __HAL_RCC_DMA1_CLK_ENABLE();
    res = HAL_DMA_Init(&uart7_dmatx_handle);
#ifdef DEBUG
    assert(res == HAL_OK);
#endif /* DEBUG */

    __HAL_LINKDMA(&uart7_handle, hdmatx, uart7_dmatx_handle);

    HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, UART7_DMA_TX_IT_PREEMPT,
                         UART7_DMA_TX_IT_SUB);
    HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);

#if (USE_HAL_UART_REGISTER_CALLBACKS == 1)
    HAL_UART_RegisterCallback(&uart7_handle, HAL_UART_TX_COMPLETE_CB_ID,
                              uart_dmatx_clear_tc_flag);
#endif /* USE_HAL_UART_REGISTER_CALLBACKS == 1 */
}

/**
 * @brief 串口7发送中断句柄
 *
 */
void DMA1_Stream1_IRQHandler(void) {
    HAL_DMA_IRQHandler(&uart7_dmatx_handle);
}
#endif /* UART7_USE_DMA_TX == 1 */

#if (UART7_USE_DMA_RX == 1)

static DMA_HandleTypeDef uart7_dmarx_handle;
static uart_rx_fifo_t uart7_rx_fifo;

/**
 * @brief 串口7接收DMA初始化
 *
 */
void uart7_dmarx_init(void) {
    HAL_StatusTypeDef res = HAL_OK;

    uart7_dmarx_handle.Instance = DMA1_Stream3;
    uart7_dmarx_handle.Init.Channel = DMA_CHANNEL_5;
    uart7_dmarx_handle.Init.Direction = DMA_PERIPH_TO_MEMORY;
    uart7_dmarx_handle.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    uart7_dmarx_handle.Init.MemInc = DMA_MINC_ENABLE;
    uart7_dmarx_handle.Init.Mode = DMA_CIRCULAR;
    uart7_dmarx_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    uart7_dmarx_handle.Init.PeriphInc = DMA_PINC_DISABLE;
    uart7_dmarx_handle.Init.Priority = UART7_DMA_RX_PRIORITY;

    uart7_rx_fifo.head_ptr = 0;
    uart7_rx_fifo.recv_buf =
        (uint8_t *)malloc(sizeof(uint8_t) * UART7_RX_BUF_SIZE);
#ifdef DEBUG
    assert(uart7_rx_fifo.recv_buf != NULL);
#endif /* DEBUG */

    uart7_rx_fifo.rx_fifo_buf =
        (uint8_t *)malloc(sizeof(uint8_t) * UART7_RX_FIFO_SZIE);
#ifdef DEBUG
    assert(uart7_rx_fifo.rx_fifo_buf != NULL);
#endif /* DEBUG */

    uart7_rx_fifo.rx_fifo =
        ring_fifo_init(uart7_rx_fifo.rx_fifo_buf,
                       sizeof(uint8_t) * UART7_RX_FIFO_SZIE, RF_TYPE_STREAM);
#ifdef DEBUG
    assert(uart7_rx_fifo.rx_fifo != NULL);
#endif /* DEBUG */

    __HAL_RCC_DMA1_CLK_ENABLE();
    res = HAL_DMA_Init(&uart7_dmarx_handle);
#ifdef DEBUG
    assert(res == HAL_OK);
#endif /* DEBUG */

    __HAL_LINKDMA(&uart7_handle, hdmarx, uart7_dmarx_handle);

    HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, UART7_DMA_RX_IT_PREEMPT,
                         UART7_DMA_RX_IT_SUB);
    HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);

#if UART7_USE_IDLE_IT
    __HAL_UART_ENABLE_IT(&uart7_handle, UART_IT_IDLE);
    __HAL_UART_CLEAR_IDLEFLAG(&uart7_handle);
#endif /* UART7_USE_IDLE_IT */
    HAL_UART_Receive_DMA(&uart7_handle, (uint8_t *)uart7_rx_fifo.recv_buf,
                         UART7_RX_BUF_SIZE);

#if (USE_HAL_UART_REGISTER_CALLBACKS == 1)
    HAL_UART_RegisterCallback(&uart7_handle, HAL_UART_RX_HALFCOMPLETE_CB_ID,
                              uart_dmarx_halfdone_callback);
    HAL_UART_RegisterCallback(&uart7_handle, HAL_UART_RX_COMPLETE_CB_ID,
                              uart_dmarx_done_callback);
#endif /* USE_HAL_UART_REGISTER_CALLBACKS == 1 */
}

/**
 * @brief 串口7接收中断句柄
 *
 */
void DMA1_Stream3_IRQHandler(void) {
    HAL_DMA_IRQHandler(&uart7_dmarx_handle);
}
#endif /* UART7_USE_DMA_RX == 1 */

#endif /* UART7_ENABLE == 1 */

#if (UART8_ENABLE == 1)

#if (UART8_USE_DMA_TX == 1)

static DMA_HandleTypeDef uart8_dmatx_handle;
static uart_tx_buf_t uart8_tx_buf;

/**
 * @brief 串口8发送DMA初始化
 *
 */
void uart8_dmatx_init(void) {
    HAL_StatusTypeDef res = HAL_OK;

    uart8_dmatx_handle.Instance = DMA1_Stream0;
    uart8_dmatx_handle.Init.Channel = DMA_CHANNEL_5;
    uart8_dmatx_handle.Init.Direction = DMA_MEMORY_TO_PERIPH;
    uart8_dmatx_handle.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    uart8_dmatx_handle.Init.MemInc = DMA_MINC_ENABLE;
    uart8_dmatx_handle.Init.Mode = DMA_NORMAL;
    uart8_dmatx_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    uart8_dmatx_handle.Init.PeriphInc = DMA_PINC_DISABLE;
    uart8_dmatx_handle.Init.Priority = UART8_DMA_TX_PRIORITY;

    uart8_tx_buf.send_buf =
        (uint8_t *)malloc(sizeof(uint8_t) * UART8_TX_BUF_SIZE);
    uart8_tx_buf.send_buf_size = UART8_TX_BUF_SIZE;
#ifdef DEBUG
    assert(uart8_tx_buf.send_buf != NULL);
#endif /* DEBUG */

    uart8_tx_buf.tc_flag = 1;

    __HAL_RCC_DMA1_CLK_ENABLE();
    res = HAL_DMA_Init(&uart8_dmatx_handle);
#ifdef DEBUG
    assert(res == HAL_OK);
#endif /* DEBUG */

    __HAL_LINKDMA(&uart8_handle, hdmatx, uart8_dmatx_handle);

    HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, UART8_DMA_TX_IT_PREEMPT,
                         UART8_DMA_TX_IT_SUB);
    HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);

#if (USE_HAL_UART_REGISTER_CALLBACKS == 1)
    HAL_UART_RegisterCallback(&uart8_handle, HAL_UART_TX_COMPLETE_CB_ID,
                              uart_dmatx_clear_tc_flag);
#endif /* USE_HAL_UART_REGISTER_CALLBACKS == 1 */
}

/**
 * @brief 串口8发送中断句柄
 *
 */
void DMA1_Stream0_IRQHandler(void) {
    HAL_DMA_IRQHandler(&uart8_dmatx_handle);
}
#endif /* UART8_USE_DMA_TX == 1 */

#if (UART8_USE_DMA_RX == 1)

static DMA_HandleTypeDef uart8_dmarx_handle;
static uart_rx_fifo_t uart8_rx_fifo;

/**
 * @brief 串口8接收DMA初始化
 *
 */
void uart8_dmarx_init(void) {
    HAL_StatusTypeDef res = HAL_OK;

    uart8_dmarx_handle.Instance = DMA1_Stream6;
    uart8_dmarx_handle.Init.Channel = DMA_CHANNEL_5;
    uart8_dmarx_handle.Init.Direction = DMA_PERIPH_TO_MEMORY;
    uart8_dmarx_handle.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    uart8_dmarx_handle.Init.MemInc = DMA_MINC_ENABLE;
    uart8_dmarx_handle.Init.Mode = DMA_CIRCULAR;
    uart8_dmarx_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    uart8_dmarx_handle.Init.PeriphInc = DMA_PINC_DISABLE;
    uart8_dmarx_handle.Init.Priority = UART8_DMA_RX_PRIORITY;

    uart8_rx_fifo.head_ptr = 0;
    uart8_rx_fifo.recv_buf =
        (uint8_t *)malloc(sizeof(uint8_t) * UART8_RX_BUF_SIZE);
#ifdef DEBUG
    assert(uart8_rx_fifo.recv_buf != NULL);
#endif /* DEBUG */

    uart8_rx_fifo.rx_fifo_buf =
        (uint8_t *)malloc(sizeof(uint8_t) * UART8_RX_FIFO_SZIE);
#ifdef DEBUG
    assert(uart8_rx_fifo.rx_fifo_buf != NULL);
#endif /* DEBUG */

    uart8_rx_fifo.rx_fifo =
        ring_fifo_init(uart8_rx_fifo.rx_fifo_buf,
                       sizeof(uint8_t) * UART8_RX_FIFO_SZIE, RF_TYPE_STREAM);
#ifdef DEBUG
    assert(uart8_rx_fifo.rx_fifo != NULL);
#endif /* DEBUG */

    __HAL_RCC_DMA1_CLK_ENABLE();
    res = HAL_DMA_Init(&uart8_dmarx_handle);
#ifdef DEBUG
    assert(res == HAL_OK);
#endif /* DEBUG */

    __HAL_LINKDMA(&uart8_handle, hdmarx, uart8_dmarx_handle);

    HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, UART8_DMA_RX_IT_PREEMPT,
                         UART8_DMA_RX_IT_SUB);
    HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);

#if UART8_USE_IDLE_IT
    __HAL_UART_ENABLE_IT(&uart8_handle, UART_IT_IDLE);
    __HAL_UART_CLEAR_IDLEFLAG(&uart8_handle);
#endif /* UART8_USE_IDLE_IT */
    HAL_UART_Receive_DMA(&uart8_handle, (uint8_t *)uart8_rx_fifo.recv_buf,
                         UART8_RX_BUF_SIZE);

#if (USE_HAL_UART_REGISTER_CALLBACKS == 1)
    HAL_UART_RegisterCallback(&uart8_handle, HAL_UART_RX_HALFCOMPLETE_CB_ID,
                              uart_dmarx_halfdone_callback);
    HAL_UART_RegisterCallback(&uart8_handle, HAL_UART_RX_COMPLETE_CB_ID,
                              uart_dmarx_done_callback);
#endif /* USE_HAL_UART_REGISTER_CALLBACKS == 1 */
}

/**
 * @brief 串口8接收中断句柄
 *
 */
void DMA1_Stream6_IRQHandler(void) {
    HAL_DMA_IRQHandler(&uart8_dmarx_handle);
}
#endif /* UART8_USE_DMA_RX == 1 */

#endif /* UART8_ENABLE == 1 */

/*****************************************************************************
 * @defgroup 发送函数部分
 * @{
 */

/**
 * @brief 根据串口句柄, 判断是哪个发送缓冲区指针
 *
 * @param huart 串口句柄
 * @return 发送缓冲区指针
 */
static inline uart_tx_buf_t *uart_tx_identify(UART_HandleTypeDef *huart) {
    switch ((uintptr_t)(huart->Instance)) {

#if USART1_USE_DMA_TX
        case USART1_BASE: {
            return &usart1_tx_buf;
        }
#endif /* USART1_USE_DMA_TX */

#if USART2_USE_DMA_TX
        case USART2_BASE: {
            return &usart2_tx_buf;
        }
#endif /* USART2_USE_DMA_TX */

#if USART3_USE_DMA_TX
        case USART3_BASE: {
            return &usart3_tx_buf;
        }
#endif /* USART3_USE_DMA_TX */

#if UART4_USE_DMA_TX
        case UART4_BASE: {
            return &uart4_tx_buf;
        }
#endif /* UART4_USE_DMA_TX */

#if UART5_USE_DMA_TX
        case UART5_BASE: {
            return &uart5_tx_buf;
        }
#endif /* UART5_USE_DMA_TX */

#if USART6_USE_DMA_TX
        case USART6_BASE: {
            return &usart6_tx_buf;
        }
#endif /* USART6_USE_DMA_TX */

#if UART7_USE_DMA_TX
        case UART7_BASE: {
            return &uart7_tx_buf;
        }
#endif /* UART7_USE_DMA_TX */

#if UART8_USE_DMA_TX
        case UART8_BASE: {
            return &uart8_tx_buf;
        }
#endif /* UART8_USE_DMA_TX */

        default: {
        } break;
    }

    return NULL;
}

/**
 * @brief 清楚串口发送完成标志位
 *
 * @param huart 串口句柄
 */
void uart_dmatx_clear_tc_flag(UART_HandleTypeDef *huart) {
    uart_tx_buf_t *uart_tx_buf = uart_tx_identify(huart);
    if (uart_tx_buf == NULL) {
        return;
    }

    uart_tx_buf->tc_flag = 1;
}

/**
 * @brief 向串口发送缓冲区中写入数据
 *
 * @param huart 串口句柄
 * @param data 数据
 * @param len 数据长度
 * @return 成功写入的长度
 */
uint32_t uart_dmatx_write(UART_HandleTypeDef *huart, const void *data,
                          size_t len) {
    if ((data == NULL) || (len == 0)) {
        return 0;
    }

    uart_tx_buf_t *send_tx_buf = uart_tx_identify(huart);
    if (send_tx_buf == NULL) {
        return 0;
    }

    /* 计算当前缓冲区剩余长度 */
    uint32_t buf_remain = send_tx_buf->send_buf_size - send_tx_buf->head_ptr;

    /* 如果要写入的长度大于剩余长度, 为避免溢出, 只写入剩余的长度 */
    if (buf_remain < len) {
        memcpy(send_tx_buf->send_buf + send_tx_buf->head_ptr, data, buf_remain);
        send_tx_buf->head_ptr += buf_remain;
        return buf_remain;
    } else {
        memcpy(send_tx_buf->send_buf + send_tx_buf->head_ptr, data, len);
        send_tx_buf->head_ptr += len;
        return len;
    }
}

/**
 * @brief 把FIFO中的数据通过DMA发送
 *
 * @param huart 串口句柄
 * @return 成功发送的长度
 * @note 先使用`uart_dmatx_write`写入数据
 */
uint32_t uart_dmatx_send(UART_HandleTypeDef *huart) {
    uart_tx_buf_t *send_tx_buf = uart_tx_identify(huart);
    if (send_tx_buf == NULL) {
        return 0;
    }

    /* 未启用DMA */
    if (huart->hdmatx == NULL) {
        return 0;
    }

    /* 未发送完毕 */
    if (!send_tx_buf->tc_flag) {
        return 0;
    }

    uint32_t len = send_tx_buf->head_ptr;
    /* 缓冲区为空 */
    if (!len) {
        return 0;
    }

    send_tx_buf->tc_flag = 0;
    HAL_UART_Transmit_DMA(huart, send_tx_buf->send_buf, (uint16_t)len);
    send_tx_buf->head_ptr = 0;
    return len;
}

/**
 * @}
 */

/*****************************************************************************
 * @defgroup 接收函数部分
 * @{
 */

/**
 * @brief 根据串口句柄, 判断是哪个接收缓冲区指针
 *
 * @param huart 串口句柄
 * @return 接收缓冲区指针
 */
static inline uart_rx_fifo_t *uart_rx_identify(UART_HandleTypeDef *huart) {

    switch ((uintptr_t)huart->Instance) {
#if USART1_USE_DMA_RX
        case USART1_BASE: {
            return &usart1_rx_fifo;
        }
#endif /* USART1_USE_DMA_RX */

#if USART2_USE_DMA_RX
        case USART2_BASE: {
            return &usart2_rx_fifo;
        }
#endif /* USART2_USE_DMA_RX */

#if USART3_USE_DMA_RX
        case USART3_BASE: {
            return &usart3_rx_fifo;
        }
#endif /* USART3_USE_DMA_RX */

#if UART4_USE_DMA_RX
        case UART4_BASE: {
            return &uart4_rx_fifo;
        }
#endif /* UART4_USE_DMA_RX */

#if UART5_USE_DMA_RX
        case UART5_BASE: {
            return &uart5_rx_fifo;
        }
#endif /* UART5_USE_DMA_RX */

#if USART6_USE_DMA_RX
        case USART6_BASE: {
            return &usart6_rx_fifo;
        }
#endif /* USART6_USE_DMA_RX */

#if UART7_USE_DMA_RX
        case UART7_BASE: {
            return &uart7_rx_fifo;
        }
#endif /* UART7_USE_DMA_RX */

#if UART8_USE_DMA_RX
        case UART8_BASE: {
            return &uart8_rx_fifo;
        }
#endif /* UART8_USE_DMA_RX */

        default: {
        } break;
    }

    return NULL;
}

/**
 * @brief DMA接收空闲回调
 *
 * @param huart 串口句柄
 */
void uart_dmarx_idle_callback(UART_HandleTypeDef *huart) {
    uart_rx_fifo_t *uart_rx_fifo = uart_rx_identify(huart);
    if (uart_rx_fifo == NULL) {
        return;
    }

    uint32_t tail_ptr;
    uint32_t copy, offset;

    /**
     * +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
     * |     head_ptr          tail_ptr         |
     * |         |                 |            |
     * |         v                 v            |
     * | --------*******************----------- |
     * +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
     */

    /* 已接收 */
    tail_ptr = huart->RxXferSize - __HAL_DMA_GET_COUNTER(huart->hdmarx);

    offset = (uart_rx_fifo->head_ptr) % (uint32_t)(huart->RxXferSize);
    copy = tail_ptr - offset;
    uart_rx_fifo->head_ptr += copy;

    ring_fifo_write(uart_rx_fifo->rx_fifo, huart->pRxBuffPtr + offset, copy);
}

/**
 * @brief 串口DMA接收半满回调
 *
 * @param huart 串口句柄
 */
void uart_dmarx_halfdone_callback(UART_HandleTypeDef *huart) {
    uart_rx_fifo_t *uart_rx_fifo = uart_rx_identify(huart);
    if (uart_rx_fifo == NULL) {
        return;
    }

    uint32_t tail_ptr;
    uint32_t offset, copy;

    /**
     * +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
     * |                  half                  |
     * |     head_ptr   tail_ptr                |
     * |         |          |                   |
     * |         v          v                   |
     * | --------*******************----------- |
     * +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
     */

    tail_ptr = (huart->RxXferSize >> 1) + (huart->RxXferSize & 1);

    offset = (uart_rx_fifo->head_ptr) % (uint32_t)(huart->RxXferSize);
    copy = tail_ptr - offset;
    uart_rx_fifo->head_ptr += copy;

    ring_fifo_write(uart_rx_fifo->rx_fifo, huart->pRxBuffPtr + offset, copy);
}

/**
 * @brief 串口DMA溢满中断回调
 *
 * @param huart 串口句柄
 */
void uart_dmarx_done_callback(UART_HandleTypeDef *huart) {
    uart_rx_fifo_t *uart_rx_fifo = uart_rx_identify(huart);
    if (uart_rx_fifo == NULL) {
        return;
    }

    uint32_t tail_ptr;
    uint32_t offset, copy;

    /**
     * +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
     * |                  half                  |
     * |                    | head_ptr tail_ptr |
     * |                    |    |            | |
     * |                    v    v            v |
     * | ------------------------************** |
     * +~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
     */

    tail_ptr = huart->RxXferSize;

    offset = (uart_rx_fifo->head_ptr) % (uint32_t)(huart->RxXferSize);
    copy = tail_ptr - offset;
    uart_rx_fifo->head_ptr += copy;

    ring_fifo_write(uart_rx_fifo->rx_fifo, huart->pRxBuffPtr + offset, copy);

    if (huart->hdmarx->Init.Mode != DMA_CIRCULAR) {
        /* 非循环DMA, 重新打开DMA接收 */
        while (HAL_UART_Receive_DMA(huart, huart->pRxBuffPtr,
                                    huart->RxXferSize) != HAL_OK) {
            __HAL_UNLOCK(huart);
        }
    }
}

/**
 * @brief 从接收FIFO读数据
 *
 * @param huart 串口句柄
 * @param buf 接收缓冲数组
 * @param len `buf`长度
 * @return 接收到的长度
 */
uint32_t uart_dmarx_read(UART_HandleTypeDef *huart, void *buf, size_t len) {
    if ((buf == NULL) || (len == 0)) {
        return 0;
    }
    uart_rx_fifo_t *uart_rx_fifo = uart_rx_identify(huart);

    if (uart_rx_fifo == NULL) {
        return 0;
    }

    return ring_fifo_read(uart_rx_fifo->rx_fifo, buf, len);
}

/**
 * @}
 */
