#include "mod.h"
#include "../mem/mod.h"  
#define UCODE_VA PGSIZE

// in trampoline.S
extern char trampoline[];  // 内核和用户切换的代码
extern char user_vector[]; // 用户触发陷阱进入内核
extern char user_return[]; // 内核处理完毕返回用户

// in trap.S
extern char kernel_vector[]; // 内核态trap处理流程, 进入内核后应当切换中断处理入口

// in trap_kernel.c
extern char *interrupt_info[16]; // 中断错误信息
extern char *exception_info[16]; // 异常错误信息

// 需要使用虚拟地址！！
static inline uint64 tramp_user_vec(void)
{
    return (uint64)TRAMPOLINE + (uint64)(user_vector - trampoline);
}

static inline uint64 tramp_user_ret(void)
{
    return (uint64)TRAMPOLINE + (uint64)(user_return - trampoline);
}


// 屏蔽 S 态中断源
static inline void s_mask_all_irqs(void)
{
    // 屏蔽 S 态软件/外部中断使能
    uint64 sie = r_sie();
    sie &= ~(SIE_SEIE | SIE_SSIE);
    w_sie(sie);
}

// 在user_vector()里面调用
// 用户态trap处理的核心逻辑
void trap_user_handler()
{
    // // 进入内核后先切回 kernel_vector，避免后续内核期间中断再次进 user_vector
    w_stvec((uint64)kernel_vector);
    
    proc_t *p = myproc(); // 获取当前进程
    trapframe_t *tf = p->tf;

    // 读取关键寄存器
    uint64 sepc = r_sepc();       // trap发生时的PC
    uint64 sstatus = r_sstatus();
    uint64 scause = r_scause();
    int fromU = ((sstatus & SSTATUS_SPP) == 0);  // 1 表示来自 U 态

    // printf("ENTER trap_user_handler: scause=%p sepc=%p\n", scause, sepc);
    tf->user_to_kern_epc = sepc;

    /* ================== 开始处理 trap ================== */

    int trap_id = scause & 0x3FF;  // 取低10位（RISC-V标准）

    if (scause & 0x8000000000000000UL) {
        // 1. 中断处理
        switch (trap_id) {
            case 1: // S-mode软件中断
                timer_interrupt_handler();
                break;
            case 9: // S-mode外设中断
                external_interrupt_handler();
                break;
            default:
                panic("trap_user_handler: unknown interrupt");
        }
        // 来自 S 的中断：不要改 tf->user_to_kern_epc（保持用户目标不变）
    } else {
        // 2. 异常处理
        switch (trap_id) {
            case 8: // Environment call from U-mode (ecall)
            {
                if (!fromU) panic("ecall not from user");
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
                panic("trap_user_handler: unknown exception");
        }
    }

    // 统一走“返回用户”的路径；user_vector 是 jalr/jr 进入，不能直接 return
    trap_user_return();

    /* ================== Trap 处理结束 ================== */

    // 重写trap入口为kernel_vector，这样以后的中断可以直接走高效路径
    // w_stvec((uint64)kernel_vector);

    //! 【可选】打开中断（如果你之前关了）
    // intr_on();
}

// 调用user_return()
// 内核态返回用户态
void trap_user_return()
{
    proc_t *p = myproc();           // 当前进程结构体
    trapframe_t *tf = p->tf; // 进程的 trapframe（保存有用户态寄存器）
    uint64 user_satp = MAKE_SATP(p->pgtbl);  // 获取用户页表的 satp 值（把进程页表的物理地址编码成 RISC-V satp 寄存器格式，用于让 CPU 切换到该用户进程的虚拟地址空间）
    //! 这个MAKE_SATP宏定义在mem/type.h中
    // printf("ENTER trap_user_return: tf=%p pgtbl=%p\n", tf, p->pgtbl);

    // // debug: 打印 trampoline/user_vector/user_return 计算出的高地址
    // uint64 tv = tramp_user_vec();
    // uint64 tr = tramp_user_ret();
    // printf("DEBUG trap: trampoline_sym=%p TRAMPOLINE=%p user_vec=%p user_ret=%p\n",
    //         trampoline, (uint64)TRAMPOLINE, tv, tr);

    // printf("DEBUG: trampoline sym=%p TRAMPOLINE=%p\n", trampoline, (uint64)TRAMPOLINE);
    // printf("DEBUG: bytes @trampoline:");
    // for (int i=0;i<16;i++) printf(" %x", ((uint8*)trampoline)[i]);
    // printf("\nDEBUG: bytes @TRAMPOLINE:");
    // for (int i=0;i<16;i++) printf(" %x", ((uint8*)TRAMPOLINE)[i]);
    // printf("\n");

    // 在把 stvec 切到 user_vector 之前关闭 S 态中断
    intr_off(); // 关闭中断

    // 保存内核侧必要的信息到 trapframe，trampoline 会依赖这些字段来恢复内核环境
    tf->user_to_kern_satp = r_satp();                      // 内核当前的 satp（内核页表）
    tf->user_to_kern_sp = p->kstack + PGSIZE;              // 内核栈顶
    tf->user_to_kern_trapvector = (uint64)trap_user_handler;// 用户态trap进入内核后由此函数处理
    tf->user_to_kern_hartid = mycpuid();                   // 当前 hart id

    // 将 trapframe 地址写入 sscratch，trampoline/user_vector 依赖此值来保存/恢复寄存器
    // 必须写入 TRAPFRAME 的"固定虚拟地址"（trampoline 在切页表前使用该虚拟地址）!!!
    // w_sscratch((uint64)tf); 不对！这是物理页！
    w_sscratch((uint64)TRAPFRAME);

    // 将 S-mode 的 trap 入口再设置回 user_vector（trampoline）
    w_stvec(tramp_user_vec());

    // 设置返回用户态时的 sepc 和 sstatus（使 sret 返回到 U-mode）
    w_sepc(tf->user_to_kern_epc);
    uint64 sstatus = r_sstatus();
    sstatus &= ~SSTATUS_SPP; // 清 SPP，表示 sret 将返回到 U-mode
    sstatus |= SSTATUS_SPIE; // 置 SPIE，使 sret 返回后 U-mode 中断可用
    w_sstatus(sstatus);

    // // debug: 打印将写入 sepc 的值
    // printf("DEBUG trap: will set sepc=%p, user_satp=%p\n", (void*)tf->user_to_kern_epc, (uint64)user_satp);

    // // debug: 检查 UCODE_VA 在用户页表中的 pte 与物理内容
    // {
    //     pte_t *pte = vm_getpte(p->pgtbl, UCODE_VA, false);
    //     if (!pte) {
    //         printf("DEBUG trap: no pte for UCODE_VA\n");
    //     } else {
    //         uint64 pteval = (uint64)(*pte);
    //         uint64 pa = PTE_TO_PA(pteval);
    //         printf("DEBUG trap: upgtbl pte=%p val=0x%x flags=0x%x pa=%p\n", pte, pteval, (int)PTE_FLAGS(pteval), (void*)pa);
    //         uint8 *kva = (uint8 *)pa;
    //         printf("DEBUG trap: mapped content at pa:");
    //         for (int i = 0; i < 16; i++) printf(" %x", (unsigned int)kva[i]);
    //         printf("\n");
    //     }
    // }

    // printf("tf->user_to_kern_epc=%p satp=%p sp=%p trapvec=%p hart=%d\n", tf->user_to_kern_epc, tf->user_to_kern_satp, tf->user_to_kern_sp, (void*)tf->user_to_kern_trapvector, (int)tf->user_to_kern_hartid);
    
    // 跳到 TRAMPOLINE 上的 user_return：切页表并 sret
    // ((void (*)(trapframe_t*, uint64))tramp_user_ret())(tf, user_satp); 不对！需要虚拟地址！
    ((void (*)(trapframe_t*, uint64))tramp_user_ret())((trapframe_t*)TRAPFRAME, user_satp);
}