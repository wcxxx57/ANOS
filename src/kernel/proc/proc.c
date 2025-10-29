#include "mod.h"

// 这个文件通过make build生成, 是proczero对应的ELF文件
#include "../../user/initcode.h"
#define initcode target_user_initcode
#define initcode_len target_user_initcode_len

// in trampoline.S
extern char trampoline[];

// in swtch.S
extern void swtch(context_t *old, context_t *new);

// in trap/trap_user.c
extern void trap_user_return();

// 第一个用户进程
static proc_t proczero;

// 获得一个初始化过的用户页表
// 完成trapframe和trampoline的映射
pgtbl_t proc_pgtbl_init(uint64 trapframe)
{
    // 1. 分配一页作为用户根页表
    pgtbl_t upgtbl = (pgtbl_t)pmem_alloc(true);
    if (!upgtbl) {
        panic("proc_pgtbl_init: pmem_alloc failed");
    }
    memset(upgtbl, 0, PGSIZE);  // 清零

    // 2. 在用户页表中映射 trampoline 与 trapframe
    // 两者都不允许U-mode访问 -> 不设置PTE_U
    // trampoline: U-mode进入S-mode的汇编代码 
    vm_mappages(upgtbl,
                (uint64)TRAMPOLINE,
                (uint64)trampoline,
                PGSIZE,
                PTE_R | PTE_X);  // 只读（不可写）、可执行
    // trapframe: 用户进程陷入内核前, 保存用户执行流的上下文 
    vm_mappages(upgtbl,
                (uint64)TRAPFRAME,
                (uint64)trapframe,
                PGSIZE,
                PTE_R | PTE_W);  //需要读写

    return upgtbl;
}

/*
    第一个用户态进程的创建
    它的代码和数据位于initcode.h的initcode数组

    第一个进程的用户地址空间布局:
    trapoline   (1 page)
    trapframe   (1 page)
    ustack      (1 page)
    .......
                        <--heap_top
    code + data (1 page)
    empty space (1 page) 最低的4096字节 不分配物理页，同时不可访问

	注意: 用用户空间的地址映射需要标记 PTE_U
*/
void proc_make_first()
{
    // main.c中已经设置了仅在cpu 0中创建第一个用户进程，避免多cpu重复创建

    // 1. 申请trapframe的物理页
    trapframe_t *tf = (trapframe_t *)pmem_alloc(true);
    if (!tf) {
        panic("proc_make_first: pmem_alloc for trapframe failed");
    }
    memset(tf, 0, PGSIZE);  // 清零

    // 2. 申请用户页表，并映射trampoline和trapframe
    pgtbl_t upgtbl = proc_pgtbl_init((uint64)tf);
    if (!upgtbl) {
        panic("proc_make_first: proc_pgtbl_init failed");
    }

    // 3. 准备用户地址空间其他部分
    // 3.1 空洞（1页） [0, PGSIZE) 不映射
    
    // 3.2 用户代码+数据（1页） [PGSIZE, 2*PGSIZE)
    const uint64 UCODE_VA = PGSIZE; // 起始虚拟地址
    void *ucode_pa = pmem_alloc(false);
    if (!ucode_pa) {
        panic("proc_make_first: pmem_alloc for ucode failed");
    }
    memset(ucode_pa, 0, PGSIZE);  // 清零
    // 拷贝initcode到用户代码页
    if (initcode_len > PGSIZE) { // 拷贝前先长度保护
        panic("proc_make_first: initcode too big");
    }
    memmove(ucode_pa, initcode, (uint32)initcode_len);
    // 映射：设置为可读写执行、用户态可访问，覆盖initcode有/无全局变量两种情况
    vm_mappages(upgtbl, 
                UCODE_VA, 
                (uint64)ucode_pa, 
                PGSIZE, 
                PTE_R | PTE_W | PTE_X | PTE_U); 
              
    // 3.3 用户栈（1页，在trapframe之下） [TRAPFRAME - PGSIZE, TRAPFRAME)
    const uint64 USTACK_TOP = (uint64)TRAPFRAME; // 栈顶地址
    const uint64 USTACK_VA = USTACK_TOP - PGSIZE; // 栈底地址
    void *ustack_pa = pmem_alloc(false);
    if (!ustack_pa) {
        panic("proc_make_first: pmem_alloc for ustack failed");
    }
    memset(ustack_pa, 0, PGSIZE);  // 清零
    // 映射：设置为可读写、用户态可访问，不允许执行（防止栈溢出攻击）
    vm_mappages(upgtbl,
                USTACK_VA,
                (uint64)ustack_pa,
                PGSIZE,
                PTE_R | PTE_W | PTE_U);

    // 4. 填充proczero结构体
    memset(&proczero, 0, sizeof(proczero)); // 清零
    proczero.pid = 1;
    proczero.pgtbl = upgtbl;
    proczero.heap_top = 2 * PGSIZE; 
    proczero.ustack_npage = 1;       
    proczero.tf = tf;

    // 5. 初始化用户初始寄存器状态（将由 user_return 装载）
    tf->user_to_kern_epc = UCODE_VA;  // 用户代码入口点
    tf->sp = USTACK_TOP;  // 用户栈顶

    // 6. 设置“回到内核”的着陆点（切到 proczero 后从 trap_user_return 开始）
    proczero.kstack = (uint64)KSTACK(mycpuid());
    proczero.ctx.ra = (uint64)trap_user_return; // 返回地址
    proczero.ctx.sp = proczero.kstack + PGSIZE; // 栈指针

    // 7. 绑定到当前 CPU，并进行上下文切换（启动 proczero 执行流）
    cpu_t *c = mycpu();
    c->proc =  &proczero;
    swtch(&c->ctx, &proczero.ctx);
    // 正常情况下不再返回；以后用户态陷入内核后，才会再次切回这里
} 