#include "mod.h"

extern super_block_t sb;

/* 内存中的inode资源集合 */
static inode_t inode_cache[N_INODE];
static spinlock_t lk_inode_cache;

/* inode_cache初始化 */
void inode_init()
{

} 

/*--------------------关于inode->index的增删查操作-----------------*/

/* 
	供free_data_blocks使用
	递归删除inode->index中的一个元素
	返回删除过程中是否遇到空的block_num (文件末尾)
*/
static bool __free_data_blocks(uint32 block_num, uint32 level)
{
	if (block_num == 0)
		return true; // 遇到空的block_num，说明是文件末尾

	// level 0：数据块，直接释放
	if (level == 0) {
		bitmap_free_block(block_num);
		return false; // 不是文件末尾
	}

	// level > 0：索引块，需要递归释放其指向的子块
	buffer_t *buf = buffer_get(block_num);
	uint32 *index_list = (uint32 *)buf->data;
	// 一个 block 中包含 BLOCK_SIZE / 4 个 uint32 索引
	uint32 n_index = BLOCK_SIZE / sizeof(uint32);
	bool meet_empty = false;

	// 递归释放子块
	for (int i = 0; i < n_index; i++) {
		if (__free_data_blocks(index_list[i], level - 1)) {
			meet_empty = true;
			break; // 遇到空的block_num，停止释放
		}
	}

	buffer_put(buf);
	// 释放当前索引块本身
	bitmap_free_block(block_num);
	return meet_empty;
}

/* 
	释放inode管理的blocks
*/
static void free_data_blocks(uint32 *inode_index)
{
	unsigned int i;
	bool meet_empty = false;

	/* step-1: 释放直接映射的block */
	for (i = 0; i < INODE_INDEX_1; i++)
	{
		meet_empty = __free_data_blocks(inode_index[i], 0);
		if (meet_empty) return;
	}

	/* step-2: 释放一级间接映射的block */
	for (; i < INODE_INDEX_2; i++)
	{
		meet_empty = __free_data_blocks(inode_index[i], 1);
		if (meet_empty) return;
	}

	/* step-3: 释放二级间接映射的block */
	for (; i < INODE_INDEX_3; i++)
	{
		meet_empty = __free_data_blocks(inode_index[i], 2);
		if (meet_empty) return;		
	}

	panic("free_data_blocks: impossible!");
}

