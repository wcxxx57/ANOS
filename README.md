# LAB-5: 系统调用流程建立 + 用户态虚拟内存管理

在lab-5中，我们继续**完善和发展了第一个用户进程`proczero`**，赋予`proczero`**更强的内存掌控能力**（包括堆、栈、离散映射三个部分）以及**完善的请求服务能力**（建立真正的系统调用流程），具体分为五个小任务。

我们的分工如下：

**丁熙妍**：完成了**任务3**（mmap_region_node 仓库管理）、**任务4**（mmap 与 munmap），以及对应的README文档。

**吴晨曦**：完成了**任务1**（用户态和内核态的数据迁移）、**任务2**（堆的手动管理与栈的自动管理），以及对应的README文档

---

## 代码组织结构

```
ECNU-OSLAB-2025-TASK
├── LICENSE        开源协议
├── .vscode        配置了可视化调试环境
├── registers.xml  配置了可视化调试环境
├── .gdbinit.tmp-riscv xv6自带的调试配置
├── common.mk      Makefile中一些工具链的定义
├── Makefile       编译运行整个项目 (CHANGE, 新增目录syscall)
├── kernel.ld      定义了内核程序在链接时的布局
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
    │   │   ├── cpu.c
    │   │   ├── print.c
    │   │   ├── uart.c
    │   │   ├── utils.c
    │   │   ├── method.h
    │   │   ├── mod.h
    │   │   └── type.h
    │   ├── mem    内存模块
    │   │   ├── pmem.c
    │   │   ├── kvm.c
    │   │   ├── uvm.c (本实验完成, 用户态虚拟内存管理主体)
    │   │   ├── mmap.c (本实验完成, mmap节点资源仓库)
    │   │   ├── method.h (CHANGE, 日常更新)
    │   │   ├── mod.h
    │   │   └── type.h (CHANGE, 日常更新)
    │   ├── trap   陷阱模块
    │   │   ├── plic.c
    │   │   ├── timer.c
    │   │   ├── trap_kernel.c
    │   │   ├── trap_user.c (本实验完成, 系统调用处理 + pagefault处理)
    │   │   ├── trap.S
    │   │   ├── trampoline.S
    │   │   ├── method.h
    │   │   ├── mod.h (CHANGE, 日常更新)
    │   │   └── type.h
    │   ├── proc   进程模块
    │   │   ├── proc.c (本实验完成, proczero->mmap初始化)
    │   │   ├── swtch.S
    │   │   ├── method.h
    │   │   ├── mod.h
    │   │   └── type.h (CHANGE, 进程结构体里新增mmap字段)
    │   ├── syscall 系统调用模块
    │   │   ├── syscall.c (NEW, 系统调用通用逻辑)
    │   │   ├── sysfunc.c (本实验完成, 各个系统调用的处理逻辑) 
    │   │   ├── method.h (NEW)
    │   │   ├── mod.h (NEW)
    │   │   └── type.h (NEW)
    │   └── main.c
    └── user       用户程序
        ├── initcode.c (CHANGE, 按照测试需求来设置)
        ├── sys.h
        ├── syscall_arch.h
        └── syscall_num.h (CHANGE, 日常更新)
```

相比于上一个实验，本次实验主要增加了以下功能：

- 实现了**用户态和内核态的数据迁移**：上次的lab4中实现了`sys_helloworld`，让内核成功输出了`"hello world"`，但用户态程序无法向内核程序传递参数，在本次实验中我们借助trapframe中保存的寄存器值实现了用户态和内核态之间的**值传递**，借助物理地址的过渡实现了用户态和内核态之间的**地址传递**。
- 实现了**用户地址空间中堆的手动管理与栈的自动管理**：上次的lab4中栈空间和堆空间被简单地设置为4KB和0KB，本次实验初步实现了动态的堆栈管理，包括**堆的伸缩**和**栈缺页的处理**。
- 完成了
- 完成了
- 完成了

---

## 具体实现

### 1.用户态和内核态的数据迁移

为实现和测试数据迁移，完成了`sys_copyin`、`sys_copyout`、`sys_copyinstr`这三个**临时系统调用**，分别用于**用户态拷贝至内核态**、**内核态拷贝至用户态**、**用户态字符串拷贝到内核态**。调用流程：

```tex
在initcode.c中调用syscall->进入trap_user_handler识别该系统调用，并调用sysfunc.c的对应函数进行处理->在sysfunc.c的对应函数中获取参数，并调用uvm.c中的对应函数进行实际拷贝
```

因此我所做的就是在[trap_user.c](src/kernel/trap/trap_user.c)中的`trap_user_handler`函数的ecall case中又加了这三个系统调用的case，然后在uvm.c和sysfunc.c中实现了对应的处理函数

#### uvm.c

在[uvm.c](src/kernel/mem/uvm.c)的`uvm_copyin`，`uvm_copyout`和`uvm_copyin_str`函数中，我主要完成的事就是**使用`memmove`函数来执行拷贝**，`memmove`函数原型：

```c
void *memmove(void *dest, const void *src, size_t n);
```

