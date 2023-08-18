
#ifndef __QUEUE_H__
#define __QUEUE_H__


#ifndef USER_APP
#include <linux/spinlock.h>
//static DEFINE_SPINLOCK(g_sub_queue_lock);
//static DEFINE_SPINLOCK(g_com_queue_lock);
static DEFINE_SPINLOCK(g_queue_lock);
#endif


struct queue {
	int head;
	int tail;
	int size;
	//unsigned long entries;
};

/* shall be atomic */
#define rd_queue_head(q)	\
		(q->head)

/* shall be atomic */
#define rd_queue_tail(q)	\
		(q->tail)

/* shall be atomic */
#define set_queue_head(q, val)	\
		do {					\
			q->head = val;			\
		} while (0)

/* shall be atomic */
#define set_queue_tail(q, val)	\
		do {					\
			q->tail = val;			\
		} while (0)

#define queue_arr(q)		\
		(q->arr)

#ifndef USER_APP
static void* simple_memcpy(void* arr2, void* arr1, size_t count)
{
	void* ret = arr2;
	while (count--)
	{
		*(char*)arr2 = *(char*)arr1;
		arr1 = (char*)arr1 + 1;
		arr2 = (char*)arr2 + 1;
	}
	return ret;
}
#endif

static void queue_assign_to(struct queue *qbase, void *value, int size)
{
#if 0
	struct nupa_queue_entry* entry = (struct nupa_queue_entry*)((char*)qbase + sizeof(struct queue) + (head * sizeof(struct nupa_queue_entry)));
	struct nupa_queue_entry* _entry = (struct nupa_queue_entry*)value;
	entry->pb  = _entry->pb;
	entry->req = _entry->req;
#endif
#ifndef USER_APP
	printk("memcpy In \r\n");
#endif
#ifdef USER_APP
	memcpy((char*)qbase + sizeof(struct queue) + (qbase->head * size), value, size);
#else
	printk("copy to %px \r\n", (char*)qbase + sizeof(struct queue) + (qbase->head * size));
	simple_memcpy((char*)qbase + sizeof(struct queue) + (qbase->head * size), value, size);
#endif
#ifndef USER_APP
	printk("memcpy Out \r\n");
#endif
}

static void queue_assign_from(struct queue *qbase, void *value, int size)
{
#if 0
	struct nupa_queue_entry* entry = (struct nupa_queue_entry*)((char*)qbase + sizeof(struct queue) + (tail * sizeof(struct nupa_queue_entry)));
	struct nupa_queue_entry* _entry = (struct nupa_queue_entry*)value;
	_entry->pb  = entry->pb;
	_entry->req = entry->req;
#endif
#ifndef USER_APP
	printk("memcpy In \r\n");
#endif
#ifdef USER_APP
	memcpy(value, (char*)qbase + sizeof(struct queue) + (qbase->tail * size), size);
#else
	simple_memcpy(value, (char*)qbase + sizeof(struct queue) + (qbase->tail * size), size);
#endif
#ifndef USER_APP
	printk("memcpy out \r\n");
#endif
}


/* size shall be pow of 2 */
static inline bool qfull(int head, int tail, int size)
{
	int const mask = size - 1;

	if (((head - tail) & mask) == mask) {
		return true;
	}
	return false;
}

static inline bool qempty(int head, int tail)
{
	return head == tail;
}

static int qpush(struct queue *qbase, void *val, int entry_size)
{
#ifndef USER_APP
	spin_lock(&g_queue_lock);
#endif
	int const tail = rd_queue_tail(qbase);
	int const size = qbase->size;
	int head = rd_queue_head(qbase);
	//int head_;

	if (qfull(head, tail, size)) {
#ifndef USER_APP
		spin_unlock(&g_queue_lock);
#endif		
		return -1;
	}
	queue_assign_to(qbase, val, entry_size);
	head = (head + 1) & (size - 1);
	set_queue_head(qbase, head);
#ifndef USER_APP
	spin_unlock(&g_queue_lock);
#endif	
	return 0;
}

static int qpop(struct queue *q, void *val, int entry_size)
{
#ifndef USER_APP
	spin_lock(&g_queue_lock);
#endif
	int const head = rd_queue_head(q);
	int const size = q->size;
	int tail = rd_queue_tail(q);
	//int tail_;
	
	if (qempty(head, tail)) {
#ifndef USER_APP
		spin_unlock(&g_queue_lock);
#endif
		return -1;
	}

	queue_assign_from(q, val, entry_size);
	tail = (tail + 1) & (size - 1);
	set_queue_tail(q, tail);
#ifndef USER_APP
	spin_unlock(&g_queue_lock);
#endif
	return 0;
}

#endif