/*
	获取inode第logical_block_num个block的物理序号block_num
	调用者保证输入的logical_block_num只有两种情况:
	1. 属于已经分配的区域 (返回block_num)
	2. 将已经分配出去的区域往外扩展1个block (申请block并返回block_num) 
	成功返回block_num, 失败返回-1
*/
static uint32 locate_or_add_block(uint32 *inode_index, uint32 logical_block_num)
{
	uint32 block_num;
	uint32 *index_table;
	buffer_t *buf1 = NULL, *buf2 = NULL;
    uint32 result = -1;
	// 每个 block 能存放的索引数量 (1024)
    uint32 index_per_block = BLOCK_SIZE / sizeof(uint32);

	// 1. 直接映射范围 (0 ~ 9)
    if (logical_block_num < INODE_INDEX_1){
		block_num = inode_index[logical_block_num];
		if (block_num == 0) {
			// 需要分配新的block
			block_num = bitmap_alloc_block();
			if (block_num == (uint32)-1)
				return -1; // 分配失败
			inode_index[logical_block_num] = block_num;
			// 新分配的块需要清零
			buffer_t *new_buf = buffer_get(block_num);
			memset(new_buf->data, 0, BLOCK_SIZE);
			buffer_write(new_buf);
			buffer_put(new_buf);
		}
		return block_num;
	}

	// 2. 一级间接映射范围 (10 ~ 10+1024*2-1)
	// 每个一级索引块控制 1024 个数据块 + 2 个一级索引槽位
	if (logical_block_num < INODE_BLOCK_INDEX_2){
		// 计算相对于一级映射起始位置的偏移
        uint32 rel_idx = logical_block_num - INODE_BLOCK_INDEX_1;
		uint32 l1_idx = rel_idx / index_per_block; // 第几个一级索引块
        uint32 l1_off = rel_idx % index_per_block; // 块内偏移

		// 检查一级索引块是否存在
		uint32 l1_block = inode_index[INODE_INDEX_1 + l1_idx];
		if (l1_block == 0) {
			// 需要分配一级索引块
			l1_block = bitmap_alloc_block();
			if (l1_block == (uint32)-1)
				return -1; // 分配失败
			inode_index[INODE_INDEX_1 + l1_idx] = l1_block;
			// 新分配的块需要清零
			buffer_t *new_buf = buffer_get(l1_block);
			memset(new_buf->data, 0, BLOCK_SIZE);
			buffer_write(new_buf);
			buffer_put(new_buf);
		}

		// 读取一级索引块
		buf1 = buffer_get(l1_block);
		index_table = (uint32 *)buf1->data;
		block_num = index_table[l1_off];
		if (block_num == 0) {
			// 需要分配新的数据块
			block_num = bitmap_alloc_block();
			if (block_num == (uint32)-1) {
				result = -1; // 分配失败
				buffer_put(buf1);
				return result;
			}
			index_table[l1_off] = block_num;
			buffer_write(buf1); // 更新索引块

			// 新分配的块需要清零
			buffer_t *new_buf = buffer_get(block_num);
			memset(new_buf->data, 0, BLOCK_SIZE);
			buffer_write(new_buf);
			buffer_put(new_buf);
		}

		result = block_num;
		buffer_put(buf1);
		return result;
	}

	// 3. 二级间接映射范围 (10+2048 ~ 10+2048+1024*1024-1)
	// 只有一个二级索引槽位 inode_index[INODE_INDEX_2]
	// 它指向一个二级索引块，该块包含 1024 个一级索引块地址
	if (logical_block_num < NODE_BLOCK_INDEX_3){
		// 计算相对于二级映射起始位置的偏移
        uint32 rel_idx = logical_block_num - INODE_BLOCK_INDEX_2;
        
		uint32 l2_block = inode_index[INODE_INDEX_2];
		if (l2_block == 0) {
			// 需要分配二级索引块
			l2_block = bitmap_alloc_block();
			if (l2_block == (uint32)-1)
				return -1; // 分配失败
			inode_index[INODE_INDEX_2] = l2_block;
			// 新分配的块需要清零
			buffer_t *new_buf = buffer_get(l2_block);
			memset(new_buf->data, 0, BLOCK_SIZE);
			buffer_write(new_buf);
			buffer_put(new_buf);
		}

		uint32 l1_idx = rel_idx / index_per_block; // 第几个一级索引块
		uint32 l1_off = rel_idx % index_per_block; // 块内偏移

		// 读取二级索引块
		buf2 = buffer_get(l2_block);
		uint32 *l2_table = (uint32 *)buf2->data;
		uint32 l1_block = l2_table[l1_idx];

		if (l1_block == 0) {
			// 需要分配一级索引块
			l1_block = bitmap_alloc_block();
			if (l1_block == (uint32)-1) {
				result = -1; // 分配失败
				buffer_put(buf2);
				return result;
			}
			l2_table[l1_idx] = l1_block;
			buffer_write(buf2); // 更新二级索引块

			// 新分配的块需要清零
			buffer_t *new_buf = buffer_get(l1_block);
			memset(new_buf->data, 0, BLOCK_SIZE);
			buffer_write(new_buf);
			buffer_put(new_buf);
		}

		// 读取一级索引块
		buf1 = buffer_get(l1_block);
		index_table = (uint32 *)buf1->data;
		block_num = index_table[l1_off];

		if (block_num == 0) {
			// 需要分配新的数据块
			block_num = bitmap_alloc_block();
			if (block_num == (uint32)-1) {
				result = -1; // 分配失败
				buffer_put(buf1);
				buffer_put(buf2);
				return result;
			}
			index_table[l1_off] = block_num;
			buffer_write(buf1); // 更新一级索引块

			// 新分配的块需要清零
			buffer_t *new_buf = buffer_get(block_num);
			memset(new_buf->data, 0, BLOCK_SIZE);
			buffer_write(new_buf);
			buffer_put(new_buf);
		}

		result = block_num;
		buffer_put(buf1);
		buffer_put(buf2);
		return result;
	}

	// 超出支持的文件大小范围
	return -1;
}

