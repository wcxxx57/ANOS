#include "arch/mod.h"
#include "lib/mod.h"
#include "mem/mod.h"
#include "trap/mod.h"
#include "proc/mod.h"

volatile static int started = 0;

int main()
{
    int cpuid = r_tp();

    if(cpuid == 0) {
        // CPU0
        print_init();
        printf("cpu %d is booting!\n", cpuid);
        pmem_init();
        kvm_init();
        kvm_inithart();
        trap_kernel_init();
        trap_kernel_inithart();
        
        // 初始化 mmap 仓库
        mmap_init();
        
        __sync_synchronize();
        started = 1;

        // 创建第一个用户进程（initcode）
        proc_make_first(); 

        // proc_make_first 内部会 swtch，理论上不会返回这里
        // 如果返回了，说明调度器设计不同，或者出错了
        panic("main: proc_make_first returned");

    } else {
        // CPU1
        while(started == 0);
        __sync_synchronize();
        printf("cpu %d is booting!\n", cpuid);
        kvm_inithart();
        trap_kernel_inithart();
    }

    while (1);
}