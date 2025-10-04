#include "mod.h"

// 内核页表
static pgtbl_t kernel_pgtbl;

// 根据pagetable,找到va对应的pte
// 若设置alloc=true 则在PTE无效时尝试申请一个物理页
// 成功返回PTE, 失败返回NULL
// 提示：使用 VA_TO_VPN + PTE_TO_PA + PA_TO_PTE
pte_t *vm_getpte(pgtbl_t pgtbl, uint64 va, bool alloc)
{
    // va不能超过最大虚拟地址
    if (va >= VA_MAX)
        return NULL;
    
    //从顶级页表开始往下找
    for (int level = 2;level >= 0;level--) {
        // 计算va在当前页表的索引
        int idx = VA_TO_VPN(va, level);

        //获取当前层级页表的pte
        pte_t *pte = &pgtbl[idx];
        
        //检查 PTE 是否有效
        if (*pte & PTE_V) {// PTE 有效
            
            if (level == 0) {//到达最后一层，找到目标 PTE
                return pte;
            }

            // 当前不是叶子层，需要进入下一级页表
            // 检查是否为中间节点（必须R=W=X=0）
            if (PTE_CHECK(*pte)) {
                // 是中间页表节点，获取其指向的物理地址
                uint64 child_pa = PTE_TO_PA(*pte);
                // 转换为内核虚拟地址（假设恒等映射或已映射）
                pgtbl = (pgtbl_t)child_pa;
                // 继续下一层循环
            } else {
                // 错误：非叶子层却有 R/W/X 权限 → 不该出现在中间路径上
                return NULL;
            }
        } else {// PTE 无效（未建立映射）
            if (!alloc) {
                // 不允许分配 → 查找失败
                return NULL;
            }
            // 分配一个新的物理页作为下一级页表
            void *pa = pmem_alloc(true);  // 从内核区域分配一页
            if (!pa) {
                return NULL;// 分配失败
            }

            // 清零新页表（避免垃圾数据）
            memset(pa, 0, PGSIZE);

            // 构造新的 PTE：指向这个新页表
            uint64 child_ppn = PA_TO_PTE((uint64)pa);  // 转成 PTE 格式的 PPN
            *pte = child_ppn | PTE_V;                  // 设置为有效，无 R/W/X（中间节点）

            // 更新当前页表指针为新分配的页表
            pgtbl = (pgtbl_t)pa;
            // 继续进入下一层
        }
    }

    // 逻辑上不会到这里
    panic("vm_getpte: unreachable");
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
        // Step 1: 获取当前虚拟地址对应的 PTE 指针
        //        如果路径不存在，自动创建中间页表
        pte_t *pte = vm_getpte(pgtbl, va, true);
        if (!pte) {
            panic("vm_mappages: cannot create PTE (out of memory?)");
        }

        // Step 2: 检查这个 PTE 是否已经有效（防止重复映射同一个虚拟页）
        if (*pte & PTE_V) {
            panic("vm_mappages: remap an already mapped page");
        }

        // Step 3: 构造新的 PTE
        //         将物理地址 pa 编码为 PPN 字段，并加上权限和 V 标志
        uint64 pte_flags = PA_TO_PTE(pa) | perm | PTE_V;
        *pte = pte_flags;

        // Step 4: 前进到下一页
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
            pmem_free((void *)pa);  // 假设这是用户页
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
            printf(".. .. level-0 pgtbl %d: pa = %p\n", j, pgtbl_2);

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
