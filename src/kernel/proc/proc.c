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

/* ------------本地变量----------- */

// 进程结构体数组 + 第一个用户进程的指针
static proc_t proc_list[N_PROC];
static proc_t *proczero;

// 全局pid + 保护它的锁
static int global_pid;
static spinlock_t pid_lk;

/* 获取一个pid */
static int alloc_pid()
{
    int tmp = 0;
    spinlock_acquire(&pid_lk);
    assert(global_pid > 0, "alloc_pid: overflow");
    tmp = global_pid++;
    spinlock_release(&pid_lk);
    return tmp;
}

/* 释放进程锁 + trap_user_return */
static void proc_return()
{

}

/* 进程模块初始化 */
void proc_init()
{    

}

/* 
    申请一个UNUSED进程结构体(返回时带锁)
    并执行通用的初始化逻辑
*/
proc_t *proc_alloc()
{

}

/* 
    回收一个进程结构体并释放它包含的资源
    tips: 调用者需要持有进程锁
*/
void proc_free(proc_t *p)
{

}



/* 
    获得一个初始化过的用户页表
    完成trapframe和trampoline的映射
*/
pgtbl_t proc_pgtbl_init(uint64 trapframe)
{
    // 1. 分配一页作为用户根页表
    pgtbl_t upgtbl = (pgtbl_t)pmem_alloc(true);
    if (!upgtbl) {
        panic("proc_pgtbl_init: pmem_alloc failed");
    }
    memset(upgtbl, 0, PGSIZE);  // 清零

    // 2. 在用户页表中映射 trampoline 与 trapframe
    vm_mappages(upgtbl, (uint64)TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);  // 可读、不可写、可执行
    vm_mappages(upgtbl, (uint64)TRAPFRAME, (uint64)trapframe, PGSIZE, PTE_R | PTE_W);  //需要读写

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
    // 1. 申请trapframe的物理页
    trapframe_t *tf = (trapframe_t *)pmem_alloc(true);
    if (!tf) {
        panic("proc_make_first: pmem_alloc for trapframe failed");
    }
    memset(tf, 0, PGSIZE);  // 清零

    // 2. 调用proc_pgtbl_init：申请用户页表，并映射trampoline和trapframe
    pgtbl_t upgtbl = proc_pgtbl_init((uint64)tf);
    if (!upgtbl) {
        panic("proc_make_first: proc_pgtbl_init failed");
    }

    // 3. 准备用户地址空间其他部分
    // 3.1 空洞（1页） [0, PGSIZE) 不映射
    
    // 3.2  为ELF文件(code + data)申请一个物理页[PGSIZE, 2*PGSIZE)、进行数据转移、完成映射
    const uint64 UCODE_VA = PGSIZE; // 起始虚拟地址
    void *ucode_pa = pmem_alloc(false);
    if (!ucode_pa) {
        panic("proc_make_first: pmem_alloc for ucode failed");
    }
    memset(ucode_pa, 0, PGSIZE);  // 清零
    // 拷贝initcode到用户代码页
    if (initcode_len > PGSIZE) { // 长度检查
        panic("proc_make_first: initcode too big");
    }
    memmove(ucode_pa, initcode, (uint32)initcode_len);
    // 映射：设置为可读写执行、用户态可访问，覆盖initcode有/无全局变量两种情况
    vm_mappages(upgtbl, UCODE_VA, (uint64)ucode_pa, PGSIZE, PTE_R | PTE_W | PTE_X | PTE_U); 
              
    // 3.3 用户栈ustack（1页，在trapframe之下） [TRAPFRAME - PGSIZE, TRAPFRAME)
    const uint64 USTACK_TOP = (uint64)TRAPFRAME; // 栈顶
    const uint64 USTACK_VA = USTACK_TOP - PGSIZE; // 栈底
    void *ustack_pa = pmem_alloc(false);
    if (!ustack_pa) {
        panic("proc_make_first: pmem_alloc for ustack failed");
    }
    memset(ustack_pa, 0, PGSIZE);  
    // 映射：设置为可读写、用户态可访问，不允许执行（防止栈溢出攻击）
    vm_mappages(upgtbl,USTACK_VA,(uint64)ustack_pa,PGSIZE,PTE_R | PTE_W | PTE_U);


    // 4. 填充proczero结构体
    memset(proczero, 0, sizeof(*proczero)); 
    proczero->pid = 1;
    proczero->pgtbl = upgtbl;
    proczero->heap_top = 2 * PGSIZE; 
    proczero->ustack_npage = 1;       
    proczero->tf = tf;
    // 初始化 mmap 链表头
    proczero->mmap = NULL;

    // 5. 设置trapframe中的user_to_kern_epc (返回后被置为PC)、sp
    tf->user_to_kern_epc = UCODE_VA;  
    tf->sp = USTACK_TOP;  // 用户栈顶

    // 6. 设置“回到内核”的着陆点（切到 proczero 后从 trap_user_return 开始）
    proczero->kstack = (uint64)KSTACK(mycpuid());
    proczero->ctx.ra = (uint64)trap_user_return; // 返回地址
    proczero->ctx.sp = proczero->kstack + PGSIZE; // 栈指针

    // 7. 绑定到当前 CPU，并进行上下文切换（启动 proczero 执行流）
    cpu_t *c = mycpu();
    c->proc =  proczero;
    swtch(&c->ctx, &proczero->ctx);
} 

/*
    父进程产生子进程
    UNUSED -> RUNNABLE
*/
int proc_fork()
{

}

/*
    进程主动放弃CPU控制权
    RUNNING->RUNNABLE
*/
void proc_yield()
{

}

/*
    当父进程退出时, 让它的所有子进程认proczero为父
    因为proczero永不退出, 可以回收子进程的资源
*/
static void proc_reparent(proc_t *parent)
{

}

/*
    唤醒等待呼叫的进程
    由proc_exit调用
    tips: 调用者需要持有p的进程锁
*/
static void proc_try_wakeup(proc_t *p)
{

}

/*
    进程退出
    RUNNING -> ZOMBIE
*/
void proc_exit(int exit_code)
{

}

/*
    父进程等待一个子进程进入ZOMBIE状态
    1. 如果等到: 释放子进程, 返回子进程的pid, 将子进程的退出状态传出到user_addr
    2. 如果发现没孩子: 返回-1
    3. 如果没等到: 父进程进入睡眠状态 
*/
int proc_wait(uint64 user_addr)
{

}

/*
    进程等待sleep_space对应的资源, 进入睡眠状态
    RUNNING -> SLEEPING
*/
void proc_sleep(void *sleep_space, spinlock_t *lock)
{

}

/*
    唤醒所有等待sleep_space的进程
    SLEEPING -> RUNNABLE
*/
void proc_wakeup(void *sleep_space)
{

}

/* 
    用户进程切换到调度器
    tips: 调用者保证持有当前进程的锁
*/
void proc_sched()
{

}

/* 
    调度器
    RUNNABLE->RUNNING
*/
void proc_scheduler()
{

}