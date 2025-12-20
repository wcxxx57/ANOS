// // test-1: read superblock
// #include "sys.h"

// int main()
// {
// 	syscall(SYS_print_str, "hello, world!\n");
// 	while(1);
// }

// // test-2: bitmap
// #include "sys.h"

// #define NUM 20
// #define N_BUFFER 8

// int main()
// {
// 	unsigned int block_num[NUM];
// 	unsigned int inode_num[NUM];

// 	for (int i = 0; i < NUM; i++)
// 		block_num[i] = syscall(SYS_alloc_block);

// 	syscall(SYS_flush_buffer, 8);
// 	syscall(SYS_show_bitmap, 0);

// 	for (int i = 0; i < NUM; i+=2)
// 		syscall(SYS_free_block, block_num[i]);
	
// 	syscall(SYS_flush_buffer, 8);
// 	syscall(SYS_show_bitmap, 0);

// 	for (int i = 1; i < NUM; i+=2)
// 		syscall(SYS_free_block, block_num[i]);

// 	syscall(SYS_flush_buffer, 8);
// 	syscall(SYS_show_bitmap, 0);

// 	for (int i = 0; i < NUM; i++)
// 		inode_num[i] = syscall(SYS_alloc_inode);

// 	syscall(SYS_flush_buffer, 8);
// 	syscall(SYS_show_bitmap, 1);

// 	for (int i = 0; i < NUM; i++)
// 		syscall(SYS_free_inode, inode_num[i]);

// 	syscall(SYS_flush_buffer, 8);
// 	syscall(SYS_show_bitmap, 1);

// 	while(1);
// }


// test-3: buffer
// #include "sys.h"

// #define PGSIZE 4096
// #define N_BUFFER 8
// #define BLOCK_BASE 5000

// int main()
// {
// 	char data[PGSIZE], tmp[PGSIZE];
// 	unsigned long long buffer[N_BUFFER];

// 	/*-------------一阶段测试: READ WRITE------------- */

// 	/* 准备字符串"ABCDEFGH" */
// 	for (int i = 0; i < 8; i++)
// 		data[i] = 'A' + i;
// 	data[8] = '\n';
// 	data[9] = '\0';

// 	/* 查看此时的buffer_cache状态 */
// 	syscall(SYS_print_str, "\nstate-1 ");
// 	syscall(SYS_show_buffer);

// 	/* 向BLOCK_BASE写入字符 */
// 	buffer[0] = syscall(SYS_get_block, BLOCK_BASE);
// 	syscall(SYS_write_block, buffer[0], data);
// 	syscall(SYS_put_block, buffer[0]);

// 	/* 查看此时的buffer_cache状态 */
// 	syscall(SYS_print_str, "\nstate-2 ");
// 	syscall(SYS_show_buffer);

// 	/* 清空内存副本, 确保后面从磁盘中重新读取 */
// 	syscall(SYS_flush_buffer, N_BUFFER);

// 	/* 读取BLOCK_BASE*/
// 	buffer[0] = syscall(SYS_get_block, BLOCK_BASE);
// 	syscall(SYS_read_block, buffer[0], tmp);
// 	syscall(SYS_put_block, buffer[0]);

// 	/* 比较写入的字符串和读到的字符串 */
// 	syscall(SYS_print_str, "\n");
// 	syscall(SYS_print_str, "write data: ");
// 	syscall(SYS_print_str, data);
// 	syscall(SYS_print_str, "read data: ");
// 	syscall(SYS_print_str, tmp);

// 	/* 查看此时的buffer_cache状态 */
// 	syscall(SYS_print_str, "\nstate-3 ");
// 	syscall(SYS_show_buffer);

// 	/*-------------二阶段测试: GET PUT FLUSH------------- */
	
// 	/* GET */
// 	buffer[0] = syscall(SYS_get_block, BLOCK_BASE);
// 	buffer[3] = syscall(SYS_get_block, BLOCK_BASE + 3);
// 	buffer[7] = syscall(SYS_get_block, BLOCK_BASE + 7);
// 	buffer[2] = syscall(SYS_get_block, BLOCK_BASE + 2);
// 	buffer[4] = syscall(SYS_get_block, BLOCK_BASE + 4);

// 	/* 查看此时的buffer_cache状态 */
// 	syscall(SYS_print_str, "\nstate-4 ");
// 	syscall(SYS_show_buffer);

// 	/* PUT */
// 	syscall(SYS_put_block, buffer[7]);
// 	syscall(SYS_put_block, buffer[0]);
// 	syscall(SYS_put_block, buffer[4]);

// 	/* 查看此时的buffer_cache状态 */
// 	syscall(SYS_print_str, "\nstate-5 ");
// 	syscall(SYS_show_buffer);

// 	/* FLUSH */
// 	syscall(SYS_flush_buffer, 3);

// 	/* 查看此时的buffer_cache状态 */
// 	syscall(SYS_print_str, "\nstate-6 ");
// 	syscall(SYS_show_buffer);

// 	while(1);
// }

// test-4: Buffer Cache Capacity & LRU Eviction Test
#include "sys.h"

#define N_BUFFER 8
#define TEST_BASE 7000

int main()
{
    unsigned long long bufs[N_BUFFER + 2];
    
    syscall(SYS_print_str, "--- Test-4: Buffer Cache Capacity & LRU Eviction ---\n");

    // 1. 填满缓冲区：获取 8 个不同的块 (7000 ~ 7007)
    syscall(SYS_print_str, "Step 1: Filling buffer cache with 8 blocks...\n");
    for (int i = 0; i < N_BUFFER; i++) {
        bufs[i] = syscall(SYS_get_block, TEST_BASE + i);
    }
    syscall(SYS_show_buffer); // 此时 7000~7007 应该全在 Active 链表

    // 2. 全部归还：使它们进入 Inactive 链表
    syscall(SYS_print_str, "\nStep 2: Putting all blocks to Inactive list...\n");
    for (int i = 0; i < N_BUFFER; i++) {
        syscall(SYS_put_block, bufs[i]);
    }
    // 此时 Inactive 链表头部应该是 7007，尾部（最久未使用）应该是 7000
    syscall(SYS_show_buffer);

    // 3. 触发置换：获取 2 个全新的块 (7008, 7009)
    // 根据 LRU，这会挤掉 Inactive 链表尾部的 7000 和 7001
    syscall(SYS_print_str, "\nStep 3: Getting 2 new blocks (7008, 7009) to force eviction...\n");
    bufs[8] = syscall(SYS_get_block, TEST_BASE + 8);
    bufs[9] = syscall(SYS_get_block, TEST_BASE + 9);
    syscall(SYS_show_buffer);

    // 4. 验证哈希表与置换：重新获取被挤掉的 7000
    // 此时 7000 应该不在缓存中，会重新触发一次磁盘读取并占用一个新的 buffer 节点
    syscall(SYS_print_str, "\nStep 4: Re-getting evicted block 7000...\n");
    bufs[0] = syscall(SYS_get_block, TEST_BASE + 0);
    syscall(SYS_show_buffer);

    syscall(SYS_print_str, "\nTest-4 Over!\n");
    while(1);
}