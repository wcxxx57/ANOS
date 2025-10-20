# LAB-3: 中断异常初步

在lab-3中，我们实现了初级的中断和异常处理机制，重点完成了**串口中断**和**时钟中断**的支持。

我们的分工如下：

**丁熙妍**：完成了**时钟中断**部分，包括`timer.c`和`main.c`，以及对应的实验文档。

**吴晨曦**：完成了**串口中断**部分，包括`trap_kernel.c`和`start.c`，以及对应的实验文档。

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
？（之后修改）
支持时钟中断，实现系统定时与“心跳”机制。
支持串口中断，实现外设输入的中断驱动。
新增 PLIC（平台级中断控制器）相关代码，使操作系统能够响应 UART 外设中断。
完善了 S-mode/M-mode 的中断异常处理流程，统一汇编入口和 C 语言分发。


## 测试用例

遇到的问题：

![error1](pictures/error1.png)

![error2](pictures/error2.png)

# 修复方法
在trap/mod.h中添加对lock/mod.h的引用

### 时钟滴答测试

一开始将`timer_interrupt_enable()`函数放在了`trap_kernel_handler()`函数中`switch`语句的`S-mode timer interrupt`分支内，导致并没有正确启用时钟中断。将其移到`S-mode software interrupt`后，就成功触发了时钟中断。

这是因为查看`trap.S`文件可以发现：M-mode中断通过设置SSIP触发的是S-mode的软件中断，而不是S-mode的时钟中断。因此，需要在处理S-mode软件中断中调用`timer_interrupt_handler()`，其内部会调用`timer_update()` 并清SSIP，这样才能正确启用时钟中断。

![dida1](pictures/dida1.png)

成功通过时钟滴答测试！

### 时钟快慢测试

通过修改`type.h`中`INTERVAL`的值，具体我尝试了原始的1000000(100ms)以及修改后的100000(10ms)和10000000(1s)，直观感受到了时钟滴答的快慢变化。

![dida2](pictures/dida2.png)
