#include "mod.h"

/*
    测试: 从用户空间传入一个int类型的数组
    uint64 addr 数组起始地址
    uint32 len  元素数量
    成功返回0
*/
uint64 sys_copyin()
{
    uint64 addr;
    uint32 len;
    arg_uint64(0, &addr); // 获取第0号参数: addr
    arg_uint32(1, &len); // 获取第1号参数: len

    int buf[5]; // 假设最多拷贝5个（用于测试）
    uvm_copyin(myproc()->pgtbl, (uint64)buf, addr, len * sizeof(int));

    printf("sys_copyin: ");
    for (uint32 i = 0; i < len; i++) {
        printf("%d ", buf[i]);
    }
    printf("\n");

    return 0;
}

/*
    测试: 向用户空间传出一个int类型的数组
    uint64 addr 数组起始地址
    成功返回拷贝的元素数量
*/

static int kernel_array[5] = {1, 2, 3, 4, 5};// 内核中的测试数组
uint64 sys_copyout()
{
    uint64 addr;
    arg_uint64(0, &addr);

    uvm_copyout(myproc()->pgtbl, addr, (uint64)kernel_array, 5 * sizeof(int));

    return 0;
}

/*
    测试: 从用户空间传入一个字符串
    uint64 addr 字符串起始地址
    成功返回0
*/
uint64 sys_copyinstr()
{
    uint64 addr;
    arg_uint64(0, &addr);

    char buf[256];
    uvm_copyin_str(myproc()->pgtbl, (uint64)buf, addr, 256);

    printf("sys_copyinstr: %s\n", buf);

    return 0;
}

/*
    用户堆空间伸缩
    uint64 new_heap_top (如果是0, 代表查询当前堆顶位置)
    成功返回new_heap_top, 失败返回-1
*/
uint64 sys_brk()
{
    // push_off(); // 上锁防止时钟中断干扰页表打印（不加好像也行）

    uint64 new_top;
    arg_uint64(0, &new_top);

    proc_t *p = myproc();
    if (!p) return (uint64)-1;

    uint64 cur = p->heap_top;

    if (new_top == 0) { // look
        printf("look event: ret_heap_top = %p\n", cur);

    }else if (new_top > cur) { // grow
        uint32 len = (uint32)(new_top - cur);
        uint64 ret = uvm_heap_grow(p->pgtbl, cur, len);
        if (ret == (uint64)-1) return (uint64)-1;
        p->heap_top = ret;
        printf("grow event: ret_heap_top = %p old_heap_top = %p len = 0x%x\n", (void *)ret, (void *)cur, len);

    }else { // ungrow & stay
        uint32 len = (uint32)(cur - new_top);
        uint64 ret = uvm_heap_ungrow(p->pgtbl, cur, len);
        if (ret == (uint64)-1) return (uint64)-1;
        p->heap_top = ret;
        printf("ungrow event: ret_heap_top = %p old_heap_top = %p len = 0x%x\n", (void *)ret, (void *)cur, len);

    }
    printf("After the event: Current pgtbl:\n");
    vm_print(p->pgtbl);
    // pop_off();
    return cur;
}

/*
    增加一段内存映射
    uint64 start 起始地址
    uint32 len   范围 (字节,需检查是否是page-aligned)
    成功返回映射空间的起始地址, 失败返回-1
*/
uint64 sys_mmap()
{
    return 0;
}

/*
    解除一段内存映射
    uint64 start 起始地址
    uint32 len   范围 (字节, 需检查是否是page-aligned)
    成功返回0 失败返回-1
*/
uint64 sys_munmap()
{
    return 0;
}