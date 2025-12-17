# LAB-7: 文件系统 之 磁盘管理

经过之前的实验，我们已经拥有了一个支持多进程调度的内核。在 Lab-7 中，我们将目光投向了**持久化存储**，围绕磁盘管理构建文件系统的基础设施，建立从**底层驱动**到**上层资源管理**的完整链路。

**吴晨曦**：完成了 **缓冲系统（Buffer）** 的实现，文件系统的初始化逻辑，以及对应的系统调用和README文档。

**丁熙妍**：完成了 **位图管理（Bitmap）** 的实现，内核页表与设备映射的适配，磁盘中断的响应与处理，以及对应的系统调用和README文档。

## 代码组织结构

```
ANOS
├── LICENSE        开源协议
├── .vscode        配置了可视化调试环境
├── registers.xml  配置了可视化调试环境
├── .gdbinit.tmp-riscv xv6自带的调试配置
├── common.mk      Makefile中一些工具链的定义
├── Makefile       编译运行整个项目 (CHANGE)
├── kernel.ld      定义了内核程序在链接时的布局
├── pictures       README使用的图片目录 (CHANGE, 日常更新)
├── README.md      实验报告 (CHANGE, 日常更新)
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
    │   │   ├── sleeplock.c
    │   │   ├── method.h
    │   │   ├── mod.h
    │   │   └── type.h
    │   ├── lib    常用库
    │   │   ├── cpu.c
    │   │   ├── print.c
    │   │   ├── uart.c
    │   │   ├── utils.c
    │   │   ├── method.h
    │   │   ├── mod.h
    │   │   └── type.h
    │   ├── mem    内存模块
    │   │   ├── pmem.c
    │   │   ├── kvm.c (本实验补充, 内核页表增加磁盘相关映射 + vm_getpte处理pgtbl为NULL的情况)
    │   │   ├── uvm.c
    │   │   ├── mmap.c
    │   │   ├── method.h
    │   │   ├── mod.h
    │   │   └── type.h
    │   ├── trap   陷阱模块
    │   │   ├── plic.c (本实验补充, 增加磁盘中断相关支持)
    │   │   ├── timer.c
    │   │   ├── trap_kernel.c (本实验补充, 在外设处理函数中识别和响应磁盘中断)
    │   │   ├── trap_user.c
    │   │   ├── trap.S
    │   │   ├── trampoline.S
    │   │   ├── method.h
    │   │   ├── mod.h (CHANGE, include 文件系统模块)
    │   │   └── type.h
    │   ├── proc   进程模块
    │   │   ├── proc.c (本实验补充, 在proc_return中调用文件系统初始化函数)
    │   │   ├── swtch.S
    │   │   ├── method.h
    │   │   ├── mod.h (CHANGE, include 文件系统模块)
    │   │   └── type.h
    │   ├── syscall 系统调用模块
    │   │   ├── syscall.c (本实验补充, 新增系统调用)
    │   │   ├── sysfunc.c (本实验补充, 新增系统调用)
    │   │   ├── method.h (CHANGE, 新增系统调用)
    │   │   ├── mod.h (CHANGE, include文件系统模块)
    │   │   └── type.h (CHANGE, 新增系统调用)
    │   ├── fs     文件系统模块
    │   │   ├── bitmap.c (本实验完成, bitmap相关操作)
    │   │   ├── buf.c (本实验完成, 内存中的block缓冲区管理)
    │   │   ├── fs.c (本实验完成, 文件系统相关)
    │   │   ├── virtio.c (NEW, 虚拟磁盘的驱动)
    │   │   ├── method.h (NEW)
    │   │   ├── mod.h (NEW)
    │   │   └── type.h (NEW)
    │   └── main.c (CHANGE, 增加virtio_init)
    ├── mkfs       磁盘映像初始化
    │   ├── mkfs.c (NEW)
    │   └── mkfs.h (NEW)
    └── user       用户程序
        ├── initcode.c (CHANGE, 日常更新)
        ├── sys.h
        ├── syscall_arch.h
        └── syscall_num.h (CHANGE, 日常更新)
```

