#include "mod.h"

/*
	出于简化目的的假设:
	如果inode_disk.type == INODE_TYPE_DIR
	那么inode_disk.size <= BLOCKSIZE (只有inode_disk.index[0]有效)
	也就是说, 单个目录最多包含BLOCKSIZE / sizeof(dentry)个目录项

	另外, INODE_TYPE_DATA要求数据之间没有空隙
	但是对于INODE_TYPE_DIR来说是无法做到的(目录项的删除很常见)
	因此, ip->size代表block中已经使用的空间大小
*/


/*----------------dentry的查找、增加、删除操作-----------------*/

/*
	在目录ip中查找是否存在名字为name的目录项
	如果找到了返回目录项中存储的inode_num
	如果没找到返回INVALID_INODE_NUM
	注意: 调用者需要持有ip->slk
*/
uint32 dentry_search(inode_t *ip, char *name)
{
	assert(sleeplock_holding(&ip->slk), "dentry_search: slk!");
	assert(ip->disk_info.type == INODE_TYPE_DIR, "dentry_search: not dir!");

	// 目前简化假设目录只占用一个 block (index[0])
	uint32 block_num = ip->disk_info.index[0];
	if (block_num == 0)
		return INVALID_INODE_NUM;

	buffer_t *buf = buffer_get(block_num);
	dentry_t *de = (dentry_t *)buf->data;

	// 遍历 block 中的所有 dentry 槽位
	for (int i = 0; i < DENTRY_PER_BLOCK; i++) {
		// 检查当前槽位是否有效且名称匹配
		if (de[i].name[0] != 0 && strncmp(de[i].name, name, MAXLEN_FILENAME) == 0) {
			uint32 inode_num = de[i].inode_num;
			buffer_put(buf);
			return inode_num; // 找到匹配的目录项，返回对应的 inode_num
		}
	}

	buffer_put(buf);
	return INVALID_INODE_NUM; // 未找到匹配的目录项
}

/*
	在目录ip中寻找空闲槽位, 插入新的dentry
	如果成功插入则返回这个目录项的偏移量(还需要更新size)
	如果插入失败(没有空间/发生重名)返回-1
	注意: 调用者需要持有ip->slk
*/
uint32 dentry_create(inode_t *ip, uint32 inode_num, char *name)
{
	assert(sleeplock_holding(&ip->slk), "dentry_create: slk!");
	assert(ip->disk_info.type == INODE_TYPE_DIR, "dentry_create: not dir!");

	// 检查重名
	if (dentry_search(ip, name) != INVALID_INODE_NUM) {
		return -1; // 重名，插入失败
	}

	uint32 block_num = ip->disk_info.index[0];
	// 如果目录还没有分配 block，先分配一个
    if (block_num == 0) {
		block_num = bitmap_alloc_block();
		if (block_num == (uint32)-1) {
			return -1; // 分配 block 失败
		}
		ip->disk_info.index[0] = block_num;
		// 新分配的块需要清零，保证 dentry.name[0] == 0
		buffer_t *buf = buffer_get(block_num);
        memset(buf->data, 0, BLOCK_SIZE);
        buffer_write(buf);
        buffer_put(buf);
		// 目录大小更新为 BLOCK_SIZE（一个块）
		ip->disk_info.size = BLOCK_SIZE;
        inode_rw(ip, true); // 写回 inode 元数据
	}

	buffer_t *buf = buffer_get(block_num);
	dentry_t *de = (dentry_t *)buf->data;
	int empty_slot = -1;

	// 遍历 block 中的所有 dentry 槽位，寻找空闲槽位
	for (int i = 0; i < DENTRY_PER_BLOCK; i++) {
		if (de[i].name[0] == 0) { // 找到空闲槽位
			empty_slot = i;
			break;
		}
	}

	if (empty_slot == -1) {
		buffer_put(buf);
		return -1; // 没有空闲槽位，插入失败
	}

	// 写入新的 dentry
	memmove(de[empty_slot].name, name, MAXLEN_FILENAME);
    de[empty_slot].inode_num = inode_num;

	buffer_write(buf);
	buffer_put(buf);
	// 返回插入的目录项偏移量
	return (uint32)(empty_slot * sizeof(dentry_t)); 
}

/*
	在目录ip下删除名称为name的dentry, 返回它的inode_num
	如果匹配失败或者遇到非法情况返回INVALID_INODE_NUM
	注意: 调用者需要持有ip->slk
*/
uint32 dentry_delete(inode_t *ip, char *name)
{
	assert(sleeplock_holding(&ip->slk), "dentry_delete: slk!");
	assert(ip->disk_info.type == INODE_TYPE_DIR, "dentry_delete: not dir!");

	uint32 block_num = ip->disk_info.index[0];
	if (block_num == 0)
		return INVALID_INODE_NUM; // 目录为空，无法删除
	
	buffer_t *buf = buffer_get(block_num);
	dentry_t *de = (dentry_t *)buf->data;

	// 遍历 block 中的所有 dentry 槽位，寻找匹配的目录项
	for (int i = 0; i < DENTRY_PER_BLOCK; i++) {
		// 检查当前槽位是否有效且名称匹配
		if (de[i].name[0] != 0 && strncmp(de[i].name, name, MAXLEN_FILENAME) == 0) {
			uint32 inode_num = de[i].inode_num;
			// 删除：标记该槽位为空
			memset(de[i].name, 0, MAXLEN_FILENAME);
			de[i].inode_num = 0;

			buffer_write(buf);
			buffer_put(buf);
			return inode_num; // 返回被删除目录项的 inode_num
		}
	}

	buffer_put(buf);
	return INVALID_INODE_NUM; // 未找到匹配的目录项，删除失败
}

