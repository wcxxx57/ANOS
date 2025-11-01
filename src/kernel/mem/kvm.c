#include "mod.h"

// 内核页表
static pgtbl_t kernel_pgtbl;

// 根据pagetable,找到va对应的pte
// 若设置alloc=true 则在PTE无效时尝试申请一个物理页
// 成功返回PTE, 失败返回NULL
// 提示：使用 VA_TO_VPN + PTE_TO_PA + PA_TO_PTE
pte_t *vm_getpte(pgtbl_t pgtbl, uint64 va, bool alloc)
{
    // 检查地址合法性
    if (va >= VA_MAX)
        return NULL;

    pgtbl_t curr = pgtbl;  // 当前正在查看的页表

    // level=2 和 level=1（中间层）
    for (int level = 2; level > 0; level--) {
        int idx = VA_TO_VPN(va, level);   // 获取当前层级的索引
        pte_t *pte = &curr[idx];          // 当前层级的 PTE

        if (*pte & PTE_V) {               // PTE 有效
            if (PTE_CHECK(*pte)) {        // 是中间节点（R/W/X=0）
                uint64 child_pa = PTE_TO_PA(*pte);
                curr = (pgtbl_t)child_pa; // 跳转到下一级页表
            } else {
                return NULL; // 非法：中间节点设置了 R/W/X
            }
        } else {                          // PTE 无效
            if (!alloc)                   // 不允许分配 → 失败
                return NULL;

            void *pa = pmem_alloc(true);  // 分配一页作为下一级页表
            if (!pa)
                return NULL;
            memset(pa, 0, PGSIZE);        // 清零

            uint64 child_ppn = PA_TO_PTE((uint64)pa);
            *pte = child_ppn | PTE_V;     // 设置 PTE 指向新页表

            curr = (pgtbl_t)pa;           // 更新当前页表为新分配的
        }
    }

    // 到达这里说明已经到了 level=0
    // 返回 level-0 的 PTE 指针（不管它是否有效）
    int idx = VA_TO_VPN(va, 0);
    return &curr[idx];
}

// 在pgtbl中建立 [va, va + len) -> [pa, pa + len) 的映射
// 本质是找到va在页表对应位置的pte并修改它
// 检查: va pa 应当是 page-aligned, len(字节数) > 0, va + len <= VA_MAX
// 注意: perm 应该如何使用
void vm_mappages(pgtbl_t pgtbl, uint64 va, uint64 pa, uint64 len, int perm)
{
    //参数检查
    // 1. 长度必须大于 0
    if (len == 0) {
        panic("vm_mappages: len is zero");
    }

    // 2. va 和 pa 必须页对齐
    if (va % PGSIZE != 0 || pa % PGSIZE != 0) {
        panic("vm_mappages: va or pa not page-aligned");
    }

    // 3. 不能越界
    if (va + len > VA_MAX) {
        panic("vm_mappages: virtual address overflow");
    }


    //逐页映射
    uint64 end = va + len;  // 结束虚拟地址

    while (va < end) {
        // Step 1: 获取当前虚拟地址对应的 PTE 指针(如果路径不存在，自动创建中间页表)
        pte_t *pte = vm_getpte(pgtbl, va, true);
        if (!pte) {
            panic("vm_mappages: cannot create PTE (out of memory?)");
        }

        // Step 2: 修改 PTE
        //         将物理地址 pa 编码为 PPN 字段，并加上权限和 V 标志
        *pte = PA_TO_PTE(pa) | perm | PTE_V;

        // Step 3: 前进到下一页
        va += PGSIZE;
        pa += PGSIZE;
    }

}

// 解除pgtbl中[va, va+len)区域的映射
// 如果freeit == true则释放对应物理页, 默认是用户的物理页
void vm_unmappages(pgtbl_t pgtbl, uint64 va, uint64 len, bool freeit)
{
    //参数检查
    // 1. 长度必须大于 0
    if (len == 0) {
        panic("vm_unmappages: len is zero");    
    // 2. va 必须页对齐
    } else if (va % PGSIZE != 0) {
        panic("vm_unmappages: va not page-aligned");
    // 3. 不能越界
    } else if (va + len > VA_MAX) {
        panic("vm_unmappages: virtual address overflow");   
    }

    //逐页解除映射
    uint64 end = va + len;  // 结束虚拟地址
    while (va < end) {
        // Step 1: 获取当前虚拟地址对应的 PTE 指针(不允许自动创建)
        pte_t *pte = vm_getpte(pgtbl, va, false);
        if (!pte || !(*pte & PTE_V)) {
            panic("vm_unmappages: unmap a not mapped page");
        }

        // Step 2: 如果需要，释放对应的物理页
        if (freeit) {
            uint64 pa = PTE_TO_PA(*pte);
            
            // 释放 默认是用户的物理页
            pmem_free(pa, false);
        }

        // Step 3: 将 PTE 标记为无效（解除映射）
        *pte = 0;

        // Step 4: 前进到下一页
        va += PGSIZE;
    }
}

