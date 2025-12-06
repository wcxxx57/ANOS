// // test-1: sys_getpid and sys_print
// #include "sys.h"

// int main()
// {
// 	int pid = syscall(SYS_getpid);
// 	if (pid == 1) {
// 		syscall(SYS_print_str, "\nproczero: hello ");
// 		syscall(SYS_print_str, "world!\n");
// 	}
// 	while (1);	
// }


// // test-2: fork
// #include "sys.h"

// int main()
// {
// 	syscall(SYS_print_str, "level-1!\n");
// 	syscall(SYS_fork);
// 	syscall(SYS_print_str, "level-2!\n");
// 	syscall(SYS_fork);
// 	syscall(SYS_print_str, "level-3!\n");
// 	while(1);
// }


// //test-3: fork wait exit 综合测试
// #include "sys.h"

// #define PGSIZE 4096
// #define VA_MAX (1ul << 38)
// #define MMAP_END (VA_MAX - (2 + 16 * 256) * PGSIZE)
// #define MMAP_BEGIN (MMAP_END - 64 * 256 * PGSIZE)

// int main()
// {
// 	int pid, i;
// 	char *str1, *str2, *str3 = "STACK_REGION\n\n";
// 	char *tmp1 = "MMAP_REGION\n", *tmp2 = "HEAP_REGION\n";
	
// 	str1 = (char*)syscall(SYS_mmap, MMAP_BEGIN, PGSIZE);
// 	for (i = 0; tmp1[i] != '\0'; i++)
// 		str1[i] = tmp1[i];
// 	str1[i] = '\0';	

// 	str2 = (char*)syscall(SYS_brk, 0);
// 	syscall(SYS_brk, (long long int)str2 + PGSIZE);
// 	for (i = 0; tmp2[i] != '\0'; i++)
// 		str2[i] = tmp2[i];
// 	str2[i] = '\0';	

// 	syscall(SYS_print_str, "\n--------test begin--------\n");
// 	pid = syscall(SYS_fork);

// 	if (pid == 0) { // 子进程
// 		syscall(SYS_print_str, "child proc: hello!\n");
// 		syscall(SYS_print_str, str1);
// 		syscall(SYS_print_str, str2);
// 		syscall(SYS_print_str, str3);
// 		syscall(SYS_exit, 1234);
// 	} else { // 父进程
// 		int exit_state = 0;
// 		syscall(SYS_wait, &exit_state);
// 		syscall(SYS_print_str, "parent proc: hello!\n");
// 		syscall(SYS_print_str, "num = ");
// 		syscall(SYS_print_int, pid);
// 		syscall(SYS_print_str, "\n");
// 		if (exit_state == 1234)
// 			syscall(SYS_print_str, "good boy!\n");
// 		else
// 			syscall(SYS_print_str, "bad boy!\n"); 
// 	}

// 	syscall(SYS_print_str, "--------test end----------\n");

// 	while (1);
	
// 	return 0;
// }

// test-4: sleep
// #include "sys.h"

// int main()
// {
// 	int pid = syscall(SYS_fork);
// 	if (pid == 0) {
// 		syscall(SYS_print_str, "Ready to sleep!\n");
// 		syscall(SYS_sleep, 30);
// 		syscall(SYS_print_str, "Ready to exit!\n");
// 		syscall(SYS_exit, 0);
// 	} else {
// 		syscall(SYS_wait, 0);
// 		syscall(SYS_print_str, "Child exit!\n");
// 	}
// 	while(1);
// }

// test-5: orphan process (reparent)
// #include "sys.h"

// int main()
// {
//     syscall(SYS_print_str, "Reparent test start\n");

//     int pid_a = syscall(SYS_fork);

//     if (pid_a == 0) {
//         // 子进程 A
//         int pid_b = syscall(SYS_fork);
//         if (pid_b == 0) {
//             // 孙子进程 B
//             syscall(SYS_print_str, "Grandchild (B) is running, sleeping...\n");
//             syscall(SYS_sleep, 20); // 睡一会，确保 A 先退出
//             syscall(SYS_print_str, "Grandchild (B) exit (should be adopted by PID 1)\n");
//             syscall(SYS_exit, 200);
//         } else {
//             // 子进程 A 立即退出，不等待 B
//             syscall(SYS_print_str, "Child (A) exit immediately\n");
//             syscall(SYS_exit, 100);
//         }
//     } else {
//         // Initcode (PID 1)
//         int status;
//         int wpid;

