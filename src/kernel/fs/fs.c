#include "mod.h"

super_block_t sb; /* 超级块 */

/* 基于superblock输出磁盘布局信息 (for debug) */
static void sb_print()
{
	printf("\ndisk layout information:\n");
	printf("1. super block:  block[0]\n");
	printf("2. inode bitmap: block[%d - %d]\n", sb.inode_bitmap_firstblock,
		sb.inode_bitmap_firstblock + sb.inode_bitmap_blocks - 1);
	printf("3. inode region: block[%d - %d]\n", sb.inode_firstblock,
		sb.inode_firstblock + sb.inode_blocks - 1);
	printf("4. data bitmap:  block[%d - %d]\n", sb.data_bitmap_firstblock,
		sb.data_bitmap_firstblock + sb.data_bitmap_blocks - 1);
	printf("5. data region:  block[%d - %d]\n", sb.data_firstblock,
		sb.data_firstblock + sb.data_blocks - 1);
	printf("block size = %d Byte, total size = %d MB, total inode = %d\n\n", sb.block_size,
		(int)((unsigned long long)(sb.total_blocks) * sb.block_size / 1024 / 1024), sb.total_inodes);
}

/* 文件系统初始化 */
void fs_init()
{
	// 初始化缓冲系统
	buffer_init();
	// 初始化inode缓存与锁
	inode_init();

	// 读取超级块
	buffer_t *b = buffer_get(FS_SB_BLOCK);
	// 将缓冲区内容拷贝到内存中的sb
	memmove(&sb, b->data, sizeof(super_block_t));
	// 归还缓冲（不修改，不需要写回）
	buffer_put(b);

	// 打印布局信息
	sb_print();

	/* fs_init in fs.c */

	printf("============= test begin =============\n\n");

	inode_t *rooti, *ip_1, *ip_2;
	
	rooti = inode_get(ROOT_INODE);
	inode_lock(rooti);
	inode_print(rooti, "root");
	inode_unlock(rooti);

	/* 第一次查看bitmap */
	bitmap_print(false);

	ip_1 = inode_create(INODE_TYPE_DIR, INODE_MAJOR_DEFAULT, INODE_MINOR_DEFAULT);
	ip_2 = inode_create(INODE_TYPE_DATA, INODE_MAJOR_DEFAULT, INODE_MINOR_DEFAULT);
	inode_lock(ip_1);
	inode_lock(ip_2);
	inode_dup(ip_2);

	inode_print(ip_1, "dir");
	inode_print(ip_2, "data");
	
	/* 第二次查看bitmap */
	bitmap_print(false);

	ip_1->disk_info.nlink = 0;
	ip_2->disk_info.nlink = 0;
	inode_unlock(ip_1);
	inode_unlock(ip_2);
	inode_put(ip_1);
	inode_put(ip_2);

	/* 第三次查看bitmap */
	bitmap_print(false);

	inode_put(ip_2);
	
	/* 第四次查看bitmap */
	bitmap_print(false);

	printf("============= test end =============\n\n");

	while(1);
}