# LAB-3: 中断异常初步

在lab-3中，我们实现了初级的中断和异常处理机制，重点完成了**串口中断**和**时钟中断**的支持。

我们的分工如下：

**丁熙妍**：完成了**时钟中断**部分，包括`timer.c`和`main.c`，以及对应的实验文档。

**吴晨曦**：完成了**串口中断**部分，包括`trap_kernel.c`和`start.c`，以及对应的实验文档

## 代码组织结构

```
ANOS
├── LICENSE        开源协议
├── .vscode        配置了可视化调试环境
├── registers.xml  配置了可视化调试环境
├── common.mk      Makefile中一些工具链的定义
├── Makefile       编译运行整个项目
├── kernel.ld      定义了内核程序在链接时的布局
├── pictures       README使用的图片目录 (CHANGE, 日常更新)
├── README.md      实验报告 (CHANGE, 日常更新)
└── src            源码
    └── kernel     内核源码
        ├── arch   RISC-V相关
        │   ├── method.h
        │   ├── mod.h
        │   └── type.h (CHANGE, 新增一些RISC-V中断相关宏定义)
        ├── boot   机器启动
        │   ├── entry.S
        │   └── start.c (本实验完成, 在M-mode多做一些事情再进入S-mode)
        ├── lock   锁机制
        │   ├── spinlock.c
        │   ├─
        │   ├── mod.h
        │   └── type.h
        ├── lib    常用库
        │   ├── cpu.c
        │   ├── print.c
        │   ├── uart.c
        │   ├── utils.c
        │   ├── method.h
        │   ├── mod.h
        │   └── type.h
        ├── mem    内存模块
        │   ├── pmem.c
        │   ├── kvm.c
        │   ├── method.h
        │   ├── mod.h
        │   └── type.h
        ├── trap   陷阱模块
        │   ├── plic.c (NEW, 使得os可以响应UART外设中断)
        │   ├── timer.c (本实验完成, 时钟中断和计时器相关操作)
        │   ├── trap_kernel.c (本实验完成, 内核态trap处理的核心逻辑)
        │   ├── trap.S (NEW, 是trap处理的汇编入口)
        │   ├── method.h (CHANGE)
        │   ├── mod.h
        │   └── type.h (CHANGE)
        └── main.c (本实验完成, 更多的初始化)
```

相比于上一个实验，本次实验主要增加了以下功能：

- 建立了**陷阱系统**的基本框架
- 完成了外设中断中的**串口中断**处理，使系统能够**输入字符并回显**
- 完成了由M-mode时钟中断和S-mode软件中断共同组成的**时钟中断**处理，使系统能够进行**时钟滴答**输出

## 具体实现概述

### 1.对trap.S的理解

我们对陷阱系统的初步实现**首先从理解trap.S开始**。trap.S中包含陷阱系统处理的核心流程，有`kernel_vector`和`timer_vector`两部分。

在kernel_trap.c中初始化trap核心的时候（`trap_kernel_inithart()`函数）将`kernel_vector`标签写入`stvec`寄存器作为**中断处理晨旭入口地址**（进入s-mode）；类似地，我们在start.c中初始化时钟中断的时候（`clockinit()`函数）也将`timer_vector`标签写入`mtvec`寄存器作为**时钟中断处理程序入口**（进入m-mode）。

为方便理解，我们将trap.S的执行流程整理为如下图所示：

![trap.S流程图](pictures/trapS.png)

其中有两点比较重要：

- 引发s-mode软件中断的`li a1, 2`，`csrw sip, a1`和`w_sip(r_sip() & ~2)`互为逆过程，将SSIP bit置为1后**还要清除SSIP bit**才能彻底结束软件中断，否则会一直持续陷入s-mode软件中断导致系统死机（**原先因为没有意识到这一点，在后面的串口输入测试中de了很久的bug**！！）（**虽然再后来又发现是个不必要的bug**！！！）
- `mscratch`寄存器指向一个用于**临时保存寄存器状态**的**专用内存区域**，是每个CPU独立的，在后面的start.c中也要用到！

### 2.start.c

在start.c中实现了两件事：

- **委托S-mode处理所有trap**：通过写入`medeleg`和`mideleg`寄存器来分别异常和中断，并使能（enable）三种中断
- **时钟中断初始化**：首先为当前CPU分配并初始化mscratch保存区域，然后设置 mscratch寄存器指向保存区域，



### 3.trap_kernel.c



### 4.timer.c







## 测试与修复

### 测试遇到的问题：

![error1](pictures/error1.png)

![error2](pictures/error2.png)

### 修复方法

在trap/mod.h中添加对lock/mod.h的引用

#### 1.时钟滴答测试

一开始将`timer_interrupt_enable()`函数放在了`trap_kernel_handler()`函数中`switch`语句的`S-mode timer interrupt`分支内，导致并没有正确启用时钟中断。将其移到`S-mode software interrupt`后，就成功触发了时钟中断。

这是因为查看`trap.S`文件可以发现：M-mode中断通过设置SSIP触发的是S-mode的软件中断，而不是S-mode的时钟中断。因此，需要在处理S-mode软件中断中调用`timer_interrupt_handler()`，其内部会调用`timer_update()` 并清SSIP，这样才能正确启用时钟中断。

![dida1](pictures/dida1.png)

成功通过时钟滴答测试！

#### 2.时钟快慢测试

通过修改`type.h`中`INTERVAL`的值，具体我尝试了原始的1000000(100ms)以及修改后的100000(10ms)和10000000(1s)，直观感受到了时钟滴答的快慢变化。

![dida2](pictures/dida2.png)

## 经验总结与思考

- 硬件寄存器（控制状态寄存器）