# LAB-1: 机器启动

lab1的核心目标是**机器启动**，具体来讲，我们主要完成了

1. 通过BIOS→_entry→start→main**进入main函数**
2. 借助MMIO读写串口设备（UART） 寄存器以完成**字符打印**，并借助stdarg.h的va_list类型实现**printf格式化输出**
3. 通过编写自旋锁函数并在printf中使用以实现双核的**有序输出**

## 1. 代码组织结构

```
ECNU-OSLAB-2025-TASK  
├── LICENSE        开源协议  
├── .vscode        配置了可视化调试环境
├── registers.xml  配置了可视化调试环境  
├── Makefile       编译运行整个项目  
├── common.mk      Makefile中一些工具链的定义  
├── kernel.ld      定义了内核程序在链接时的布局  
├── pictures       README使用的图片目录  
├── README.md      实验指导书  
└── src            源码
    └── kernel     内核源码
        ├── arch   RISC-V相关
        │   ├── method.h  
        │   ├── mod.h  
        │   └── type.h  
        ├── boot   机器启动
        │   ├── entry.S  
        │   └── start.c (本实验完成)  
        ├── lock   锁机制
        │   ├── spinlock.c (本实验完成)  
        │   ├── method.h  
        │   ├── mod.h  
        │   └── type.h  
        ├── lib    常用库
        │   ├── cpu.c  
        │   ├── print.c (本实验完成)  
        │   ├── uart.c  
        │   ├── method.h  
        │   ├── mod.h  
        │   └── type.h  
        └── main.c (本实验完成)  
```

## 2. 进入main函数

依据**kernel.ld**中的`ENTRY(_entry)`和`. = 0x80000000`两行代码，QEMU启动时，CPU的PC会被设置为`0x800000000`，即`_entry`标签的地址。`_entry`标签被定义在**entry.S**汇编文件中，entry.S文件为函数栈顶指针设置了合适的值，并且**调用了start函数**`call start`。

然后程序进入**start.c**文件中的`start()`函数，在`start()`函数中，程序需要**进入main函数**并且从**M-mode进入S-mode**，为了从更高特权级的M-mode降级到S-mode，需要**利用异常返回机制手动构造一个异常返回环境**

- 首先通过`w_mstatus(status)`修改`m_status`寄存器，假装在发生“（假）异常”前的环境状态为S-mode
- 然后通过`mepc`设定返回原先环境后的下一条指令，即**设定为main函数**
- 最后使用`mret`回到之前的环境状态（即**我们假装的S-mode**）

总体start.c的文件结构如下：

```
start():
    w_satp(0);           // 关闭分页
    w_tp(hartid);        // 保存核心ID（只有m-mode可以访问ID）
    w_mstatus(MPP=S);    // 设置返回模式为 S-mode
    w_mepc(main);        // 设置返回地址为 main
    mret                 // 跳转到 S-mode 并执行 main()
    ↓
main()                   // 操作系统主逻辑开始！
```

然后我们就成功通过BIOS→_entry→start()→main进入到了main函数！

## 3. 串口与格式化输出

为了证明我们进入了main函数，我们需要让main函数打印点东西。

### 3.1 串口输出

为了将字符打印到终端，需要让**驱动程序**控制**串口设备（UART）**，控制串口设备也就是**读写串口设备的寄存器**。CPU 无法直接访问这些物理寄存器，然而**MMIO（Memory-Mapped I/O）技术**能够把设备的寄存器“映射”到一段特定的内存地址空间上 ，于是驱动程序可以**像读写普通内存一样读写外部设备的寄存器**，从而实现了对外部串口设备的控制。

```c
// type.h头文件中的相关宏定义
#define Reg(reg) ((volatile unsigned char *)(UART_BASE + reg))
#define WriteReg(reg, v) (*(Reg(reg)) = (v))
```

- `Reg`函数的作用是把“**寄存器编号**”转换成该寄存器在**内存中的实际地址（指针）**
- `WriteReg`的作用是**将值 `v` 写入 UART（或其他外设）的某个寄存器 `reg`**

具体的**串口输入/输出函数**在**uart.c中**实现，使用了以上两个宏定义函数。

```c
//uart.c中实现的串口输出接口
void uart_init(void); //uart 初始化
void uart_putc_sync(int c);// 单个字符输出
int uart_getc_sync(void);// 单个字符输入
void uart_intr(void);// 中断处理(键盘输入->屏幕输出)
```

在这些串口输出接口的基础上，加上对可变参数的解析可以实现下面的`printf`格式化输出

### 3.2 格式化输出