/*---------------------关于inode的管理: get dup lock unlock put----------------------*/

/* 
	磁盘里的inode <-> 内存里的inode
	调用者需要持有ip->slk并设置合理的inode_num
*/
void inode_rw(inode_t *ip, bool write)
{

}

/*
	尝试在inode_cache里寻找是否存在目标inode
	如果不存在则申请一个空闲的inode
	如果没有空闲位置直接panic
	核心逻辑: ref++
*/
inode_t *inode_get(uint32 inode_num)
{

}

/*
	在磁盘里创建1个新的inode
	1. 查询和修改inode_bitmap
	2. 填充inode_region对应位置的inode
	注意: 返回的inode未上锁
*/
inode_t *inode_create(uint16 type, uint16 major, uint16 minor)
{

}

/*
	ip->ref++ with lock proctect
*/
inode_t* inode_dup(inode_t* ip)
{

}

/*
	锁住inode
	如果inode->disk_info无效则更新一波
*/
void inode_lock(inode_t* ip)
{

}

/*
	解锁inode
*/
void inode_unlock(inode_t *ip)
{

}

/*
	与inode_get相对应, 调用者释放inode资源
	如果达成某些条件, 可能触发彻底删除
*/
void inode_put(inode_t* ip)
{

}

/*
	在磁盘里删除1个inode
	1. 修改inode_bitmap释放inode_region资源
	2. 修改block_bitmap释放block_region资源
	注意: 调用者需要持有ip->slk
*/
void inode_delete(inode_t *ip)
{

}

/*----------------------基于inode的数据读写操作--------------------*/

/*
	基于inode的数据读取
	inode管理的数据空间逻辑上是一个连续的数组data
	需要拷贝data[offset,offset+len)到dst(用户态地址/内核态地址)
	返回读取的数据量(字节)
*/
uint32 inode_read_data(inode_t *ip, uint32 offset, uint32 len, void *dst, bool is_user_dst)
{

}

/*
	基于inode的数据写入
	inode管理的数据空间逻辑上是一个连续的数组data
	需要拷贝src(用户态地址/内核态地址)到data[offset,offset+len)
	返回写入的数据量(字节)
*/
uint32 inode_write_data(inode_t *ip, uint32 offset, uint32 len, void *src, bool is_user_src)
{

}

static char *inode_type_list[] = {"DATA", "DIR", "DEVICE"};

/* 输出inode信息(for debug) */
void inode_print(inode_t *ip, char* name)
{
	assert(sleeplock_holding(&ip->slk), "inode_print: slk");

	spinlock_acquire(&lk_inode_cache);

	printf("inode %s:\n", name);
	printf("ref = %d, inode_num = %d, valid_info = %d\n", ip->ref, ip->inode_num, ip->valid_info);
	printf("type = %s, major = %d, minor = %d, nlink = %d, size = %d\n", inode_type_list[ip->disk_info.type],
		ip->disk_info.major, ip->disk_info.minor, ip->disk_info.nlink, ip->disk_info.size);

	printf("index_list = [ ");
	for (int i = 0; i < INODE_INDEX_1; i++)
		printf("%d ", ip->disk_info.index[i]);
	printf("] [ ");
	for (int i = INODE_INDEX_1; i < INODE_INDEX_2; i++)
		printf("%d ", ip->disk_info.index[i]);
	printf("] [ ");
	for (int i = INODE_INDEX_2; i < INODE_INDEX_3; i++)
		printf("%d ", ip->disk_info.index[i]);
	printf("]\n\n");

	spinlock_release(&lk_inode_cache);
}
