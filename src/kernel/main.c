#include "arch/mod.h"
#include "lib/mod.h"
#include "mem/mod.h"
#include "trap/mod.h"
#include "proc/mod.h"

// // 测试三：mmap_region_node 仓库管理
// volatile static int started = 0;
// volatile static bool over_1 = false, over_2 = false;
// volatile static bool over_3 = false, over_4 = false;

// void* mmap_list[N_MMAP];

// int main()
// {
//     int cpuid = r_tp();

//     if(cpuid == 0) {
//         // CPU0
//         print_init();
//         printf("cpu %d is booting!\n", cpuid);
//         pmem_init();
//         kvm_init();
//         kvm_inithart();
//         trap_kernel_init();
//         trap_kernel_inithart();
        
//         // 初始化 + 初始状态显示
//         mmap_init();
//         mmap_show_nodelist();
//         printf("\n");

//         __sync_synchronize();
//         started = 1;

//         // 申请
//         for(int i = 0; i < N_MMAP / 2; i++)
//             mmap_list[i] = mmap_region_alloc();
//         over_1 = true;

//         // 屏障
//         while(over_1 == false ||  over_2 == false);

//         // 释放
//         for(int i = 0; i < N_MMAP / 2; i++)
//             mmap_region_free(mmap_list[i]);
//         over_3 = true;

//         // 屏障
//         while (over_3 == false || over_4 == false);

//         // 查看结束时的状态
//         mmap_show_nodelist();        

//     } else {
//         // CPU1
//         while(started == 0);
//         // while (over_1 == false); // 【关键】等待 CPU0 完成前 128 个节点的分配
//         // CPU1 再开始分配，此时只能拿到剩余的后 128 个节点
//         __sync_synchronize();
//         printf("cpu %d is booting!\n", cpuid);
//         kvm_inithart();
//         trap_kernel_inithart();

//         // 申请
//         for(int i = N_MMAP / 2; i < N_MMAP; i++)
//             mmap_list[i] = mmap_region_alloc();
//         over_2 = true;

//         // 屏障
//         while(over_1 == false || over_2 == false);

//         // 释放
//         for(int i = N_MMAP / 2; i < N_MMAP; i++)
//             mmap_region_free(mmap_list[i]);
//         over_4 = true;
//     }

//     while (1);
// }

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