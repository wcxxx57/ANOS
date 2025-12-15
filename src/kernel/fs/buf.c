#include "mod.h"

static buffer_node_t buf_cache[N_BUFFER];
static buffer_node_t buf_head_active, buf_head_inactive;
static spinlock_t lk_buf_cache;

/* 
	将一个节点拿出来并插入
	1. 活跃链表的头部 buf_head_active->next
	2. 活跃链表的尾部 buf_head_active->prev
	3. 不活跃链表的头部 buf_head_inactive->next
	4. 不活跃链表的尾部 buf_head_inactive->prev
*/
static void insert_node(buffer_node_t *node, bool insert_active, bool insert_next)
{
	/* 如果有需要, 让node先离开当前位置 */
	if (node->next != NULL && node->prev != NULL) {
		node->next->prev = node->prev;
		node->prev->next = node->next;
	}

	/* 选择目标双向循环链表 */
	buffer_node_t *head = &buf_head_inactive;
	if (insert_active)
		head = &buf_head_active;

	/* 然后将node插入head->next or head->prev */	
	if (insert_next) {
		node->next = head->next;
		node->next->prev = node;
		node->prev = head;
		head->next = node;
	} else {
		node->prev = head->prev;
		node->prev->next = node;
		node->next = head;
		head->prev = node;
	}
}

/* 
	buffer系统初始化：
	1. 初始化全局的lk_buf_cache + buf_head_active + buf_head_inactive
	2. 初始化buf_cache中的所有node, 并将他们放在不活跃链表中
*/
void buffer_init()
{
	// 初始化两个链表头为自环
	buf_head_active.next = &buf_head_active;
	buf_head_active.prev = &buf_head_active;
	buf_head_inactive.next = &buf_head_inactive;
	buf_head_inactive.prev = &buf_head_inactive;
	spinlock_init(&lk_buf_cache, "buffer_cache");

	// 初始化所有缓存节点，放入不活跃链表。
	for (int i = 0; i < (int)N_BUFFER; i++) {
		buffer_node_t *node = &buf_cache[i];
		node->buf.block_num = BLOCK_NUM_UNUSED;
		node->buf.ref = 0;
		node->buf.data = NULL;
		node->buf.disk = false;
		sleeplock_init(&node->buf.slk, "buffer");
		// 为了让第一个buffer最终位于inactive->next，选择插入到尾部，保持顺序
		insert_node(node, /*active*/false, /*insert_next*/false);
	}
}

/* 磁盘读取: block -> buf */
static void buffer_read(buffer_t *buf)
{
	// 调用前应持有睡眠锁
	assert(sleeplock_held(&buf->slk), "buffer_read: sleeplock not held");
	virtio_disk_rw(buf, /*write*/false);
}

/* 磁盘写入: buf -> block */
void buffer_write(buffer_t *buf)
{
	assert(sleeplock_held(&buf->slk), "buffer_write: sleeplock not held");
	virtio_disk_rw(buf, /*write*/true);
}

