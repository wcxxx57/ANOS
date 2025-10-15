#include "arch/mod.h"
#include "lib/mod.h"
#include "mem/mod.h"
#include "trap/mod.h"

volatile static int started = 0;

int main()
{
    // 初始化 S-mode 的中断/异常处理
    trap_kernel_init();
    trap_kernel_inithart();

    // 创建系统时钟
    timer_create();

    // 打开 S-mode 中断
    intr_on();

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
   
    // 滴答测试
    uint64 last = (uint64)-1;
    while (1) {
        uint64 t = timer_get_ticks();
        if (t != last) {
            last = t;
            printf("cpu %d:di da\n", cpuid);
        }
    }
}