// 完成UART、CLINT、PLIC、内核代码区、内核数据区、可分配区域的页表映射
// 相当于部分填充kernel_pgtbl
void kvm_init()
{
    // Step 1: 分配根页表（第2级页表）
    kernel_pgtbl = (pgtbl_t)pmem_alloc(true);  // 从内核区域分配一页
    if (!kernel_pgtbl) {
        panic("kvm_init: cannot allocate root page table");
    }
    memset(kernel_pgtbl, 0, PGSIZE);  // 清零

    // === Step 2: 获取内核代码和数据的范围 ===
    uint64 text_start = KERNEL_BASE;           
    uint64 data_end   = (uint64)ALLOC_BEGIN;   // 数据段结束位置
    uint64 size = data_end - text_start;
    uint64 map_size = (size + PGSIZE - 1) & ~(PGSIZE - 1);
    

    // === Step 3: 恒等映射内核代码和数据区===
    vm_mappages(kernel_pgtbl,
                text_start,
                text_start,           // va = pa
                map_size,
                PTE_R | PTE_W | PTE_X);  // 可读写执行

    // === Step 4: 映射设备（UART/CLINT/PLIC）===
    vm_mappages(kernel_pgtbl,
                UART_BASE,
                UART_BASE,
                PGSIZE,
                PTE_R | PTE_W);  // 不可执行

    vm_mappages(kernel_pgtbl,
                CLINT_BASE,
                CLINT_BASE,
                0x10000,  // 64KB
                PTE_R | PTE_W); // 不可执行

    vm_mappages(kernel_pgtbl,
                PLIC_BASE,
                PLIC_BASE,
                0x4000000,  // ~64MB
                PTE_R | PTE_W); // 不可执行

    // === Step 5: 映射可用内存区域 [ALLOC_BEGIN, ALLOC_END) ===
    uint64 phy_pool_begin = (uint64)ALLOC_BEGIN;
    uint64 phy_pool_end   = (uint64)ALLOC_END;
    uint64 phy_pool_sz    = phy_pool_end - phy_pool_begin;

    vm_mappages(kernel_pgtbl,
                phy_pool_begin,
                phy_pool_begin,
                phy_pool_sz,
                PTE_R | PTE_W);  // 不可执行

    // === Step 6: 映射 trampoline 区域 ===
    extern char trampoline[];  // 链接符号：trampoline 代码所在“实际物理页”的地址
    vm_mappages(kernel_pgtbl,
                (uint64)TRAMPOLINE,  // va
                (uint64)trampoline,  // pa
                PGSIZE,
                PTE_R | PTE_X);  // 可读可执行
    
    // === Step 7: 映射每个进程的内核栈 (例如，procid=0) ===
    // KSTACK(procid) 是高虚拟地址（每个栈相隔 2 页：1 页栈 + 1 页 guard）
    // 这里给 proczero (procid=0) 分配 1 页物理栈，并映射到 KSTACK(0)
    // 应该是在用户空间中分配的物理页！！！不是内核空间！！！
    uint64 procid = 0;  // 假设是进程 ID 为 0 的进程
    void *kstack_pa = pmem_alloc(false);
    if (!kstack_pa) panic("kvm_init: alloc kstack failed");
    memset(kstack_pa, 0, PGSIZE);

    vm_mappages(kernel_pgtbl,
                (uint64)KSTACK(procid),   // VA
                (uint64)kstack_pa,        // PA
                PGSIZE*2,                   
                PTE_R | PTE_W);           // 只读写

    // // 检查 kernel 页表里 TRAMPOLINE 的 pte
    // pte_t *kpte = vm_getpte(kernel_pgtbl, TRAMPOLINE, false);
    // printf("DEBUG kvm: kernel pte for TRAMPOLINE=%p\n", kpte);
    // if (kpte) printf("DEBUG kvm: pte=0x%x pa=0x%x flags=0x%x\n", (uint64)*kpte, PTE_TO_PA((uint64)*kpte), (int)PTE_FLAGS((uint64)*kpte));
}

// 每个CPU都需要调用, 从不使用页表切换到使用内核页表
// 切换后需要刷新TLB里面的缓存
void kvm_inithart()
{
    w_satp(MAKE_SATP(kernel_pgtbl));
    sfence_vma();
}

// 输出页表内容(for debug)
void vm_print(pgtbl_t pgtbl)
{
    // 顶级页表，次级页表，低级页表
    pgtbl_t pgtbl_2 = pgtbl, pgtbl_1 = NULL, pgtbl_0 = NULL;
    pte_t pte;

    printf("level-2 pgtbl: pa = %p\n", pgtbl_2);
    for (int i = 0; i < PGSIZE / sizeof(pte_t); i++)
    {
        pte = pgtbl_2[i];
        if (!((pte)&PTE_V))
            continue;
        assert(PTE_CHECK(pte), "vm_print: pte check fail (1)");
        pgtbl_1 = (pgtbl_t)PTE_TO_PA(pte);
        printf(".. level-1 pgtbl %d: pa = %p\n", i, pgtbl_1);

        for (int j = 0; j < PGSIZE / sizeof(pte_t); j++)
        {
            pte = pgtbl_1[j];
            if (!((pte)&PTE_V))
                continue;
            assert(PTE_CHECK(pte), "vm_print: pte check fail (2)");
            pgtbl_0 = (pgtbl_t)PTE_TO_PA(pte);
            printf(".. .. level-0 pgtbl %d: pa = %p\n", j, pgtbl_0);

            for (int k = 0; k < PGSIZE / sizeof(pte_t); k++)
            {
                pte = pgtbl_0[k];
                if (!((pte)&PTE_V))
                    continue;
                assert(!PTE_CHECK(pte), "vm_print: pte check fail (3)");
                printf(".. .. .. physical page %d: pa = %p flags = %d\n", k, (uint64)PTE_TO_PA(pte), (int)PTE_FLAGS(pte));
            }
        }
    }
}
