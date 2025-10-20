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
    
    //---[DEBUG]查看相关硬件寄存器配置是否正确---
    // printf("=== DEBUG START ===\n");
    // printf("STVEC: %p\n", r_stvec());
    // printf("SSTATUS: %p\n", r_sstatus() & SSTATUS_SIE ? "SIE=ON" : "SIE=OFF");
    
    // uint32 ier = ReadReg(IER);
    // printf("UART IER = 0x%x\n", ier);

    // uint32 plic_en = *(volatile uint32*)PLIC_SENABLE(0);
    // printf("PLIC enable = 0x%x\n", plic_en);
    
    // extern char kernel_vector[];
    // printf("kernel_vector addr: %p\n", kernel_vector);


    // 串口输入测试
    int cpuid = mycpuid();
    if (cpuid == 0) {
        printf("CPU %d is booting!\n", cpuid);
        __sync_synchronize(); // 防止指令重排
        started = 1;

        while (1) {
            asm volatile("wfi");  // 等待中断
        }
    } else {
        while (started == 0);
        __sync_synchronize();
        printf("CPU %d is booting!\n", cpuid);

        while (1) {
            asm volatile("wfi");  // 等待中断
        }
    }
   

    
    // int cpuid = mycpuid();
    // if (cpuid == 0) {
    //     print_init();
    //     printf("CPU %d is booting!\n", cpuid);
    //     __sync_synchronize();
    //     started = 1;
    // } else {
    //     while (started == 0);
    //     __sync_synchronize();
    //     printf("CPU %d is booting!\n", cpuid);
    // }

    // //  滴答测试
    // uint64 last = (uint64)-1;
    // while (1) {
    //     uint64 t = timer_get_ticks();
    //     if (t != last) {
    //         last = t;
    //         printf("cpu %d:di da\n", cpuid);
    //     }
    // }
}