/* 输出目录中所有有效目录项的信息 (for debug) */
void dentry_print(inode_t *ip)
{
	assert(sleeplock_holding(&ip->slk), "dentry_print: slk!");
	assert(ip->disk_info.type == INODE_TYPE_DIR, "dentry_print: not dir!");

	dentry_t *de;
	buffer_t *buf;

	if (ip->disk_info.index[0] == 0)
		panic("dentry_print: invalid index[0]!");
	
	printf("inode_num = %d, dentries:\n", ip->inode_num);

	buf = buffer_get(ip->disk_info.index[0]);
	for (de = (dentry_t*)(buf->data); de < (dentry_t*)(buf->data + BLOCK_SIZE); de++)
	{
		if (de->name[0] != 0) {
			printf("dentry: offset = %d, inode_num = %d, name = %s\n",
				(uint32)((uint8*)de - buf->data), de->inode_num, de->name);
		}
	}
	buffer_put(buf);

	printf("\n");
}

/*------------------从文件名到文件路径-----------------*/

/*
	Examples:
	get_element("a/bb/c", name) = "bb/c" + name = "a"
	get_element("///aa//bb", name) = "bb" + name = "aa"
	get_element("aaa", name) = "" + name = "aaa"
	get_element("", name) = NULL + name = ""
	get_element("//", name) = NULL + name = ""
*/
static char* get_element(char *path, char *name)
{
	/* 跳过前置的'/' */
    while (*path == '/')
		path++;

	/* 如果遇到末尾了则返回 */
    if (*path == 0) {
		name[0] = 0;
		return NULL;
	}

	/* 记录起点位置 */
    char *start = path;
    
	/* 推进path直到遇到'/'或者到达末尾 */
	while (*path != '/' && *path != 0)
        path++;

	/* 提取到的name的长度 */
    int len = path - start;
	len = MIN(len, MAXLEN_FILENAME-1);
	
	/* 设置name */
	memmove(name, start, len);
	name[len] = 0;

	/* 跳过后置的'/' */
    while (*path == '/') path++;

    return path;
}
/*
	根据文件路径(/A/B/C)查找对应inode(inode_B or inode_C)
	如果find_parent_inode == true, 返回父节点inode, name为下一级子节点的名字
	如果find_parent_inode == false, 返回子节点inode, name无意义
	如果失败返回NULL
*/
static inode_t* __path_to_inode(char *path, char *name, bool find_parent_inode)
{
	inode_t *ip, *next_ip;

	// 1. 从根目录开始（目前只支持绝对路径）
	ip = inode_get(ROOT_INODE);
	inode_lock(ip);

	// 2. 循环解析路径分量
	while ((path = get_element(path, name)) != NULL) {
		// 如果需要找父节点，且 path 已经为空（说明 name 是最后一级），则当前 ip 就是父节点
		if (find_parent_inode && *path == '\0') {
			// 此时 name 已经被 get_element 填充为最后一级的文件名
            inode_unlock(ip); // 【修复】 必须解锁！
			return ip; // 返回父节点
		}

		// 在当前目录 ip 中查找 name
		if (ip->disk_info.type != INODE_TYPE_DIR) {
			// 不是目录，无法继续查找
			inode_unlock(ip);
			inode_put(ip);
			return NULL;
		}

		uint32 next_inode_num = dentry_search(ip, name);
		if (next_inode_num == INVALID_INODE_NUM) {
			// 未找到对应的目录项
			inode_unlock(ip);
			inode_put(ip);
			return NULL;
		}

		// 获取下一级 inode
		// 先释放当前目录锁和 inode，再获取下一级 inode（防止死锁）
		inode_unlock(ip); // 释放当前目录锁
		next_ip = inode_get(next_inode_num);
		inode_put(ip); // 释放当前目录 inode

		ip = next_ip;
		inode_lock(ip); // 锁定下一级 inode
	}

	// 3. 循环结束
	if (find_parent_inode) {
		// 需要返回父节点：但路径已经解析完毕，说明没有父节点
		inode_unlock(ip);
		inode_put(ip);
		return NULL;
	}

	// 需要返回最后一级 inode：
	inode_unlock(ip);
	return ip;
}

/*
	基于path寻找inode
	失败返回NULL
*/
inode_t* path_to_inode(char *path)
{
	char name[MAXLEN_FILENAME];
	return __path_to_inode(path, name, false);
}

/* 
	基于path寻找inode->parent, 将inode->name放入name
	失败返回NULL, 同时name无效
*/
inode_t* path_to_parent_inode(char *path, char *name)
{
	return __path_to_inode(path, name, true);
}