/* 从buf_cache中获取一个buf */
buffer_t* buffer_get(uint32 block_num)
{
	spinlock_acquire(&lk_buf_cache);

	buffer_node_t *node;
	// 1) 先在活跃链表查找
	for (node = buf_head_active.next; node != &buf_head_active; node = node->next) {
		if (node->buf.block_num == block_num) {
			// 命中：移动到活跃链表头部
			insert_node(node, /*active*/true, /*insert_next*/true);
			node->buf.ref++;
			spinlock_release(&lk_buf_cache);
			sleeplock_acquire(&node->buf.slk);
			return &node->buf;
		}
	}
	// 2) 再在不活跃链表查找
	for (node = buf_head_inactive.next; node != &buf_head_inactive; node = node->next) {
		if (node->buf.block_num == block_num) {
			// 命中：移动到活跃链表头部
			insert_node(node, /*active*/true, /*insert_next*/true);
			node->buf.ref++;
			// 如果还没有物理页，为其分配
			if (node->buf.data == NULL) {
				uint64 pa = (uint64)pmem_alloc(false);
				assert(pa != 0, "buffer_get: pmem_alloc failed");
				node->buf.data = (uint8*)pa;
			}
			spinlock_release(&lk_buf_cache);
			sleeplock_acquire(&node->buf.slk);
			// 从磁盘读入最新数据
			buffer_read(&node->buf);//!!只有全新的块时，才必须读磁盘？
			return &node->buf;
		}
	}
	// 3) 缓存未命中：选择不活跃链表中最不活跃的（尾部）
	buffer_node_t *victim = buf_head_inactive.prev;
	// 应当是一个有效节点
	assert(victim != &buf_head_inactive, "buffer_get: no inactive buffer available");
	assert(victim->buf.ref == 0, "buffer_get: victim ref not zero");
	// 如无物理页则分配
	if (victim->buf.data == NULL) {
		uint64 pa = (uint64)pmem_alloc(false);
		assert(pa != 0, "buffer_get: pmem_alloc failed (victim)");
		victim->buf.data = (uint8*)pa;
	}
	// 绑定新的块号，移动到活跃链表头部并增加引用
	victim->buf.block_num = block_num;
	insert_node(victim, /*active*/true, /*insert_next*/true);
	victim->buf.ref++;
	spinlock_release(&lk_buf_cache);

	// 加锁并进行磁盘读取
	sleeplock_acquire(&victim->buf.slk);
	buffer_read(&victim->buf);
	return &victim->buf;
}

/* 向buf_cache归还一个buf */
void buffer_put(buffer_t *buf)
{
	// 释放内部睡眠锁
	if (sleeplock_held(&buf->slk))
		sleeplock_release(&buf->slk);

	spinlock_acquire(&lk_buf_cache);
	// 引用计数减一
	assert(buf->ref > 0, "buffer_put: ref already zero");
	buf->ref--;
	// 若无人引用，移入不活跃链表头部
	if (buf->ref == 0) {
		// 在两个链表中找到对应的节点
		buffer_node_t *node;
		for (node = buf_head_active.next; node != &buf_head_active; node = node->next) {
			if (&node->buf == buf) break;
		}
		if (node == &buf_head_active) {
			for (node = buf_head_inactive.next; node != &buf_head_inactive; node = node->next) {
				if (&node->buf == buf) break;
			}
		}
		assert(node != &buf_head_active && node != &buf_head_inactive, "buffer_put: node not found");
		insert_node(node, /*active*/false, /*insert_next*/true);
	}
	spinlock_release(&lk_buf_cache);
}

/*
	从后向前遍历非活跃链表, 尝试释放buffer_count个buffer持有的物理内存(data)
	返回成功释放资源的buffer数量
*/
uint32 buffer_freemem(uint32 buffer_count)
{
	spinlock_acquire(&lk_buf_cache);
	uint32 freed = 0;
	for (buffer_node_t *node = buf_head_inactive.prev; node != &buf_head_inactive && freed < buffer_count; node = node->prev) {
		// 仅处理无人引用的缓冲
		if (node->buf.ref == 0 && node->buf.data != NULL) {
			pmem_free((void*)node->buf.data);
			node->buf.data = NULL;
			freed++;
		}
	}
	spinlock_release(&lk_buf_cache);
	return freed;
}

/* 输出buffer_cache的信息 (for test) */
void buffer_print_info()
{
	buffer_node_t *node;

	assert(N_BUFFER == N_BUFFER_TEST, "buffer_print_info: invalid N_BUFFER");

	spinlock_acquire(&lk_buf_cache);

	printf("buffer_cache information:\n");
	
	printf("1.active list:\n");
	for (node = buf_head_active.next; node != &buf_head_active; node = node->next) {
		printf("buffer %d(ref = %d): page(pa = %p) -> block[%d]\n",
			(int)(node - buf_cache), node->buf.ref, (uint64)node->buf.data, node->buf.block_num);
	}
	printf("over!\n");

	printf("2.inactive list:\n");
	for (node = buf_head_inactive.next; node != &buf_head_inactive; node = node->next) {
		printf("buffer %d(ref = %d): page(pa = %p) -> block[%d]\n",
			(int)(node - buf_cache), node->buf.ref, (uint64)node->buf.data, node->buf.block_num);
	}
	printf("over!\n");

	spinlock_release(&lk_buf_cache);
}