相对于上一个实验, 本实验主要增加了以下功能：

- 实现了**虚拟磁盘驱动 (VirtIO)**，打通了内核与磁盘的通信渠道。
- 实现了**缓冲系统(Buffer)**，构建了内存与磁盘间的数据交换桥梁。
- 实现了**位图管理(Bitmap)**，为文件系统的元数据和数据块分配提供支持。
- 完善了**硬件抽象层**，包括 **MMIO 内存映射**的适配以及 **PLIC 磁盘中断**的响应与分发。

## 具体实现

### 1. buf.c

### 2. fs.c

### 3. bitmap.c

在文件系统中，我们需要高效地管理成千上万个 inode 和 data block 的分配状态。**位图 (Bitmap)** 是最经典且节省空间的解决方案：使用 **1 个 bit** 来标记 1 个资源的占用情况（0 代表空闲，1 代表占用）。

为了形象地展示 Bitmap 的工作原理，我绘制了下图：

```
磁盘布局: [ Superblock | Bitmap Block 0 | ... | Data Block 0 | Data Block 1 | ... ]
                            |
                            v
Bitmap Block 0 内部:
Byte 0: [ 1 1 0 1 0 0 0 0 ] 
          ^ ^ ^
          | | +-- 对应 Data Block 2 （ 0 表示空闲，可分配 ）
          | +---- 对应 Data Block 1 （ 1 表示已占用 ）
          +------ 对应 Data Block 0 （ 1 表示已占用 ）
```

在 `bitmap.c` 中，我实现了**从底层位操作到上层资源管理**的完整逻辑，具体包括对 inode 和 data block 的**查找、分配与释放**：

#### 底层原子操作：查找与置位

底层的核心函数是 `bitmap_search_and_set`，它负责在一个具体的 bitmap block 中寻找空闲位：

- 首先调用 `buffer_get` 读取 bitmap block 到内存。
- 通过双重循环遍历 block 中的每个字节和每个位：
   - 如果一个**字节**的值为 `0xFF`（即 8 位全被占用），且该字节完全在有效范围内，则直接跳过。
   - 否则，**逐位**检查该字节，寻找第一个为 `0` 的空闲位。
   - 找到后，通过**位运算** `|= (1U << j)` 将其置 1，并立即调用 `buffer_write` 将修改**写回磁盘**，最后释放 buffer。
- 需要注意的是：由于磁盘的总资源数往往不是 `BLOCK_SIZE * 8` 的整数倍，**最后一个 bitmap block 通常是不满的**，因此函数接收 `valid_count` 参数，在检查位时进行**边界判断**，避免防止分配出超出磁盘容量的无效资源。
  

#### 上层资源管理：分配与释放

基于底层的位操作，我们实现了对 inode 和 data block 的分配与释放：`bitmap_alloc_block/inode` 和 `bitmap_free_block/inode`。

- **资源分配**：
  - 这是一个**跨 Block 的搜索过程**，函数根据 Superblock 中记录的 bitmap 起始位置和块数，循环遍历每一个 bitmap block。
  - 遍历过程中，动态计算每个 block 的 `valid_count`（**只有最后一个 block 需要特殊截断**），并调用**底层函数 `bitmap_search_and_set`** 进行查找。
  - 一旦找到空闲位，通过公式 `全局ID = 起始ID + 已扫描位数 + 块内偏移` 计算出最终的 `block_num` 或 `inode_num`并返回。

- **资源释放**：
  - 释放过程相对简单，就是**资源分配的逆过程**。
  - 根据传入的全局 ID，计算出它属于第几个 bitmap block 以及在该 block 中的第几个 bit。
  - 调用底层函数 **`bitmap_clear`**，通过**位运算** `&= ~(1U << bit_offset)` 将目标位清零，并调用 `buffer_write` 将修改**写回磁盘**，完成释放。


## 测试与修复

## 总结与思考
