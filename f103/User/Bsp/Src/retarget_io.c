/**
 * @file    retarget_io.c
 * @author  Deadline039
 * @brief   重定向C库的stdin, stdout, stderr到指定设备
 * @version 1.0
 * @date    2024-07-29
 * @ref     正点原子, 野火, CMSIS
 * @ref
 * https://developer.arm.com/documentation/100073/0611/the-c-and-c---library-functions-reference
 */

#include "retarget_io.h"
#include <stdio.h>

#if ((RETARGET_STDOUT == 1) || (RETARGET_STDERR == 1))
#if defined(__ARMCC_VERSION) /* Compiler */

#if (__ARMCC_VERSION >= 6010050) /* 使用AC6编译器时 */

__asm(".global __use_no_semihosting\n\t"); /* 声明不使用半主机模式 */
/* AC6下需要声明main函数为无参数格式，否则部分例程可能出现半主机模式 */
__asm(".global __ARM_use_no_argv \n\t");

#elif ((__ARMCC_VERSION >= 5000000) &&                                         \
       (__ARMCC_VERSION < 6000000)) /* 使用AC5编译器 */
/* 使用AC5编译器时, 要在这里定义__FILE 和 不使用半主机模式 */

/* 关闭多字节警告 */
#pragma diag_suppress 870

#pragma import(__use_no_semihosting)

struct __FILE {
    int handle;
    /* Whatever you require here. If the only file you are using is */
    /* standard output using printf() for debugging, no file handling */
    /* is required. */
};
#endif /* __ARMCC_VERSION */
/**
 * 不使用半主机模式，至少需要重定义_ttywrch, _sys_exit,
 * _sys_command_string函数,以同时兼容AC6和AC5模式
 */

/**
 * @brief 将字符写入控制台, 控制台输出时可从此重定向
 *
 * @param ch 输出的字符
 * @return 字符
 * @note 此函数默认在半主机模式下实现
 */
int _ttywrch(int ch) {
    ch = ch;
    return ch;
}

/**
 * @brief 调用了C库中类似exit的函数时会调用此函数
 *
 * @param x 退出的参数
 * @note 如果使用MicroLib, C库中的所有退出函数都会被移除
 */
void _sys_exit(int x) {
    x = x;
}

/**
 * @brief 此函数负责将argc, argv参数传递main函数. 编译器自动链接
 *
 * @param cmd main参数
 * @param len 参数字符串长度
 * @return main函数参数的字符串
 */
char *_sys_command_string(char *cmd, int len) {
    (void)cmd;
    (void)len;
    return NULL;
}

/**
 * @brief 向文件写入字符
 *
 * @param ch 写入的字符
 * @param f 文件指针
 * @return 写入的字符
 */
int fputc(int ch, FILE *f) {

#if ((RETARGET_STDOUT == 1) && (RETARGET_STDERR == 1))
    if (f == stdout) {
#endif /* ((RETARGET_STDOUT == 1) && (RETARGET_STDERR == 1)) */

#if (RETARGET_STDOUT == 1)
#if (RE_STDOUT_TARGET == 0) /* 使用串口 */
        while ((STDOUT_UART->SR & 0X40) == 0)
            ; /* 等待上一个字符发送完成 */

        STDOUT_UART->DR = (uint8_t)ch; /* 将要发送的字符 ch 写入到DR寄存器 */
#elif (RE_STDOUT_TARGET == 1) /* 使用ITM */

        ITM_SendChar(ch);

#endif /* RE_STDOUT_TARGET */

#endif /* RETARGET_STDOUT == 1 */

#if ((RETARGET_STDOUT == 1) && (RETARGET_STDERR == 1))
    } else if (f == stderr) {
#endif /* ((RETARGET_STDOUT == 1) && (RETARGET_STDERR == 1)) */

#if (RETARGET_STDERR == 1)
#if (RE_STDERR_TARGET == 0) /* 使用串口 */
        while ((STDERR_UART->SR & 0X40) == 0)
            ; /* 等待上一个字符发送完成 */

        STDERR_UART->DR = (uint8_t)ch; /* 将要发送的字符 ch 写入到DR寄存器 */
#elif (RE_STDERR_TARGET == 1) /* 使用ITM */

        ITM_SendChar(ch);

#endif /* RE_STDERR_TARGET */

#endif /* RETARGET_STDERR == 1 */

#if ((RETARGET_STDOUT == 1) && (RETARGET_STDERR == 1))
    }
#endif /* ((RETARGET_STDOUT == 1) && (RETARGET_STDERR == 1)) */

    return ch;
}

#elif (defined(__GNUC__)) /* 使用ARM GCC编译器 */

#pragma import(__use_no_semihosting) /* 不使用半主机模式 */

/**
 * @brief gcc文件写入函数
 *
 * @param f 文件指针
 * @param ptr 字符串
 * @param len 字符串长度
 * @return 字符串长度
 */
int _write(int f, char *ptr, int len) {

#if ((RETARGET_STDOUT == 1) && (RETARGET_STDERR == 1))
    if (f == stdout) {
#endif /* ((RETARGET_STDOUT == 1) && (RETARGET_STDERR == 1)) */

#if (RETARGET_STDOUT == 1)
#if (RE_STDOUT_TARGET == 0) /* 使用串口 */

        for (int i = 0; i < len; i++) {
            while ((STDOUT_UART->SR & 0X40) == 0)
                ; /* 等待上一个字符发送完成 */

            STDOUT_UART->DR =
                (uint8_t)ptr[i]; /* 将要发送的字符 ch 写入到DR寄存器 */
        }

#elif (RE_STDOUT_TARGET == 1) /* 使用ITM */

        for (int i = 0; i < len; i++) {
            ITM_SendChar(ptr[i]);
        }

#endif /* RE_STDOUT_TARGET */

#endif /* RETARGET_STDOUT == 1 */

#if ((RETARGET_STDOUT == 1) && (RETARGET_STDERR == 1))
    } else if (f == stderr) {
#endif /* ((RETARGET_STDOUT == 1) && (RETARGET_STDERR == 1)) */

#if (RETARGET_STDERR == 1)
#if (RE_STDERR_TARGET == 0) /* 使用串口 */

        for (int i = 0; i < len; i++) {
            while ((STDERR_UART->SR & 0X40) == 0)
                ; /* 等待上一个字符发送完成 */

            STDERR_UART->DR =
                (uint8_t)ptr[i]; /* 将要发送的字符 ch 写入到DR寄存器 */
        }

#elif (RE_STDERR_TARGET == 1) /* 使用ITM */

        for (int i = 0; i < len; i++) {
            ITM_SendChar(ptr[i]);
        }

#endif /* RE_STDERR_TARGET */

#endif /* RETARGET_STDERR == 1 */

#if ((RETARGET_STDOUT == 1) && (RETARGET_STDERR == 1))
    }
#endif /* ((RETARGET_STDOUT == 1) && (RETARGET_STDERR == 1)) */

    return len;
}

/**
 * @brief 调用了C库中类似exit的函数时会调用此函数
 *
 * @param x 退出的参数
 */
void _sys_exit(int x) {
    x = x;
}

#endif /* Compiler */

#endif /* ((RETARGET_STDOUT == 1) || (RETARGET_STDERR == 1)) */
