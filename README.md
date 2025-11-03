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

相比于上一个实验，本次实验主要增加了以下功能：【等会儿改】

- 建立了**陷阱系统**的基本框架
- 完成了外设中断中的**串口中断**处理，使系统能够**输入字符并回显**
- 完成了由M-mode时钟中断和S-mode软件中断共同组成的**时钟中断**处理，使系统能够进行**时钟滴答**输出

---

## 具体实现概述

### 0. 第一个用户进程诞生流程

可能会简单写一下对整体流程的理解

### 1. kvm.c补充映射



### 2. proc.c



### 3. trap_user.c



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

## 思考总结

