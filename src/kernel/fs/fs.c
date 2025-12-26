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

	// // 测试1: inode的访问 + 创建 + 删除
	// printf("============= test begin =============\n\n");

	// inode_t *rooti, *ip_1, *ip_2;
	
	// rooti = inode_get(ROOT_INODE);
	// inode_lock(rooti);
	// inode_print(rooti, "root");
	// inode_unlock(rooti);

	// /* 第一次查看bitmap */
	// bitmap_print(false);

	// ip_1 = inode_create(INODE_TYPE_DIR, INODE_MAJOR_DEFAULT, INODE_MINOR_DEFAULT);
	// ip_2 = inode_create(INODE_TYPE_DATA, INODE_MAJOR_DEFAULT, INODE_MINOR_DEFAULT);
	// inode_lock(ip_1);
	// inode_lock(ip_2);
	// inode_dup(ip_2);

	// inode_print(ip_1, "dir");
	// inode_print(ip_2, "data");
	
	// /* 第二次查看bitmap */
	// bitmap_print(false);

	// ip_1->disk_info.nlink = 0;
	// ip_2->disk_info.nlink = 0;
	// inode_unlock(ip_1);
	// inode_unlock(ip_2);
	// inode_put(ip_1);
	// inode_put(ip_2);

	// /* 第三次查看bitmap */
	// bitmap_print(false);

	// inode_put(ip_2);
	
	// /* 第四次查看bitmap */
	// bitmap_print(false);

	// printf("============= test end =============\n\n");

	// while(1);

	
	// // 测试2: 写入和读取inode管理的数据
	// printf("============= test begin =============\n\n");

	// inode_t *ip_1, *ip_2;
	// uint32 len, cut_len;

	// // ================== 修改 1: 将大内存申请提前 ==================
    // // 必须在进行任何文件系统操作(buffer操作)之前申请，才能保证物理地址连续
    // // 并且由于我们实现的 pmem_alloc 是从高地址向低地址分配内存，
	// // 所以测试代码还兼容了 pmem 分配器可能的两种方向（地址递增或递减）
    
    // char *pages[5];
    // char *big_src; 
    // char big_dst[9];
    // big_dst[8] = 0;

    // // 1. 申请 5 个页面
    // for (int i = 0; i < 5; i++) {
    //     pages[i] = pmem_alloc(true);
    // }

    // // 2. 检测分配方向
    // bool ascending = (pages[1] == pages[0] + PGSIZE);
    // bool descending = (pages[1] == pages[0] - PGSIZE);

    // if (!ascending && !descending) {
    //     // 如果既不是递增也不是递减，说明内存有碎片，无法进行大块连续内存测试
    //     panic("contiguous fail! Memory is fragmented/not contiguous.");
    // }

    // // 3. 验证所有页面是否连续
    // for (int i = 1; i < 5; i++) {
    //     if (ascending)
    //         assert(pages[i] == pages[i-1] + PGSIZE, "contiguous fail! (gap in ascending)");
    //     else
    //         assert(pages[i] == pages[i-1] - PGSIZE, "contiguous fail! (gap in descending)");
    // }

    // // 4. 确定 big_src 基地址（必须是这块连续内存的最低地址，以便数组索引）
    // if (ascending) {
    //     big_src = pages[0];
    // } else {
    //     big_src = pages[4]; // 递减分配时，最后一个申请的页面地址最低
    // }

    // // 5. 填充数据
    // for (uint32 i = 0; i < 5 * (PGSIZE / 8); i++)
    //     for (uint32 j = 0; j < 8; j++)
    //         big_src[i * 8 + j] = 'A' + j;
    // // ==========================================================


	// /* 小批量读写测试 */

	// int small_src[10], small_dst[10];
	// for (int i = 0; i < 10; i++)
	// 	small_src[i] = i;
	
	// ip_1 = inode_create(INODE_TYPE_DATA, INODE_MAJOR_DEFAULT, INODE_MINOR_DEFAULT);
	// inode_lock(ip_1);
	// inode_print(ip_1, "small_data");

	// printf("writing data...\n\n");
	// cut_len = 10 * sizeof(int);
	// for (uint32 offset = 0; offset < 400 * cut_len; offset += cut_len) {
	// 	len = inode_write_data(ip_1, offset, cut_len, small_src, false);
	// 	assert(len == cut_len, "write fail 1!");
	// }
	// inode_print(ip_1, "small_data");

	// len = inode_read_data(ip_1, 120 * cut_len + 4, cut_len, small_dst, false);
	// assert(len == cut_len, "read fail 1!");
	// printf("read data:");
	// for (int i = 0; i < 10; i++)
	// 	printf(" %d", small_dst[i]);
	// printf("\n\n");

	// ip_1->disk_info.nlink = 0;
	// inode_unlock(ip_1);
	// inode_put(ip_1);

	// /* 大批量读写测试 */

	// // char *big_src, big_dst[9];
	// // big_dst[8] = 0;

	// // /* 申请五个连续物理页面 (初始化阶段, 通常来说能拿到连续的) */
	// // 由于 Buffer Cache 打乱物理内存顺序，所以其实不能保证拿到连续的物理页面！
	// // big_src = pmem_alloc(true);
	// // assert(pmem_alloc(true) == big_src + PGSIZE, "contiguous fail!");
	// // assert(pmem_alloc(true) == big_src + PGSIZE * 2, "contiguous fail!");
	// // assert(pmem_alloc(true) == big_src + PGSIZE * 3, "contiguous fail!");
	// // assert(pmem_alloc(true) == big_src + PGSIZE * 4, "contiguous fail!");

	// // for (uint32 i = 0; i < 5 * (PGSIZE / 8); i++)
	// // 	for (uint32 j = 0; j < 8; j++)
	// // 		big_src[i * 8 + j] = 'A' + j;

	// ip_2 = inode_create(INODE_TYPE_DATA, INODE_MAJOR_DEFAULT, INODE_MINOR_DEFAULT);
	// inode_lock(ip_2);
	// inode_print(ip_2, "big_data");

	// printf("writing data...\n\n");
	// cut_len = PGSIZE * 4 + 1110;

	// // ================== 修改 2: 减小测试规模 ==================
    // // 原来的 10000 次循环会写入约 175MB 数据，导致内存耗尽
    // // 改为 100 次循环，足以验证多级索引逻辑
	// for (uint32 offset = 0; offset < cut_len * 100; offset += cut_len)
	// {
	// 	len = inode_write_data(ip_2, offset, cut_len, big_src, false);
	// 	assert(len == cut_len, "write fail 2!");
	// }
	// inode_print(ip_2, "big_data");

	// // ================== 修改 3: 修正变量名 ip_1 -> ip_2 ==================
    // // 这里应该读取 ip_2 的数据，ip_1 已经被释放了
	// // len = inode_read_data(ip_1, cut_len * 10000 - 8, 8, big_dst, false);
	// len = inode_read_data(ip_2, cut_len * 100 - 8, 8, big_dst, false);
	// assert(len == 8, "read fail 2");
	// printf("read data: %s\n", big_dst);


	// ip_2->disk_info.nlink = 0;
	// inode_unlock(ip_2);
	// inode_put(ip_2);

	// pmem_free((uint64)big_src, true);
	// pmem_free((uint64)big_src + PGSIZE, true);
	// pmem_free((uint64)big_src + PGSIZE * 2, true);
	// pmem_free((uint64)big_src + PGSIZE * 3, true);
	// pmem_free((uint64)big_src + PGSIZE * 4, true);


	// printf("============= test end =============\n");

	// while(1);

	// // 测试3: 目录项的增加、删除、查找操作
	// printf("============= test begin =============\n\n");

	// inode_t *rooti, *ip_1, *ip_2, *ip_3;
	// uint32 inode_num_1, inode_num_2, inode_num_3;
	// uint32 len, cutlen, offset;
	// char tmp[10];

	// tmp[9] = 0;
	// cutlen = 9;
	// rooti = inode_get(ROOT_INODE);

	// /* 搜索预置的dentry */

	// inode_lock(rooti);
	// inode_num_1 = dentry_search(rooti, "ABCD.txt");
	// inode_num_2 = dentry_search(rooti, "abcd.txt");
	// inode_num_3 = dentry_search(rooti, ".");
	// if (inode_num_1 == INVALID_INODE_NUM || 
	// 	inode_num_2 == INVALID_INODE_NUM || 
	// 	inode_num_3 == INVALID_INODE_NUM) {
	// 	panic("invalid inode num!");
	// }
	// dentry_print(rooti);
	// inode_unlock(rooti);

	// ip_1 = inode_get(inode_num_1);
	// inode_lock(ip_1);
	// ip_2 = inode_get(inode_num_2);
	// inode_lock(ip_2);
	// ip_3 = inode_get(inode_num_3);
	// inode_lock(ip_3);

	// inode_print(ip_1, "ABCD.txt");
	// inode_print(ip_2, "abcd.txt");
	// inode_print(ip_3, "root");

	// len = inode_read_data(ip_1, 0, cutlen, tmp, false);
	// assert(len == cutlen, "read fail 1!");
	// printf("\nread data: %s\n", tmp);

	// len = inode_read_data(ip_2, 0, cutlen, tmp, false);
	// assert(len == cutlen, "read fail 2!");
	// printf("read data: %s\n\n", tmp);

	// inode_unlock(ip_1);
	// inode_unlock(ip_2);
	// inode_unlock(ip_3);
	// inode_put(ip_1);
	// inode_put(ip_2);
	// inode_put(ip_3);

	// /* 创建和删除dentry */
	// inode_lock(rooti);

	// ip_1 = inode_create(INODE_TYPE_DIR, INODE_MAJOR_DEFAULT, INODE_MINOR_DEFAULT);	
	// offset = dentry_create(rooti, ip_1->inode_num, "new_dir");
	// inode_num_1 = dentry_search(rooti, "new_dir");
	// printf("new dentry offset = %d\n", offset);
	// printf("new dentry inode_num = %d\n\n", inode_num_1);
	
	// dentry_print(rooti);

	// inode_num_2 = dentry_delete(rooti, "new_dir");
	// assert(inode_num_1 == inode_num_2, "inode num is not equal!");

	// dentry_print(rooti);

	// inode_unlock(rooti);
	// inode_put(rooti);

	// printf("============= test end =============\n");

	// while(1);

	// 测试4: 文件路径的解析
	printf("============= test begin =============\n\n");

	inode_t *rooti, *ip_1, *ip_2, *ip_3, *ip_4, *ip_5;
	
	/* 准备测试环境 */

	rooti = inode_get(ROOT_INODE);
	ip_1 = inode_create(INODE_TYPE_DIR, INODE_MAJOR_DEFAULT, INODE_MINOR_DEFAULT);
	ip_2 = inode_create(INODE_TYPE_DIR, INODE_MAJOR_DEFAULT, INODE_MINOR_DEFAULT);
	ip_3 = inode_create(INODE_TYPE_DATA, INODE_MAJOR_DEFAULT, INODE_MINOR_DEFAULT);
	
	inode_lock(rooti);
	inode_lock(ip_1);
	inode_lock(ip_2);
	inode_lock(ip_3);

	if (dentry_create(rooti, ip_1->inode_num, "AABBC") == -1)
		panic("dentry_create fail 1!");
	if (dentry_create(ip_1, ip_2->inode_num, "aaabb") == -1)
		panic("dentry_create fail 2!");
	if (dentry_create(ip_2, ip_3->inode_num, "file.txt") == -1)
		panic("dentry_create fail 3!");

	char tmp1[] = "This is file context!";
	char tmp2[32];
	inode_write_data(ip_3, 0, sizeof(tmp1), tmp1, false);

	inode_rw(rooti, true);
	inode_rw(ip_1, true);
	inode_rw(ip_2, true);
	// ================== 修改 : 缺少写回元数据 ==================
	// 没有将 ip_3 的元数据写回磁盘了！
	inode_rw(ip_3, true);

	inode_unlock(rooti);
	inode_unlock(ip_1);
	inode_unlock(ip_2);
	inode_unlock(ip_3);
	inode_put(rooti);
	inode_put(ip_1);
	inode_put(ip_2);
	inode_put(ip_3);

	char *path = "///AABBC///aaabb/file.txt";
	char name[MAXLEN_FILENAME];

	ip_4 = path_to_inode(path);
	if (ip_4 == NULL)
		panic("invalid ip_4");

	ip_5 = path_to_parent_inode(path, name);
	if (ip_5 == NULL)
		panic("invalid ip_5");
	
	printf("get a name = %s\n\n", name);

	inode_lock(ip_4);
	inode_lock(ip_5);

	inode_print(ip_4, "file.txt");
	inode_print(ip_5, "aaabb");

	inode_read_data(ip_4, 0, 32, tmp2, false);
	printf("read data: %s\n\n", tmp2);

	inode_unlock(ip_4);
	inode_unlock(ip_5);
	inode_put(ip_4);
	inode_put(ip_5);

	printf("============= test end =============\n");

