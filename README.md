# LAB-4: 第一个用户进程的诞生

在lab-4中，我们实现了第一个用户进程的诞生，重点完成了**用户进程管理**和**用户态陷阱处理**，并成功测试了**内核对用户程序syscall的响应**。

我们的分工如下：

**丁熙妍**：完成了`porc.c`中**用户进程管理**和**系统调用的测试修复**，以及对应的实验文档。

**吴晨曦**：完成了`kvm.c`中**内核页表补充映射**和`trap_user.c`中**用户态陷阱处理**，以及对应的实验文档

---

## 代码组织结构

```
ECNU-OSLAB-2025-TASK
├── LICENSE        开源协议
├── .vscode        配置了可视化调试环境
├── registers.xml  配置了可视化调试环境
├── .gdbinit.tmp-riscv xv6自带的调试配置
├── common.mk      Makefile中一些工具链的定义
├── Makefile       编译运行整个项目
├── kernel.ld      定义了内核程序在链接时的布局 (CHANGE, 支持trampsec)
├── pictures       README使用的图片目录 (CHANGE, 日常更新)
├── README.md      实验指导书 (CHANGE, 日常更新)
└── src            源码
    ├── kernel     内核源码
    │   ├── arch   RISC-V相关
    │   │   ├── method.h
    │   │   ├── mod.h
    │   │   └── type.h
    │   ├── boot   机器启动
    │   │   ├── entry.S
    │   │   └── start.c
    │   ├── lock   锁机制
    │   │   ├── spinlock.c
    │   │   ├── method.h
    │   │   ├── mod.h
    │   │   └── type.h
    │   ├── lib    常用库
    │   │   ├── cpu.c (CHANGE, 新增myproc函数)
    │   │   ├── print.c
    │   │   ├── uart.c
    │   │   ├── utils.c
    │   │   ├── method.h (CHANGE, 新增myproc函数)
    │   │   ├── mod.h
    │   │   └── type.h (CHANGE, 扩充CPU结构体 + 帮助)
    │   ├── mem    内存模块
    │   │   ├── pmem.c
    │   │   ├── kvm.c (本实验完成, 增加内核页表的映射内容 trampoline + KSTACK(0))
    │   │   ├── method.h
    │   │   ├── mod.h
    │   │   └── type.h
    │   ├── trap   陷阱模块
    │   │   ├── plic.c
    │   │   ├── timer.c
    │   │   ├── trap_kernel.c (CHANGE, 去掉了提示信息的static标记)
    │   │   ├── trap_user.c (本实验完成, 用户态陷阱处理)
    │   │   ├── trap.S
    │   │   ├── trampoline.S
    │   │   ├── method.h (CHANGE, 增加函数定义)
    │   │   ├── mod.h
    │   │   └── type.h
    │   ├── proc   进程模块
    │   │   ├── proc.c (本实验完成, 进程管理核心逻辑)
    │   │   ├── swtch.S (NEW, 上下文切换)
    │   │   ├── method.h (NEW)
    │   │   ├── mod.h (NEW)
    │   │   └── type.h (NEW)
    │   └── main.c (CHANGE, 日常更新)
    └── user       用户程序
        ├── initcode.c (NEW)
        ├── sys.h (NEW)
        ├── syscall_arch.h (NEW)
        └── syscall_num.h (NEW)
```

相比于上一个实验，本次实验主要增加了以下功能：

- 补充了**内核页表的映射**（tampoline区域和KSTACK(0)内核栈）
- 完成了第一个用户进程**proczero的初始化**（包括用户页表映射和上下文切换等）
- 完成了**用户态陷阱的处理流程**（包括内核态到用户态的准备工作，用户态系统调用的处理等）

---

## 具体实现概述

### 0. 第一个用户进程诞生流程

可能会简单写一下对整体流程的理解

### 1. kvm.c补充映射

在`kvm.c`中补充映射了trampoline和KSTACK(0)区域，建立了**trampoline区域**和**第一个进程的内核栈**在**内核页表**上的映射：

```c
m_mappages(kernel_pgtbl,TRAMPOLINE,(uint64)trampoline,PGSIZE, PTE_R | PTE_W | PTE_X); // 映射 trampoline 区域
vm_mappages(kernel_pgtbl,KSTACK(0),(uint64)kstack_pa,PGSIZE, PTE_R | PTE_W); // 映射 KSTACK(0)
```

### 2. proc.c



### 3. trap_user.c

本次实验完成了`trap_user_return`和`trap_user_handler`两个用户态陷阱处理的核心函数。

用户态陷阱处理结构（A-B-C-D）：

```text
user_vector-->trap_user_handler-->trap_user_return-->user_return
```

其中`user_vector`和`user_return`的代码在`trampoline.S`汇编中

#### trap_user_return

trap_user_return的作用是为进程**从内核态进入用户态前做准备工作**，我们主要完成了以下事情：

