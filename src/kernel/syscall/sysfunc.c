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

    printf("get an array from user: ");
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

    printf("send an array to user: ");
    for (int i = 0; i < 5; i++) {
        printf("%d ", kernel_array[i]);
    }
    printf("\n");
    
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

    printf("get a string from user: %s\n", buf);

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
    return p->heap_top;
}

/*
    增加一段内存映射
    uint64 start 起始地址
    uint32 len   范围 (字节,需检查是否是page-aligned)
    成功返回映射空间的起始地址, 失败返回-1
*/
uint64 sys_mmap()
{
    uint64 start; // 起始地址
    uint32 len;   // 地址范围
    arg_uint64(0, &start);
    arg_uint32(1, &len);

    // 检查长度是否有效
    if (len == 0) {
        printf("sys_mmap: len == 0\n");
        return (uint64)-1;
    }
    // 检查地址是否页对齐
    if (start % PGSIZE != 0) {
        printf("sys_mmap: start not page-aligned\n");
        return (uint64)-1;
    }
    if (len % PGSIZE != 0) {
        printf("sys_mmap: len not page-aligned\n");
        return (uint64)-1;
    }

    uint32 npages = len / PGSIZE;
    int perm = PTE_R | PTE_W | PTE_U; 

    uint64 ret_addr = uvm_mmap(start, npages, perm);

    // 调试
    proc_t *p = myproc();
    printf("sys_mmap: start = %p, len = 0x%x, ret_addr = %p\n", (void *)start, len, (void *)ret_addr);
    uvm_show_mmaplist(p->mmap);
    vm_print(p->pgtbl);
    printf("\n");

    return ret_addr;
}

/*
    解除一段内存映射
    uint64 start 起始地址
    uint32 len   范围 (字节, 需检查是否是page-aligned)
    成功返回0 失败返回-1
*/
uint64 sys_munmap()
{
    uint64 start; // 起始地址
    uint32 len;   // 地址范围
    arg_uint64(0, &start);
    arg_uint32(1, &len);

    if (len == 0) return (uint64)-1;
    if (start % PGSIZE != 0 || len % PGSIZE != 0) return (uint64)-1;

    uint32 npages = len / PGSIZE;
    uvm_munmap(start, npages);

    // 调试
    proc_t *p = myproc();
    printf("sys_munmap: start = %p, len = 0x%x\n", (void *)start, len);
    uvm_show_mmaplist(p->mmap);
    vm_print(p->pgtbl);
    printf("\n");

    return 0;
}


/*
    测试页表复制与销毁
    成功返回0
*/
static pgtbl_t test_pgtbl = NULL;
uint64 sys_test_pgtbl()
{
    uint32 choice;
    arg_uint32(0, &choice); 

    proc_t *p = myproc();
    switch (choice) {
        case 0: // 查询当前页表
            printf("Query Current Page Table：\n");
            vm_print(p->pgtbl);
            break;

        case 1: // 复制页表
            printf("Copy Page Table：\n");
            
            // 分配一个新的顶级页表页
            test_pgtbl = (pgtbl_t)pmem_alloc(true);

            // 执行复制 (不包括 TRAMPOLINE 和 TRAPFRAME)
            printf("Copying page table from %p to %p...\n", p->pgtbl, test_pgtbl);
            uvm_copy_pgtbl(p->pgtbl, test_pgtbl, p->heap_top, p->ustack_npage, p->mmap);
            
            // 补充映射 TRAMPOLINE (共享，重映射即可)
            pte_t *pte = vm_getpte(p->pgtbl, TRAMPOLINE, false);
            assert(pte != NULL && (*pte & PTE_V), "sys_test_pgtbl: TRAMPOLINE not mapped in original pgtbl");
            vm_mappages(test_pgtbl, TRAMPOLINE, PTE_TO_PA(*pte), PGSIZE, PTE_FLAGS(*pte));

            // 补充映射 TRAPFRAME (私有，需分配新页并拷贝)
            pte = vm_getpte(p->pgtbl, TRAPFRAME, false);
            assert(pte != NULL && (*pte & PTE_V), "sys_test_pgtbl: TRAPFRAME not mapped in original pgtbl");
            uint64 old_pa = PTE_TO_PA(*pte);
            uint64 new_pa = (uint64)pmem_alloc(false);
            assert(new_pa != 0, "sys_test_pgtbl: pmem_alloc for new trapframe failed");
            memmove((void*)new_pa, (void*)old_pa, PGSIZE);
            vm_mappages(test_pgtbl, TRAPFRAME, new_pa, PGSIZE, PTE_FLAGS(*pte));

            // 打印新页表，检查是否一致
            printf("Copied New Page Table:\n");
            vm_print(test_pgtbl);
            break;

        case 2: // 销毁页表
            printf("Destroy Copied Page Table:\n");
            if (test_pgtbl == NULL) {
                printf("Error Test: No copied page table to destroy.\n");
                return (uint64)-1;
            }
            printf("Destroying page table at PA: %p\n", test_pgtbl);
            uvm_destroy_pgtbl(test_pgtbl);
            test_pgtbl = NULL;
            printf("Destroy completed.\n");
            break;

        default:
            printf("sys_test_pgtbl: Unknown choice %d\n", choice);
            return (uint64)-1;
    }

    return 0;
}

