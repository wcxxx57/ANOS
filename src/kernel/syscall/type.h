#pragma once
#include "../arch/type.h"

/* 系统调用号 */

#define SYS_copyin 1    // 用户->内核数据复制
#define SYS_copyout 2   // 内核->用户数据复制
#define SYS_copyinstr 3 // 用户->内核字符串复制
#define SYS_brk 4       // 调整堆边界
#define SYS_mmap 5      // 创建内存映射
#define SYS_munmap 6    // 解除内存映射
#define SYS_test_pgtbl 7 // 页表的复制与销毁
#define SYS_print_str 8     // 打印字符串
#define SYS_print_int 9     // 打印32位整数
#define SYS_getpid 10        // 获取当前进程的ID
#define SYS_fork 11          // 进程复制
#define SYS_wait 12          // 等待子进程退出
#define SYS_exit 13          // 进程退出
#define SYS_sleep 14        // 进程睡眠一段时间

#define SYS_MAX_NUM 14

/* 可以传入的最大字符串长度 */
#define STR_MAXLEN 127