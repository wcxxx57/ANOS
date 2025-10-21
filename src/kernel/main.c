#include "arch/mod.h"
#include "lib/mod.h"
#include "mem/mod.h"
#include "trap/mod.h"

volatile static int started = 0;

int main()
{    
    // 初始化UART串口
    uart_init();
    
    // 初始化 S-mode 的中断/异常处理
    trap_kernel_init();
    trap_kernel_inithart();
    
    ////---[DEBUG]查看相关硬件寄存器配置是否正确---
    // printf("=== DEBUG START ===\n");
    // printf("STVEC: %p\n", r_stvec());
    // printf("SSTATUS: %p\n", r_sstatus() & SSTATUS_SIE ? "SIE=ON" : "SIE=OFF");
    
    // uint32 ier = ReadReg(IER);
    // printf("UART IER = 0x%x\n", ier);

    // uint32 plic_en = *(volatile uint32*)PLIC_SENABLE(0);
    // printf("PLIC enable = 0x%x\n", plic_en);
    
    // extern char kernel_vector[];
    // printf("kernel_vector addr: %p\n", kernel_vector);


    //---[test]串口输入测试---
    // int cpuid = mycpuid();
    // if (cpuid == 0) {
    //     printf("CPU %d is booting!\n", cpuid);
    //     __sync_synchronize(); // 防止指令重排
    //     started = 1;

    //     while (1) {
    //         asm volatile("wfi");  // 等待中断
    //     }
    // } else {
    //     while (started == 0);
    //     __sync_synchronize();
    //     printf("CPU %d is booting!\n", cpuid);

    //     while (1) {
    //         asm volatile("wfi");  // 等待中断
    //     }
    // }
   
    // ---[test]时钟中断测试---
    int cpuid = mycpuid();
    if (cpuid == 0) {
        print_init();
        printf("CPU %d is booting!\n", cpuid);
        __sync_synchronize();
        started = 1;
    } else {
        while (started == 0);
        __sync_synchronize();
        printf("CPU %d is booting!\n", cpuid);
    }

    //// ---[test]时钟滴答测试---
    // uint64 last = (uint64)-1;
    // while (1) {
    //     uint64 t = timer_get_ticks();
    //     if (t != last) {
    //         last = t;
    //         printf("cpu %d:di da\n", cpuid);
    //     }
    // }

    // ---[test]时钟快慢测试---
    uint64 last = (uint64)-1;
    while (1) {
        if (cpuid == 0) {                 // 只在 CPU0 打印，避免多核重复输出
            uint64 t = timer_get_ticks();
            if (t != last) {
                last = t;
                printf("ticks = %d\n", (int)t);
            }
        }
        asm volatile("wfi");              // 等待中断，降低忙等
    }

    // ---[test]补充测试---
    // UART与Timer共存性测试
    // int cpuid = mycpuid();
    // if (cpuid == 0) {
    //     printf("CPU %d is booting!\n", cpuid);
    //     __sync_synchronize();
    //     started = 1;
    // } else {
    //     while (started == 0) { }
    //     __sync_synchronize();
    //     printf("CPU %d is booting!\n", cpuid);
    // }

    // uint64 last = 0, last_print = 0;
    // const uint64 print_every = 50;   // 每 50 个 tick 打一次, 避免刷屏干扰 UART 回显

    // while (1) {
    //     if (cpuid == 0) {             // 仅 CPU0 打印心跳
    //         uint64 t = timer_get_ticks();
    //         if (t != last) {          // 只在 tick 变化时处理
    //             last = t;
    //             if (t - last_print >= print_every) {
    //                 last_print = t;
    //                 printf("ticks=%d\n", (int)t);
    //             }
    //         }
    //     }
    //     asm volatile("wfi");          // 让位给中断 (Timer / UART)
    // }

}