不过其中对dest、src以及拷贝长度n这三个参数的处理有以下两个细节：

- **用户地址空间和内核地址空间不匹配**的问题（用户传入的地址空间是基于用户页表的, 但是进入内核后使用的是内核页表)，在进行**地址传递**时，要首先查询用户页表, 找到了虚拟地址对应的**物理地址,** 然后再做数据迁移：

  ```c
  // 获取src对应的PTE和物理地址
  pte_t *pte = vm_getpte(pgtbl, src, false); 
  uint64 pa = PTE_TO_PA(*pte); 
  ```

- 数据传递的**src和dst 不一定是page-aligned**的问题，拷贝时不能直接整页大小的拷贝，可能会**因为不对齐而跨页从而拷贝错误**，于是我首先通过计算页内偏移和剩余可拷贝字节数，**计算实际需要拷贝字节数**，再进行拷贝。

#### sysfunc.c

对于[sysfunc.c](src/kernel/syscall/sysfunc.c)的`sys_copyin`，`sys_copyout`和`sys_copyinstr`这三个函数，我首先利用`arg_uint32`和`arg_uint64`**获取用户态传来的参数**（通过`proc->tf->ax`拿到这些参数），然后**调用上面在uvm.c中实现的对应的`uvm_copyxx`函数进行拷贝**，比如`sys_copyout()`如下，`sys_copyin`和`sys_copyinstr`也类似：

```c
static int kernel_array[5] = {1, 2, 3, 4, 5}; // 内核中的测试数组
uint64 sys_copyout()
{
    uint64 addr;
    arg_uint64(0, &addr); // 获取第0号参数
    uvm_copyout(myproc()->pgtbl, addr, (uint64)kernel_array, 5 * sizeof(int)); // 调用uvm_copyout从内核拷贝到用户态
    return 0;
}
```

#### 测试1：用户态和内核态的数据迁移

测试逻辑:

- 用户读取内核中的数组 (1 2 3 4 5)
- 用户将读到的数组传递给内核, 内核收到后打印出来
- 用户将自己的字符串传递给内核, 内核收到后打印出来

测试代码在[initcode.c](src/user/initcode.c)中的对应注释部分，在sysfunc.c的函数中添加了一些调试信息，测试结果见[`pictures/test1.png`](pictures/test1.png)

### 2.堆的手动管理与栈的自动管理

#### 堆的手动管理

这个功能的实现与任务一的逻辑类似，完成了：

- [sysfunc.c](src/kernel/syscall/sysfunc.c)中`sys_brk`**系统调用函数**，用于实现**用户堆空间的伸缩和查找**
- 对应的**支撑函数**，[uvm.c](src/kernel/mem/uvm.c)中的`uvm_heap_grow`和`uvm_heap_ungrow`函数
- 同时在[trap_user.c](src/kernel/trap/trap_user.c)中的`trap_user_handler`函数中添加一个case以**识别`sys_brk`这个系统调用**。

##### uvm_heap_grow和uvm_heap_ungrow

这是`sys_brk`的两个支撑函数，我实现的核心方法为：

- `uvm_heap_grow`函数对应**用户堆空间增加**，通过一个**for循环**为每个增加的页**分配物理页并映射**：

  ```c
  void *pa = pmem_alloc(false); // 分配物理页
  vm_mappages(pgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W | PTE_U);//映射
  ```

- `uvm_heap_ungrow`函数对应**用户堆空间减少**，通过一个**for循环**把每一个要释放的页**解映射**：

  ```c
  vm_unmappages(pgtbl, va, PGSIZE, true); //解映射
  ```

细节上，考虑到可能出现新/旧**堆顶不page-aligned**的情况，我统一采用按页向上取整的方式进行对齐：

```c
uint64 cur_pages = (cur_heap_top + PGSIZE - 1) / PGSIZE; // 向上取整
uint64 new_pages = (new_top + PGSIZE - 1) / PGSIZE; // 向上取整
```

同时，也进行了**边界检查**：

```c
if (new_pages * PGSIZE > (uint64)MMAP_BEGIN) 
    return (uint64)-1;
```

##### sys_brk

和`sys_copyxx`类似，同样是先通过`arg_uint64`**从用户态获取参数`new_top`(新堆顶)**。

若`new_top=0`则代表**查询当前堆顶**，返回现堆顶`cur`；若`new_top>cur`，则进行**堆增长**，调用`uvm_heap_grow(p->pgtbl, cur, len)`；若`new_top<cur`，则进行**堆收缩**，调用`uvm_heap_ungrow(p->pgtbl, cur, len)`（`new_top=cur`也合并在这个情况）

#### 测试2(1)：堆的手动管理

测试逻辑：

- 传入参数0，**查询栈顶**
- 向上**拓展**9 pages的堆空间
- `new_heap_top=old_heap_top`，保持**不变**
- 向下**收缩**5 pages的堆空间

测试代码在[initcode.c](src/user/initcode.c)中的对应注释部分，在`sys_brk`中增加了一些调试性输出，调用kvm.c中的`vm_print`函数打印页表内容来进行验证。测试结果见[`pictures/test2(1).png`](pictures/test2(1).png)，从打印的页表可以看出，伸缩堆空间的时候，页表映射确实随之改变。