/*
    打印一个字符串
    char *str
    成功返回0
*/
uint64 sys_print_str()
{
    uint64 addr;
    arg_uint64(0, &addr); // 获取字符串地址

    char buf[256];
    // 从用户空间拷贝字符串到内核空间
    uvm_copyin_str(myproc()->pgtbl, (uint64)buf, addr, 256);

    printf("%s", buf);

    return 0;
}

/*
    打印一个32位整数
    int num
    成功返回0
*/
uint64 sys_print_int()
{
    int num;
    arg_uint32(0, (uint32 *)&num); // 获取整数参数
    printf("%d", num);
    return 0;
}

/*
    进程复制
    返回子进程的pid
*/
uint64 sys_fork()
{
    return proc_fork();
}

/*
    等待子进程退出
    uint64 addr_exit_state
*/
uint64 sys_wait()
{
    uint64 addr_exit_state;
    arg_uint64(0, &addr_exit_state); // 获取接收退出状态的用户地址
    return proc_wait(addr_exit_state);
}

/*
    进程退出
    int exit_code
    不返回
*/
uint64 sys_exit()
{
    int exit_code;
    arg_uint32(0, (uint32 *)&exit_code); // 获取退出码
    proc_exit(exit_code);
    return 0; // 不会执行到这里
}

/*
    让进程睡眠一段时间
    uint32 ntick (1个tick大约0.1秒)
    成功返回0
*/
uint64 sys_sleep()
{
    uint32 ntick;
    arg_uint32(0, &ntick); // 获取睡眠的tick数
    timer_wait((uint64)ntick);
    return 0;
}

/*
    返回当前进程的pid
*/
uint64 sys_getpid()
{
    return (uint64)(myproc()->pid);
}

/*---------------- buffer 相关 syscalls ----------------*/

// 获取一个描述指定块的buffer，返回buffer的内核地址（用于后续读写）
uint64 sys_get_block()
{
    uint32 block_num;
    arg_uint32(0, &block_num);
    buffer_t *b = buffer_get(block_num);
    return (uint64)b;
}

// 将buf->data拷贝到用户空间地址
uint64 sys_read_block()
{
    uint64 buf_addr, user_addr;
    arg_uint64(0, &buf_addr);
    arg_uint64(1, &user_addr);

    buffer_t *b = (buffer_t *)buf_addr;
    // 读取时如果调用者尚未读磁盘，可再保证一次（持锁即一致）
    if (!sleeplock_held(&b->slk))
        sleeplock_acquire(&b->slk);
    // 将缓冲区数据拷贝到用户空间
    uvm_copyout(myproc()->pgtbl, user_addr, (uint64)b->data, BLOCK_SIZE);
    sleeplock_release(&b->slk);
    return 0;
}

// 将用户空间数据写入到buf->data并写回磁盘
uint64 sys_write_block()
{
    uint64 buf_addr, user_addr;
    arg_uint64(0, &buf_addr);
    arg_uint64(1, &user_addr);

    buffer_t *b = (buffer_t *)buf_addr;
    if (!sleeplock_held(&b->slk))
        sleeplock_acquire(&b->slk);
    // 先把用户数据拷入缓冲区
    uvm_copyin(myproc()->pgtbl, (uint64)b->data, user_addr, BLOCK_SIZE);
    // 再写入磁盘
    buffer_write(b);
    sleeplock_release(&b->slk);
    return 0;
}

// 释放一个buffer（引用计数-1，可能移入不活跃队列）
uint64 sys_put_block()
{
    uint64 buf_addr;
    arg_uint64(0, &buf_addr);
    buffer_t *b = (buffer_t *)buf_addr;
    buffer_put(b);
    return 0;
}

// 打印buffer链表状态（仅测试）
uint64 sys_show_buffer()
{
    buffer_print_info();
    return 0;
}

// 释放非活跃链表中的物理页缓存
uint64 sys_flush_buffer()
{
    uint32 n;
    arg_uint32(0, &n);
    return (uint64)buffer_freemem(n);
}