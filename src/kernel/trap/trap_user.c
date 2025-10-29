#include "mod.h"

// in trampoline.S
extern char trampoline[];  // 内核和用户切换的代码
extern char user_vector[]; // 用户触发陷阱进入内核
extern char user_return[]; // 内核处理完毕返回用户

// in trap.S
extern char kernel_vector[]; // 内核态trap处理流程, 进入内核后应当切换中断处理入口

// in trap_kernel.c
extern char *interrupt_info[16]; // 中断错误信息
extern char *exception_info[16]; // 异常错误信息

// 在user_vector()里面调用
// 用户态trap处理的核心逻辑
void trap_user_handler()
{
    printf("ENTER trap_user_handler: scause=%p sepc=%p\n", r_scause(), r_sepc()); 
    struct proc *p = myproc(); // 获取当前进程
    struct trapframe *tf = p->tf;

    // 读取关键寄存器
    uint64 sepc = r_sepc();       // trap发生时的PC
    uint64 sstatus = r_sstatus();
    uint64 scause = r_scause();

    // 确保是从 U-mode 来的
    assert((sstatus & SSTATUS_SPP) == 0, "trap_user_handler: not from u-mode");

    // 记录发生 trap 时的 PC，用于后续恢复
    tf->user_to_kern_epc = sepc;

    /* ================== 开始处理 trap ================== */

    int trap_id = scause & 0x3FF;  // 取低10位（RISC-V标准）

    if (scause & 0x8000000000000000UL) {
        // 1. 中断处理：U-mode不会触发中断，直接报错
        panic("trap_user_handler: interrupt from user?");
    } else {
        // 2. 异常处理
        switch (trap_id) {
            case 8: // Environment call from U-mode (ecall)
            {
                uint64 num = tf->a7; // 系统调用号
                uint64 ret = 0;
                switch (num) {
                    case SYS_helloworld:
                        // 第一个用户进程的系统调用：打印信息
                        printf("proczero: hello world!\n");
                        ret = 0;
                        break;
                    default://! 其余系统调用暂未实现，直接报错
                        panic("trap_user_handler: unknown syscall");
                }
                tf->a0 = ret; // 系统调用返回值写入 a0
                tf->user_to_kern_epc += 4; // 系统调用返回时，PC 应该是 sepc + 4（类似中断）
                break;
            }
            //! 其余异常类型暂时不处理，直接报错
            default:
                panic("trap_user_handler");
        }
    }

    /* ================== Trap 处理结束 ================== */

    // 重写trap入口为kernel_vector，这样以后的中断可以直接走高效路径
    w_stvec((uint64)kernel_vector);

    //! 【可选】打开中断（如果你之前关了）
    intr_on();
}

// 调用user_return()
// 内核态返回用户态
void trap_user_return()
{
    struct proc *p = myproc();           // 当前进程结构体
    struct trapframe *tf = p->tf; // 进程的 trapframe（保存有用户态寄存器）
    uint64 user_satp = MAKE_SATP(p->pgtbl);  // 获取用户页表的 satp 值（把进程页表的物理地址编码成 RISC-V satp 寄存器格式，用于让 CPU 切换到该用户进程的虚拟地址空间）
    //! 这个MAKE_SATP宏定义在mem/type.h中
    printf("ENTER trap_user_return: tf=%p pgtbl=%p\n", tf, p->pgtbl);

    // 保存内核侧必要的信息到 trapframe，trampoline 会依赖这些字段来恢复内核环境
    tf->user_to_kern_satp = r_satp();                      // 内核当前的 satp（内核页表）
    tf->user_to_kern_sp = p->kstack + PGSIZE;              // 内核栈顶
    tf->user_to_kern_trapvector = (uint64)trap_user_handler;// 用户态trap进入内核后由此函数处理
    tf->user_to_kern_hartid = mycpuid();                   // 当前 hart id

    // 将 trapframe 地址写入 sscratch，trampoline/user_vector 依赖此值来保存/恢复寄存器
    w_sscratch((uint64)tf);

    // 将 S-mode 的 trap 入口再设置回 user_vector（trampoline）
    w_stvec((uint64)user_vector);

    // 设置返回用户态时的 sepc 和 sstatus（使 sret 返回到 U-mode）
    w_sepc(tf->user_to_kern_epc);
    uint64 sstatus = r_sstatus();
    sstatus &= ~SSTATUS_SPP; // 清 SPP，表示 sret 将返回到 U-mode
    sstatus |= SSTATUS_SPIE; // 置 SPIE，使 sret 返回后 U-mode 中断可用
    w_sstatus(sstatus);

     printf("tf->user_to_kern_epc=%p satp=%p sp=%p trapvec=%p hart=%d\n", tf->user_to_kern_epc, tf->user_to_kern_satp, tf->user_to_kern_sp, (void*)tf->user_to_kern_trapvector, (int)tf->user_to_kern_hartid);
    // 调用 assembly 的 user_return，真正切换到用户页表并 sret 返回
    ((void (*)(struct trapframe*, uint64))user_return)(tf, user_satp);
}