在UART接口的基础上，使用**stdarg.h**和**va_list**类型来逐个获得printf函数的**可变参数**

stdarg.h是编译器自带的库，其中的va_list类型有以下使用函数：

```c
#include <stdarg.h>
va_list ap;        // 参数指针
va_start(ap, fmt); // 指向第一个可变参数
va_arg(ap, type);  // 获取下一个类型为 type 的参数
va_end(ap);        // 清理（可选）
```

在编写**printf**函数时，首先声明一个`va_list ap`作为**参数指针**，用于**遍历可变参数**，再声明一个`const char *p`作为**字符指针**，用于**遍历格式字符串**，识别到`%`就从可变参数中取出对应值并打印。printf函数的核心代码及注释如下：

```c
for(p = fmt;*p;p++){
    //若不是格式化字符, 直接输出
    if(*p!='%'){
        uart_putc_sync(*p);
        continue;
    }
    //若识别到格式化字符
    p++;
    switch(*p){
        case 'd':
            printint(va_arg(ap,int),10,1);
            break;
        case 'p':
            printint(va_arg(ap,uint32),16,0);
            break;
        case 'x':
            printptr(va_arg(ap,uint64));
            break;
        case 'c':
            c=va_arg(ap,int); // char会被提升为int
            uart_putc_sync(c);
            break;
        case 's':
            s=va_arg(ap,char*);
            if(s==0)
                s="(null)";//若为空字符串
            while(*s!='\0'){
                uart_putc_sync(*s);
                s++;
            }
            break;
        default:
            uart_putc_sync('%');
            uart_putc_sync(*p);
            break;
    }
}

```

然后在main中写出以下程序来验证`printf`函数的格式化输出情况：

```c
printf("Hello!This is our OS Kernel!This Kernel is written by %d people:%s and %s.\n \
Let's try and print %c,%p and %x.\n \
We made it!It's amazing!!\n", \
2, "dxy", "wcx", 'A', 0x12345678U, 0x1234567890abcdefULL);
```

运行结果如下图所示：

![乱序](picture/luan.png)

可以看出，程序成功**打印出了格式化内容**，但是出现了**混乱交错的现象**，因此还需要添加一种同步机制来协调共享资源的有序使用。

## 4.自旋锁

### 4.1 自旋锁接口的实现

自旋锁接口的实现在spinlock.c文件中

### 4.2 在printf函数中使用自旋锁

在printf函数中使用自旋锁以实现有序输出，即需要在开始解析并输出格式字符串前先“**上锁**”，然后在解析输出完后再“**解锁**”。

```c
spinlock_acquire(&print_lk);// 上锁

for(p = fmt;*p;p++){//解析并输出格式字符串
    switch(*p){
        case 'b':
            ...
        case ...
            。。。
    }
}

spinlock_release(&print_lk);//解锁
```

使用完自旋锁后，针对3.2中同样的测试用例，打印结果如下：

![有序](picture/youxu.png)

成功完成了两个核的有序输出！

## 5. 课后实验

这里有两个额外的实验帮助你理解锁的用处 

### 5.1 并行加法

``` 
    volatile static int started = 0;

    volatile static int sum = 0;

    int main()
    {
        int cpuid = r_tp();
        if(cpuid == 0) {
            print_init();
            printf("cpu %d is booting!\n", cpuid);        
            __sync_synchronize();
            started = 1;
            for(int i = 0; i < 1000000; i++)
                sum++;
            printf("cpu %d report: sum = %d\n", cpuid, sum);
        } else {
            while(started == 0);
            __sync_synchronize();
            printf("cpu %d is booting!\n", cpuid);
            for(int i = 0; i < 1000000; i++)
                sum++;
            printf("cpu %d report: sum = %d\n", cpuid, sum);
        }   
        while (1);    
    }  
```

在 **main.c** 中测试上述代码，很明显，我们的预期是后report的cpu应该告诉我们 `sum = 2000000`

但是实际结果可能是这样的  

```
cpu 0 is booting!
cpu 1 is booting!
cpu 0 report: sum = 1128497
cpu 1 report: sum = 1143332
```

考虑如何使用锁进行修正，修正后的输出可能是这样的  

```
cpu 0 is booting!
cpu 1 is booting!
cpu 0 report: sum = 1996573
cpu 1 report: sum = 2000000
```

简单说明上锁和解锁的位置不同会有什么影响（tips: 锁的粒度粗细）

### 5.2 并行输出  

尝试去掉`printf`里的锁，参考4.1的实验思路，设计测试方法使得`printf`的输出出现交错的情况  

4.1和4.2的测试代码和实验结果可以附在你的README中, 但是不要体现在你的代码里