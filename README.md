# STM32串口DMA回环测试

串口DMA demo程序，流程如下：

```
PC TX    ---> 串口1 RX
串口1 TX  ---> 串口2 RX
...
串口x TX  ---> 串口y RX
串口y TX  ---> PC RX
```

在1Mbps波特率下发送200多KB文件，没有丢失。

<img src="https://github.com/Deadline039/stm32-dma-uart-demo/blob/main/assets/PixPin_2024-08-02_19-29-00.png" width="75%">
<img src="https://github.com/Deadline039/stm32-dma-uart-demo/blob/main/assets/PixPin_2024-08-02_19-30-13.png" width="100%">

在`User/Bsp/Inc/uart.h`中可以配置是否启用串口，是否使用DMA，DMA缓冲区大小，中断优先级等。

该文件支持使用CMSIS Configuration Wizard配置。如果在Keil中配置，请将文件编码更改为GB2312。

<img src="https://github.com/Deadline039/stm32-dma-uart-demo/blob/main/assets/PixPin_2024-08-02_19-21-04.png" width="75%">

## 测试平台

正点原子MiniSTM32, STM32F103RCT6, 256KB ROM, 64KB RAM. 共3个USART，2个UART。其中UART5无DMA。

正点原子Apollo F429, STM32F429IGT6, 1 MB ROM, 256KB RAM. 共4个USART，4个UART。UART7, UART8的DMA会与其他串口冲突，故这里不使用。

## 编译环境

ARM Compiler 6.21, C11/C++11, LTO, O0优化

## 参照

https://github.com/Prry/stm32-uart-dma

https://gitee.com/wei513723/stm32-stable-uart-transmit-receive
