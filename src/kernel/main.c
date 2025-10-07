#include "arch/mod.h"
#include "lib/mod.h"
#include "mem/mod.h"

volatile static int started = 0;

volatile static int over_1 = 0, over_2 = 0;

//static int* mem[1024];

/*---------------------------------- 测试代码 --------------------------------*/

void test_mapping_and_unmapping()
{
    // 1. 初始化测试页表
    pte_t* pte;
    pgtbl_t pgtbl = (pgtbl_t)pmem_alloc(true);
    memset(pgtbl, 0, PGSIZE);

    // 2. 准备测试条件
    uint64 va_1 = 0x100000;
    uint64 va_2 = 0x8000;
    uint64 pa_1 = (uint64)pmem_alloc(false);
    uint64 pa_2 = (uint64)pmem_alloc(false);

    // 3. 建立映射
    vm_mappages(pgtbl, va_1, pa_1, PGSIZE, PTE_R | PTE_W);
    vm_mappages(pgtbl, va_2, pa_2, PGSIZE, PTE_R);

    // 4. 验证映射结果
    pte = vm_getpte(pgtbl, va_1, false);
    assert(pte != NULL, "test_mapping_and_unmapping: pte_1 not found");
    assert((*pte & PTE_V) != 0, "test_mapping_and_unmapping: pte_1 not valid");
    assert(PTE_TO_PA(*pte) == pa_1, "test_mapping_and_unmapping: pa_1 mismatch");
    assert((*pte & (PTE_R | PTE_W)) == (PTE_R | PTE_W), "test_mapping_and_unmapping: flag_1 mismatch");

    pte = vm_getpte(pgtbl, va_2, false);
    assert(pte != NULL, "test_mapping_and_unmapping: pte_2 not found");
    assert((*pte & PTE_V) != 0, "test_mapping_and_unmapping: pte_2 not valid");
    assert(PTE_TO_PA(*pte) == pa_2, "test_mapping_and_unmapping: pa_2 mismatch");
    assert((*pte & (PTE_R | PTE_W)) == (PTE_R), "test_mapping_and_unmapping: flag_2 mismatch");

    // 5. 解除映射
    vm_unmappages(pgtbl, va_1, PGSIZE, true);
    vm_unmappages(pgtbl, va_2, PGSIZE, true);

    // 6. 验证解除映射结果
    pte = vm_getpte(pgtbl, va_1, false);
    assert(pte != NULL, "test_mapping_and_unmapping: pte_1 not found");
    assert((*pte & PTE_V) == 0, "test_mapping_and_unmapping: pte_1 still valid");
    pte = vm_getpte(pgtbl, va_2, false);
    assert(pte != NULL, "test_mapping_and_unmapping: pte_2 not found");
    assert((*pte & PTE_V) == 0, "test_mapping_and_unmapping: pte_2 still valid");

    // 7. 由于页表的释放函数还没实现, 作为测试用例可以展示不释放页表空间

    printf("test_mapping_and_unmapping passed!\n");
}

int main()
{
    // int cpuid = r_tp();

    // if(cpuid == 0) {

    //     print_init();
    //     pmem_init();

    //     printf("cpu %d is booting!\n", cpuid);
    //     __sync_synchronize();
    //     started = 1;

    //     for(int i = 0; i < 512; i++) {
    //         mem[i] = pmem_alloc(true);
    //         memset(mem[i], 1, PGSIZE);
    //         printf("mem = %p, data = %d\n", mem[i], mem[i][0]);
    //     }
    //     printf("cpu %d alloc over\n", cpuid);
    //     over_1 = 1;
        
    //     while(over_1 == 0 || over_2 == 0);
        
    //     for(int i = 0; i < 512; i++)
    //         pmem_free((uint64)mem[i], true);
    //     printf("cpu %d free over\n", cpuid);

    // } else {

    //     while(started == 0);
    //     __sync_synchronize();
    //     printf("cpu %d is booting!\n", cpuid);
        
    //     for(int i = 512; i < 1024; i++) {
    //         mem[i] = pmem_alloc(true);
    //         memset(mem[i], 1, PGSIZE);
    //         printf("mem = %p, data = %d\n", mem[i], mem[i][0]);
    //     }
    //     printf("cpu %d alloc over\n", cpuid);
    //     over_2 = 1;

    //     while(over_1 == 0 || over_2 == 0);

    //     for(int i = 512; i < 1024; i++)
    //         pmem_free((uint64)mem[i], true);
    //     printf("cpu %d free over\n", cpuid);        
 
    // }
    // while (1);    

    int cpuid = r_tp();

    if(cpuid == 0) {

        print_init();
        pmem_init();
        kvm_init();
        kvm_inithart();

        printf("cpu %d is booting!\n", cpuid);
        __sync_synchronize();
        // started = 1;

        pgtbl_t test_pgtbl = pmem_alloc(true);
        uint64 mem[5];
        for(int i = 0; i < 5; i++)
            mem[i] = (uint64)pmem_alloc(false);

        printf("\ntest-1\n\n");    
        vm_mappages(test_pgtbl, 0, mem[0], PGSIZE, PTE_R);
        vm_mappages(test_pgtbl, PGSIZE * 10, mem[1], PGSIZE / 2, PTE_R | PTE_W);
        vm_mappages(test_pgtbl, PGSIZE * 512, mem[2], PGSIZE - 1, PTE_R | PTE_X);
        vm_mappages(test_pgtbl, PGSIZE * 512 * 512, mem[2], PGSIZE, PTE_R | PTE_X);
        vm_mappages(test_pgtbl, VA_MAX - PGSIZE, mem[4], PGSIZE, PTE_W);
        vm_print(test_pgtbl);

        printf("\ntest-2\n\n");    
        vm_mappages(test_pgtbl, 0, mem[0], PGSIZE, PTE_W);
        vm_unmappages(test_pgtbl, PGSIZE * 10, PGSIZE, true);
        vm_unmappages(test_pgtbl, PGSIZE * 512, PGSIZE, true);
        vm_print(test_pgtbl);

    } else {

        while(started == 0);
        __sync_synchronize();
        printf("cpu %d is booting!\n", cpuid);
         
    }

    test_mapping_and_unmapping();
    
    while (1); 
}