- **保存内核态必要信息**到trapframe，并将trapframe地址写入sscratch
- 将`user_vector`设为S-mode的**trap处理入口**
- 通过写入sepc设置**返回用户态后PC指针**
- 通过写入sstatus设置**sret后返回到U-mode**（主要针对proczero第一次进入用户态时，需要手动将S-mode的上一个状态设置为U-mode）
- 最后，**调用`trampoline.S`中的`user_return`**

前两点相当于为进入用户态以后再发生trap做准备，第三第四点相当于为正确地返回用户态的正确位置做准备，最后一点就是调用user_return，再在该汇编中切换回用户页表、恢复用户态寄存器，真正回到用户态。

> 要注意的是trap_user_return中的地址设置都要使用**正确的虚拟地址**！在`trap_user_return()`函数运行时使用的是**内核页表**，在`trampoline.S`中的`user_return`刚开始时，通过`csrw satp, a1`指令切换到了**用户页表**。所以在调用`user_return`切换回用户态时，要使用**内核页表下的虚拟地址**；在设置`user_vector`用户态trap处理入口时要使用**用户页表下的虚拟地址**（因为是在用户态trap，已启用用户页表），不过由于`TRAMPOLINE`在内核页表中和用户页表中都有映射，在切换前后都可使用。也不可以直接使用`(uint64)user_vector`和`(uint64)user_return`这种链接时的**物理/恒等映射地址**，否则会因页表未映射而 crash！

```c
// user_vector地址——trap处理入口
// uint64 user_vector_addr = (uint64)user_vector; 错误！
uint64 user_vector_addr = (uint64)TRAMPOLINE+(uint64)(user_vector-trampoline); //正确√（用户页表下的虚拟地址）
w_stvec(user_vector_addr);
// user_return地址——切换回用户态
// uint64 user_return_addr = (uint64)user_return; 错误！
uint64 user_return_addr = (uint64)TRAMPOLINE+(uint64)(user_return-trampoline);// 正确√（内核页表下的虚拟地址）
((void (*)(trapframe_t*, uint64))user_return_addr)((trapframe_t*)TRAPFRAME, user_satp);
```

#### trap_user_handler

`trap_user_handler`的实现和上个实验实现的`trap_kernel_handler`类似，主要有以下两点新实现的不同：

- 进入`trap_user_handler`后会重写trap入口, 将它设为`trap_kernel_handler`，方便处理“**已经在内核里的中断**”

  ```c
  w_stvec((uint64)kernel_vector);
  ```


- 多处理了**系统调用**（`trap_id`=8），虽然系统调用属于异常，但是返回时要通过修改epc来设置 **PC=PC+4**，表示继续执行系统调用执行后面的指令，而不是重复再执行系统调用

  ```c
  case 8: // Environment call from U-mode (ecall)
  {
      uint64 num = tf->a7; // 系统调用号
      switch (num) {
          case SYS_helloworld: // 第一个用户进程的系统调用：打印信息
              printf("proczero: hello world!\n");
              break;
      }
      tf->user_to_kern_epc += 4; // PC = PC + 4
      break;
  }
  ```

## 测试与修复

修复：

1. kmv映射有问题
2. 没有短暂关闭 S 态中断

遇到问题：“窗口期”里的 S 态中断打断，导致把 S 态的 sepc（0x8000...）错误写进了 tf->user_to_kern_epc。随后 trap_user_return 用这个内核地址当作用户返回 PC，自然不会得到两次 ecall

因此需要在 trap_user_return 中：在把 stvec 切到 user_vector 之前关闭 S 态中断。

3. 无法进入trap_user_handler

正确完成2之后，发现无法进入 trap_user_handler（因为未修复2之前，rap 直接落在 kernel_vector，因此可以进入 trap_kernel_handler，但无法成功输出hello world）

（修复2之后，CPU 的 trap 路径从直接走 kernel_vector 变成走 trampoline（user_vector -> 保存到 TRAPFRAME -> 切回 kernel_vector -> 调 trap_user_handler），而这个链条中出现了问题，导致无法进入trap_user_handler）

最终发现是因为trap_user_return中需要传入的是trapframe的虚拟地址！！！

成功输出两次hello world！

为了测试中断，在timer_interrupt_handler中添加打印tick，成功！

![alt text](pictures/01.png)

## 思考与总结

- 关于**地址可见性与页表切换**

  - 在本次实验中，我对虚拟内存系统的核心机制——**地址可见性（Address Visibility）与页表切换（Page Table Switching）** 有了深刻的认识。特别是在实现用户态与内核态之间的上下文切换过程中，我意识到：

    > **物理地址、内核页表映射的虚拟地址、用户页表映射的虚拟地址三者并不等价，它们的可访问性完全依赖于当前激活的页表（即 `satp` 寄存器的状态）**。 

  - 另外，要注意在调用 `pmem_alloc()` 分配内存时，返回的是一个可在**当前地址空间中**安全访问的地址，并不是永远等于物理地址！

- 

