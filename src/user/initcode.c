#include "sys.h"

// 与内核保持一致
#define VA_MAX       (1ul << 38)
#define PGSIZE       4096
#define MMAP_END     (VA_MAX - (16 * 256 + 2) * PGSIZE)
#define MMAP_BEGIN   (MMAP_END - 64 * 256 * PGSIZE)

int main()
{
    // // 测试一：测试用户态与内核态的数据传递
    // int L[5];
    // char* s = "hello, world"; 
    // syscall(SYS_copyout, L);
    // syscall(SYS_copyin, L, 5);
    // syscall(SYS_copyinstr, s);
    // while(1);
    // return 0;

    // // 测试二：测试堆空间增长与收缩
    // long long heap_top = 0;
    // heap_top = syscall(SYS_brk, 0);
    // heap_top = syscall(SYS_brk, heap_top + PGSIZE * 9);
    // heap_top = syscall(SYS_brk, heap_top);
    // heap_top = syscall(SYS_brk, heap_top - PGSIZE * 5);
    // while(1);
    // return 0;

    // // 测试三：测试用户栈空间的自动增长
    // char tmp[PGSIZE * 4];

    // tmp[PGSIZE * 3] = 'h';
    // tmp[PGSIZE * 3 + 1] = 'e';
    // tmp[PGSIZE * 3 + 2] = 'l';
    // tmp[PGSIZE * 3 + 3] = 'l';
    // tmp[PGSIZE * 3 + 4] = 'o';
    // tmp[PGSIZE * 3 + 5] = '\0';

    // syscall(SYS_copyinstr, tmp + PGSIZE * 3);

    // tmp[0] = 'w';
    // tmp[1] = 'o';
    // tmp[2] = 'r';
    // tmp[3] = 'l';
    // tmp[4] = 'd';
    // tmp[5] = '\0';

    // syscall(SYS_copyinstr, tmp);

    // while (1);
    // return 0;

    // // 测试五：测试mmap和munmap
    // // 建议画图理解这些地址和长度的含义
    // // sys_mmap 测试 
    // syscall(SYS_mmap, MMAP_BEGIN + 4 * PGSIZE, 3 * PGSIZE);
    // syscall(SYS_mmap, MMAP_BEGIN + 10 * PGSIZE, 2 * PGSIZE);
    // syscall(SYS_mmap, MMAP_BEGIN + 2 * PGSIZE,  2 * PGSIZE);
    // syscall(SYS_mmap, MMAP_BEGIN + 12 * PGSIZE, 1 * PGSIZE);
    // syscall(SYS_mmap, MMAP_BEGIN + 7 * PGSIZE, 3 * PGSIZE);
    // syscall(SYS_mmap, MMAP_BEGIN, 2 * PGSIZE);
    // syscall(SYS_mmap, 0, 10 * PGSIZE);

    // // sys_munmap 测试
    // syscall(SYS_munmap, MMAP_BEGIN + 10 * PGSIZE, 5 * PGSIZE);
    // syscall(SYS_munmap, MMAP_BEGIN, 10 * PGSIZE);
    // syscall(SYS_munmap, MMAP_BEGIN + 17 * PGSIZE, 2 * PGSIZE);
    // syscall(SYS_munmap, MMAP_BEGIN + 15 * PGSIZE, 2 * PGSIZE);
    // syscall(SYS_munmap, MMAP_BEGIN + 19 * PGSIZE, 2 * PGSIZE);
    // syscall(SYS_munmap, MMAP_BEGIN + 22 * PGSIZE, 1 * PGSIZE);
    // syscall(SYS_munmap, MMAP_BEGIN + 21 * PGSIZE, 1 * PGSIZE);

    // 补充测试
    // 1. 参数检查测试 
    // 预期：内核应拒绝非法参数，且不应 panic
    syscall(SYS_mmap, MMAP_BEGIN + 100, PGSIZE); // 错误：地址非页对齐
    syscall(SYS_mmap, MMAP_BEGIN, PGSIZE - 1);   // 错误：长度非页对齐
    syscall(SYS_mmap, MMAP_BEGIN, 0);            // 错误：长度为0

    // 2. 重叠检测测试
    // 预期：第一次申请成功，第二次申请应失败，panic 卡死
    syscall(SYS_mmap, MMAP_BEGIN, PGSIZE);       // 成功：申请一页
    syscall(SYS_mmap, MMAP_BEGIN, PGSIZE);       // 失败：重叠
    syscall(SYS_munmap, MMAP_BEGIN, PGSIZE);     // 清理

    // 3. 自动分配策略验证 
    typedef unsigned long long uint64;
    // 构造布局: [P1] [P2] [P3]
    uint64 p1 = syscall(SYS_mmap, 0, PGSIZE);
    uint64 p2 = syscall(SYS_mmap, 0, PGSIZE);
    uint64 p3 = syscall(SYS_mmap, 0, PGSIZE);
    
    // 制造空洞: [P1] [Hole] [P3]
    syscall(SYS_munmap, p2, PGSIZE);

    // 再次申请: 预期内核应优先填补 P2 的空洞 (First-Fit)，而不是在 P3 后面分配
    uint64 p_new = syscall(SYS_mmap, 0, PGSIZE);
    
    // 清理所有
    syscall(SYS_munmap, p1, PGSIZE);
    syscall(SYS_munmap, p_new, PGSIZE); // 如果 p_new == p2，这里能正常释放
    syscall(SYS_munmap, p3, PGSIZE);

    // 4. 跨节点解除映射
    // 这是一个复杂场景：一次 munmap 操作跨越多个不连续的节点
    // 构造: Node A [0, 2) ... Node B [3, 5)  (单位: Page)
    syscall(SYS_mmap, MMAP_BEGIN, 2 * PGSIZE);
    syscall(SYS_mmap, MMAP_BEGIN + 3 * PGSIZE, 2 * PGSIZE);

    // 释放 [1, 4): 跨越了 A 的尾部 (Page 1) 和 B 的头部 (Page 3)
    // 预期结果: 
    // Node A 被去尾 -> 剩 [0, 1)
    // Node B 被砍头 -> 剩 [4, 5)
    // 中间的空洞区域 [2, 3) 本来就没映射，munmap 应静默忽略
    syscall(SYS_munmap, MMAP_BEGIN + 1 * PGSIZE, 3 * PGSIZE);

    // 清理剩余碎片
    syscall(SYS_munmap, MMAP_BEGIN, 1 * PGSIZE);             // 清理 A 的剩余
    syscall(SYS_munmap, MMAP_BEGIN + 4 * PGSIZE, 1 * PGSIZE); // 清理 B 的剩余

    while(1);
    return 0;
}