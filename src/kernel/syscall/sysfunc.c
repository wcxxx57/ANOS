#include "mod.h"

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
    if (!sleeplock_holding(&b->slk))
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
    if (!sleeplock_holding(&b->slk))
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

/*
    从data_bitmap申请1个block
    返回 block_num
*/
uint64 sys_alloc_block()
{
    return (uint64)bitmap_alloc_block();
}

/*
    向data_bitmap释放1个block
    参数: block_num
*/
uint64 sys_free_block()
{
    uint32 block_num;
    arg_uint32(0, &block_num);
    bitmap_free_block(block_num);
    return 0;
}

/*
    从inode_bitmap申请1个inode
    返回 inode_num
*/
uint64 sys_alloc_inode()
{
    return (uint64)bitmap_alloc_inode();
}

/*
    向inode_bitmap释放1个inode
    参数: inode_num
*/
uint64 sys_free_inode()
{
    uint32 inode_num;
    arg_uint32(0, &inode_num);
    bitmap_free_inode(inode_num);
    return 0;
}

/*
    输出目标bitmap的状态
    参数: bitmap_type (0: data_bitmap, 1: inode_bitmap)
*/
uint64 sys_show_bitmap()
{
    uint32 bitmap_type;
    arg_uint32(0, &bitmap_type);
    // type == 0 打印 data bitmap, type == 1 打印 inode bitmap
    bitmap_print(bitmap_type == 0);
    return 0;
}