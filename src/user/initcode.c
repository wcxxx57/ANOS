#include "sys.h"

#define PGSIZE 4096

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

    // 测试三：测试用户栈空间的自动增长
    char tmp[PGSIZE * 4];

    tmp[PGSIZE * 3] = 'h';
    tmp[PGSIZE * 3 + 1] = 'e';
    tmp[PGSIZE * 3 + 2] = 'l';
    tmp[PGSIZE * 3 + 3] = 'l';
    tmp[PGSIZE * 3 + 4] = 'o';
    tmp[PGSIZE * 3 + 5] = '\0';

    syscall(SYS_copyinstr, tmp + PGSIZE * 3);

    tmp[0] = 'w';
    tmp[1] = 'o';
    tmp[2] = 'r';
    tmp[3] = 'l';
    tmp[4] = 'd';
    tmp[5] = '\0';

    syscall(SYS_copyinstr, tmp);

    while (1);
    return 0;
}