#### 栈的自动管理

用户栈空间是**根据程序运行的需要**逐步分配的，并且扩展后**不会收缩**。当用户读或写一块未分配的地址空间时, 会触发**13号异常(Load Page Fault)** / **15号异常(Store/AMO Page Fault)**，从而通过**处理缺页异常**拓展所需页面。

我首先在`trap_user_handler`里的异常处理中增加了case 13和case 15来识别这两种异常（stval寄存器中放的是page fault的地址）：

```c
case 13:
case 15:
    uint64 old_ustack_npage = p->ustack_npage;
    uint64 ret = uvm_ustack_grow(p->pgtbl, old_ustack_npage, r_stval());
    break;
```

然后实现了[uvm.c](src/kernel/mem/uvm.c)中`uvm_ustack_grow`函数负责的**缺页异常的处理**。

##### uvm_ustack_grow

实现起来和`uvm_heap_grow`类似，核心就是利用`pmem_alloc`**分配新的物理页**，然后利用`vm_mappages`进行**映射**。就是流程上首先需要**判断发生page fault的地址是否是合理的栈扩展地址**：

```c
if (fault_addr >= TRAPFRAME || fault_addr <= MMAP_END) 
    return (uint64)-1; 
```

确认合法性后，再计算计算需要拓展的栈页数并进行**物理页面的申请和映射**，最后还要**更新`proc->ustack_npage`**，同时，依然也有**边界检查**（栈不能越过 `MMAP_END`）：

```c
uint64 max_stack_pages = (TRAPFRAME - (uint64)MMAP_END) / PGSIZE;
if (need_pages > max_stack_pages) 
    return (uint64)-1;
```

#### 测试2(2)：栈的自动管理

测试逻辑：

- 通过定义**非static（确保在栈上）的长数组**，让栈的大小超过4KB，从而测试栈的自动拓展能力

测试代码在[initcode.c](src/user/initcode.c)中的对应注释部分，在`trap_user_handler`中增加了一些调试性输出。测试结果见[`pictures/test2(2).png`](pictures/test2(2).png)，从输出可以看出，确实触发了缺页异常，并且拓展了对应的栈空间。

### 3.mmap_region_node仓库管理

调试：第二股有时候是255-128、127-0交替，但有时候会出现如：255-267-254-265-253-...情况，这是因为`main.c` 中的逻辑仅规定了 **CPU0 将申请到的节点指针存入 `mmap_list[0..127]`**，而 **CPU1 存入 `mmap_list[128..255]`**。然而，`mmap_region_alloc` 本身是从共享的空闲链表中取节点的。由于 CPU0 和 CPU1 是 **并发执行** 申请操作的：
1. 若 CPU1 抢锁速度快，它可能申请到物理索引较小的节点。
2. 释放时，CPU1 归还该较小节点，导致高位索引释放流中混入了低位索引节点。

若希望每次运行结果都是严格的 `255` 与 `127` 开头的交替序列，需在 `main.c` 中强制 **串行化分配过程**，即要求 CPU1 在申请前必须等待 CPU0 完成申请：

``` c
// CPU1 代码段
while (started == 0);
while (over_1 == false); // 等待 CPU0 完成前 128 个节点的分配
// CPU1 再开始分配，此时只能拿到剩余的后 128 个节点
for(int i = N_MMAP / 2; i < N_MMAP; i++)
    mmap_list[i] = mmap_region_alloc();
over_2 = true;
```

但乱序输出时，结果显示仍是将这256个节点全部正确分配和释放了，并且没有重复释放，因此该问题并不影响功能的正确性，仅是输出顺序上的差异。



### 4.mmap 与 munmap

调试：
出现了死循环
![alt text](pics/bug5.png)
发现是因为uvm_mmap 中的合并逻辑破坏了链表结构:仔细看了下 mmap_merge，发现它只负责两个节点的“内容合并”和“内存释放”，但并没有处理链表指针的更新问题，因此须在外部手动更新。
而在 uvm_mmap 中调用 mmap_merge 时，我释放了被合并的节点，但没有更新链表中前一个节点的 next 指针。这导致链表中出现了一个指向“已释放内存”（悬挂指针）的链接。

当 uvm_show_mmaplist 遍历链表时，它会顺着这个悬挂指针跑进错误的内存区域（通常是跑进了空闲节点链表，或者指向了自己），从而导致死循环打印。

修复：
```c
// 先向后合并：若后继节点紧邻则合并，保留node
    if (node->next != NULL && node->begin + node->npages * PGSIZE == node->next->begin) {
        mmap_region_t *next_node = node->next; // 暂存即将被合并的节点
        node->next = next_node->next; // 【关键修复】先从链表中摘除 next_node
        mmap_merge(node, next_node, true); // 然后合并并释放 next_node
    }
```

成功输出!


### 5.页表的复制与销毁





---

## 总结与思考/问题与思考

测试或者做的时候遇到的比较有启发的问题和修复可以写这里

- 逐层封装的设计