// 测试5: 硬链接(Hard Link)与引用计数测试
    printf("============= test begin =============\n\n");

    inode_t *rooti_5, *file_orig, *file_link, *check_inode;
    char content[] = "Shared Content via Hard Link";
    char read_buf[32];

	printf("before test:");
	bitmap_print(false);
    rooti_5 = inode_get(ROOT_INODE);
    
    // 1. 创建原始文件 "original.txt"
    inode_lock(rooti_5);
    file_orig = inode_create(INODE_TYPE_DATA, INODE_MAJOR_DEFAULT, INODE_MINOR_DEFAULT);
	printf("creating 'original.txt'...\n");
    dentry_create(rooti_5, file_orig->inode_num, "original.txt");
    bitmap_print(false);

    // 写入数据
    inode_lock(file_orig);
    inode_write_data(file_orig, 0, sizeof(content), content, false);
    printf("Created 'original.txt' with inode %d, nlink = %d\n", file_orig->inode_num, file_orig->disk_info.nlink);
    inode_unlock(file_orig);
    inode_unlock(rooti_5);

    // 2. 创建硬链接 "linked.txt" 指向同一个 inode
    inode_lock(rooti_5);
    // 增加 inode 的链接数
    inode_lock(file_orig);
    file_orig->disk_info.nlink++;
    inode_rw(file_orig, true); // 写回磁盘更新 nlink
    inode_unlock(file_orig);

    // 在目录中创建新条目指向原 inode 号
    dentry_create(rooti_5, file_orig->inode_num, "linked.txt");
    printf("Created hard link 'linked.txt' pointing to inode %d\n", file_orig->inode_num);
    inode_unlock(rooti_5);

    // 3. 通过新路径读取数据验证
    file_link = path_to_inode("/linked.txt");
    assert(file_link != NULL, "link lookup failed");
    assert(file_link->inode_num == file_orig->inode_num, "inode mismatch");
    
    inode_lock(file_link);
    printf("Inode %d nlink is now: %d\n", file_link->inode_num, file_link->disk_info.nlink);
    assert(file_link->disk_info.nlink == 2, "nlink should be 2");
    
    inode_read_data(file_link, 0, sizeof(content), read_buf, false);
    printf("Read from 'linked.txt': %s\n", read_buf);
    inode_unlock(file_link);

    // 4. 删除原始路径 "original.txt"
    inode_lock(rooti_5);
    dentry_delete(rooti_5, "original.txt");
    
    // 减少 nlink
    inode_lock(file_orig);
    file_orig->disk_info.nlink--;
    inode_rw(file_orig, true);
    printf("Deleted 'original.txt', inode %d nlink is now: %d\n", file_orig->inode_num, file_orig->disk_info.nlink);
    inode_unlock(file_orig);
    inode_unlock(rooti_5);

    // 5. 验证文件仍然可以通过 "linked.txt" 访问
    check_inode = path_to_inode("/linked.txt");
    assert(check_inode != NULL, "file should still exist");
    
    inode_lock(check_inode);
    memset(read_buf, 0, sizeof(read_buf));
    inode_read_data(check_inode, 0, sizeof(content), read_buf, false);
    printf("Read from 'linked.txt' after original deleted: %s\n", read_buf);
    inode_unlock(check_inode);

    // 6. 删除最后一个链接 "linked.txt" 并验证回收
    printf("\nDeleting last link 'linked.txt'...\n");
    
    inode_lock(rooti_5);
    dentry_delete(rooti_5, "linked.txt"); // 删除目录项
    
    // 获取 check_inode 的锁 (check_inode 指向 linked.txt 对应的 inode)
    inode_lock(check_inode); 
    check_inode->disk_info.nlink--; // nlink 1 -> 0
    inode_rw(check_inode, true);    // 写回磁盘，持久化 nlink=0
    
    printf("Deleted 'linked.txt', inode %d nlink is now: %d\n", check_inode->inode_num, check_inode->disk_info.nlink);
    inode_unlock(check_inode);
    inode_unlock(rooti_5);

    // 应触发物理回收
    inode_put(file_orig); 
    inode_put(file_link);
    
    // 此时 check_inode 的 ref 应该是 1 (因为上面 path_to_inode 获取了一次)
    // 这次 put 应该触发真正的回收 (free_data_blocks + bitmap_free_inode)
    printf("Triggering final inode_put (should recycle resources)...\n");
    inode_put(check_inode); 

    // 7. [验证] 验证文件彻底消失
    inode_t *gone_inode = path_to_inode("/linked.txt");
    assert(gone_inode == NULL, "File should be gone");
    
    // 打印位图确认 inode 3 对应的位变回了 0
    bitmap_print(false); 

    // 注意：check_inode 等指针已经被 put 了，清理阶段不需要再 put
    inode_put(rooti_5);
    
    printf("============= test end =============\n");

}