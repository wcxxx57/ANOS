#include "mod.h"

// 内核空间和用户空间的可分配物理页分开描述
static alloc_region_t kern_region, user_region;

// 物理内存的初始化
// 本质上就是填写kern_region和user_region, 包括基本数值和空闲链表
void pmem_init(void)
{
    // 初始化kern_region和user_region的锁
    spinlock_init(&kern_region.lk, "kernel_region_lock");
    spinlock_init(&user_region.lk, "user_region_lock");

    // 划分内核和用户内存区域的边界
    uint64 boundary = (uint64)ALLOC_BEGIN + KERN_PAGES * PGSIZE;

    // 初始化kern_region剩余内容
    kern_region.begin = (uint64)ALLOC_BEGIN;
    kern_region.end = boundary;
    kern_region.allocable = 0; // 可分配的空闲页面数
    kern_region.list_head.next = NULL; // 可分配链的链头节点

    // 初始化user_region剩余内容
    user_region.begin = boundary;
    user_region.end = (uint64)ALLOC_END;
    user_region.allocable = 0; 
    user_region.list_head.next = NULL; 

    // 将内核区域的物理页加入空闲链表
    // 相当于把内核区域的物理页都free掉
    for (uint64 p = kern_region.begin; p < kern_region.end; p += PGSIZE)
    {
        pmem_free(p, true);
    }
    // 将用户区域的物理页加入空闲链表
    for (uint64 p = user_region.begin; p < user_region.end; p += PGSIZE)
    {
        pmem_free(p, false);
    }
}

// 尝试返回一个可分配的清零后的物理页
// 失败则panic锁死
void* pmem_alloc(bool in_kernel)
{
    page_node_t *page;

    return page;
}

// 释放一个物理页
// 失败则panic锁死
void pmem_free(uint64 page, bool in_kernel)
{

}