//         // 第一次 wait，应该捕获到 A
//         wpid = syscall(SYS_wait, &status);
//         syscall(SYS_print_str, "Wait 1: pid=");
//         syscall(SYS_print_int, wpid);
//         syscall(SYS_print_str, " (expect A)\n");

//         // 第二次 wait，如果 reparent 成功，应该捕获到 B
//         // 如果 reparent 失败，这里会一直阻塞或者返回 -1
//         wpid = syscall(SYS_wait, &status);
//         syscall(SYS_print_str, "Wait 2: pid=");
//         syscall(SYS_print_int, wpid);
//         syscall(SYS_print_str, " (expect B)\n");
        
//         if (wpid > 0) {
//             syscall(SYS_print_str, "Reparent test pass!\n");
//         } else {
//             syscall(SYS_print_str, "Reparent test failed!\n");
//         }
//     }
//     while(1);
//     return 0;
// }

// test-6: preemption
// #include "sys.h"

// int main()
// {
//     syscall(SYS_print_str, "Preemption test start\n");
    
//     int pid = syscall(SYS_fork);
    
//     // 两个进程都进行长循环打印
//     // 如果有抢占，输出应该是交替的 (A B A B ...)
//     // 如果无抢占，可能会看到一大片 A 然后一大片 B
    
//     if (pid == 0) {
//         for (int i = 0; i < 200; i++) {
//             if (i % 20 == 0) syscall(SYS_print_str, "A");
//             // 简单的延时循环，消耗 CPU
//             for (int j = 0; j < 1000000; j++); 
//         }
//         syscall(SYS_exit, 0);
//     } else {
//         for (int i = 0; i < 200; i++) {
//             if (i % 20 == 0) syscall(SYS_print_str, "B");
//             for (int j = 0; j < 1000000; j++);
//         }
//         syscall(SYS_wait, 0);
//     }
    
//     syscall(SYS_print_str, "\nPreemption test finish\n");
//     while(1);
//     return 0;
// }

// test-7: concurrent stress fork
#include "sys.h"

#define N_CHILDREN 5  // 同时存在的子进程数量

int main()
{
    syscall(SYS_print_str, "Concurrent fork test start\n");

    int pids[N_CHILDREN];
    int i;

    // 1. 连续创建多个子进程
    for (i = 0; i < N_CHILDREN; i++) {
        int pid = syscall(SYS_fork);
        
        if (pid < 0) {
            syscall(SYS_print_str, "Fork failed at index ");
            syscall(SYS_print_int, i);
            syscall(SYS_print_str, "\n");
            syscall(SYS_exit, 1);
        }

        if (pid == 0) {
            // 子进程逻辑：
            // 打印自己的 PID，稍微消耗点时间，然后退出
            // 这样可以增加它们同时存在的概率
            syscall(SYS_print_str, "Child running...\n");
            for(int j=0; j<1000000; j++); // 简单的延时
            syscall(SYS_exit, 0);
        } else {
            // 父进程记录 PID
            pids[i] = pid;
            // 屏障：让子进程先跑，减少输出被插入的概率
            syscall(SYS_sleep, 1);
            syscall(SYS_print_str, "Forked child pid=");
            syscall(SYS_print_int, pid);
            syscall(SYS_print_str, "\n");
        }
    }

    (void)pids; // 避免未使用警告

    // 2. 循环回收所有子进程
    // 注意：wait 返回的顺序不一定等于 fork 的顺序
    for (i = 0; i < N_CHILDREN; i++) {
        int status;
        int wpid = syscall(SYS_wait, &status);
        
        if (wpid < 0) {
            syscall(SYS_print_str, "Wait failed!\n");
            syscall(SYS_exit, 1);
        }
        // 屏障：等一 tick，避免子进程的输出插在 Reaped 行中间
        syscall(SYS_sleep, 1);
        syscall(SYS_print_str, "Reaped child pid=");
        syscall(SYS_print_int, wpid);
        syscall(SYS_print_str, "\n");
    }

    syscall(SYS_print_str, "Concurrent fork test pass!\n");
    while(1);
    return 0;
}
