#include <linux/major.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/blk-mq.h>
#include <linux/bitops.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/pgtable.h>
#include <linux/hdreg.h>
#include <linux/uio_driver.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include "nupa.h"
#include "queue.h"

static struct nupa_dev* g_nupa_dev;
struct nupa_meta_info_header* g_meta_info_header;
struct queue *g_nupa_sub_queue;
struct queue *g_nupa_com_queue;

struct uio_info nupa_uio_info = {
    .name = "nupa_uio",
    .version = "1.0",
    .irq = UIO_IRQ_NONE,
};

static void sub_queue_assign_to(struct queue *qbase, int head, void *entry)
{
	memcpy(&g_nupa_sub_queue->entries[head], entry, sizeof(struct nupa_queue_entry));
}

static void sub_queue_assign_from(struct queue *qbase, int tail, void *entry)
{
	memcpy(entry, &g_nupa_sub_queue->entries[tail], sizeof(struct nupa_queue_entry));
}

static void com_queue_assign_to(struct queue *qbase, int head, void *entry)
{
	memcpy(&g_nupa_com_queue->entries[head], entry, sizeof(struct nupa_queue_entry));
}

static void com_queue_assign_from(struct queue *qbase, int tail, void *entry)
{
	memcpy(entry, &g_nupa_com_queue->entries[tail], sizeof(struct nupa_queue_entry));
}

/**
*                                 meta_data               entries                      entries
*       --------------------------------------------------------------------------------------------------------
*       |                             |        |           |      |      |      |         |      |      |
*       |         DATA                | header | sub queue |  x   |   x  |  ... |com queue|  x   |  x   | ...
*       |                             |        |           |      |      |      |         |      |      |
*       --------------------------------------------------------------------------------------------------------
*/

static void nupa_meta_data_init(void)
{
	g_meta_info_header = (struct nupa_meta_info_header*)(g_nupa_dev->nupa_buf + NUPA_DATA_SIZE);
	memset(g_meta_info_header, 0, sizeof(struct nupa_meta_info_header));
	memset(g_meta_info_header->vb, 0xFF, sizeof(g_meta_info_header->vb));

	g_nupa_sub_queue = (struct queue *)((char*)g_meta_info_header + sizeof(struct nupa_meta_info_header));
	memset(g_nupa_sub_queue, 0, sizeof(struct queue));
	g_nupa_sub_queue->size = QUEUE_SIZE;
	g_nupa_sub_queue->entries = (struct nupa_queue_entry*) ((char*)g_nupa_sub_queue + sizeof(struct queue));
	g_nupa_sub_queue->assign_to = sub_queue_assign_to;
	g_nupa_sub_queue->assign_from = sub_queue_assign_from;

	g_nupa_com_queue = (struct queue *)((char*)g_nupa_sub_queue + QUEUE_SIZE * sizeof(struct nupa_queue_entry));
	memset(g_nupa_com_queue, 0, sizeof(struct queue));
	g_nupa_com_queue->size = QUEUE_SIZE;
	g_nupa_com_queue->entries = (struct nupa_queue_entry*)((char*)g_nupa_com_queue + sizeof(struct queue));
	g_nupa_com_queue->assign_to = com_queue_assign_to;
	g_nupa_com_queue->assign_from = com_queue_assign_from;
	return;
}

bool is_vb_dirty(unsigned long vb, unsigned long* dirty_bit_map)
{
	unsigned long byte = vb / sizeof(unsigned long);
	unsigned long offset = vb % sizeof(unsigned long);
	unsigned value = dirty_bit_map[byte];
	return (value & (1 << offset));
}

void set_vb_dirty(unsigned long vb, unsigned long* dirty_bit_map)
{
	unsigned long byte = vb / sizeof(unsigned long);
	unsigned long offset = vb % sizeof(unsigned long);
	dirty_bit_map[byte] |= 1 << offset;
}

void clr_vb_dirty(unsigned long vb, unsigned long* dirty_bit_map)
{
	unsigned long byte = vb / sizeof(unsigned long);
	unsigned long offset = vb % sizeof(unsigned long);
	dirty_bit_map[byte] &= ~(1 << offset);
}


static int nupa_uio_probe(struct platform_device *pdev) {
  struct device *dev = &pdev->dev;
  nupa_uio_info.mem[0].name = "area1";
  nupa_uio_info.mem[0].addr = RESERVE_MEM_START;
  nupa_uio_info.mem[0].memtype = UIO_MEM_PHYS;
  nupa_uio_info.mem[0].size = NUPA_DISK_SIZE;
  return uio_register_device(dev, &nupa_uio_info);
}

static int nupa_uio_remove(struct platform_device *pdev) 
{
	printk("[Info] nupa_uio_remove\r\n");
	uio_unregister_device(&nupa_uio_info);
	return 0;
}
static struct platform_device *nupa_uio_device;
static struct platform_driver nupa_uio_driver = {
    .driver =
        {
            .name = "nupa_uio",
        },
    .probe = nupa_uio_probe,
    .remove = nupa_uio_remove,
};

static int nupa_uio_init(void) 
{
  nupa_uio_device = platform_device_register_simple("nupa_uio", -1, NULL, 0);
  return platform_driver_register(&nupa_uio_driver);
}

static void nupa_uio_exit(void) 
{
  platform_device_unregister(nupa_uio_device);
  platform_driver_unregister(&nupa_uio_driver);
}

static int nupa_fops_open(struct block_device *bdev, fmode_t mode)
{
	printk("[Info] nupa_open \r\n");
	return 0;
}

static void nupa_fops_release(struct gendisk *disk, fmode_t mode)
{
	return ;
}

static int nupa_fops_ioctl(struct block_device *bdev, fmode_t mode, unsigned cmd, unsigned long arg)
{
    printk("[Info] ioctl cmd 0x%08x\n", cmd);
    return -ENOTTY;
}

static int nupa_fops_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	geo->heads = 2;
	geo->cylinders = 32;
	geo->sectors = (NUPA_DISK_SIZE / geo->heads / geo->cylinders);
	return 0;
}

/**
* for read
* 1. if vb is invalid or vb is not equal to pb, read frome remote
*	 if vb is invalid ,do we have to wait until we read from remote? 
*    consider rebuild case, simplest solution is read from remote ,could be optimized later
* 2. if vb is valid , check dirty bit
*   (1) if clean, direct read
*   (2) if dirty, also direct read, coz local cache is always newest, no need to read from remote
*/
static void nupa_process_read(void* u_buf, void* k_buf, unsigned long start, struct bio_vec* bvec)
{
	unsigned long pb , vb, addr, size;
	unsigned long len  = bvec->bv_len;
	unsigned long offset = bvec->bv_offset;
	while(len) {
		pb = start / NUPA_BLOCK_SIZE;
        vb = pb % CACHE_BLOCK_NUM;
		addr = start & NUPA_BLOCK_SIZE_MASK;
		size = NUPA_BLOCK_SIZE - addr;
		if (len < size)
			size = len;

		if(g_meta_info_header->vb[vb] == INVALID_ADDR) {
			/**
			*  1. sync read from remote
			*  2. set pb
			*  3. do local copy
			*/
			/**
			* TODO: sync read pb from remote
			*/

			g_meta_info_header->vb[vb] = pb;
		} else if(g_meta_info_header->vb[vb] != pb) {
			/**
			*  1. check dirty bit
			*  2. if dirty, wait till clean
			*  3. if clean, sync read from remote, set pb, then do local copy  
			*/
			while(is_vb_dirty(vb, g_meta_info_header->dirty_bit_map));
			/**
			* TODO: sync read pb from remote
			*/

			g_meta_info_header->vb[vb] = pb;
		}else if(g_meta_info_header->vb[vb] == pb) {
			/**
			* doesn't matter if it's dirty or not, coz local is always newer then remote
			* just do local copy
			*/
			printk("[Read] local cache hit\r\n");
		}

		memcpy(u_buf + offset, k_buf + start, size);

		start += size;
		offset += size;
		k_buf += size;
		u_buf += size;
        len -= size;
	}
}


/**
* for write
* 1. if vb is INVALID or vb is not equal to pb, direct write
* 2. if vb is equal to pb, check dirty bit
*    (1) if clean, direct write
*    (2) if dirty, wait till dirty bit is clean, then write  
*/
static void nupa_process_write(void* u_buf, void* k_buf, unsigned long start, struct bio_vec* bvec)
{
	unsigned long pb , vb, addr, size;
	unsigned long len  = bvec->bv_len;
	unsigned long offset = bvec->bv_offset;
	while(len) {
		pb = start / NUPA_BLOCK_SIZE;
        vb = pb % CACHE_BLOCK_NUM;
		addr = start & NUPA_BLOCK_SIZE_MASK;
		size = NUPA_BLOCK_SIZE - addr;
		if (len < size)
			size = len;

        if((g_meta_info_header->vb[vb] == INVALID_ADDR)) {
			/** This vb is new 
			* 1. write to local ,set pb and set dirty bit 
			* 2. send to process queue
			*/
			printk("[Info] write to local\r\n");
			memcpy(k_buf + start, u_buf + offset, len);
			/** 
			* TODO: send to process queue
			*/
			set_vb_dirty(vb, g_meta_info_header->dirty_bit_map);
			g_meta_info_header->vb[vb] = pb;

		} else if(g_meta_info_header->vb[vb] != pb) {
			/**  
			* 1. check dirty bit
			* 2. if clean, direct write
			* 3. if dirty, wait till dirty bit is clean, then read from remote
			* 4. write to local , set pb and set dirty bit
			* 5. send to process queue
			*/
			// if vb is dirty, wait till app flush dirty block into remote and clean dirty bit
			while(is_vb_dirty(vb, g_meta_info_header->dirty_bit_map)); 
			memcpy(k_buf + start, u_buf + offset, len);
			set_vb_dirty(vb, g_meta_info_header->dirty_bit_map);
			g_meta_info_header->vb[vb] = pb;
			/**
			* TODO: send to process queue
			*/


		} else if(g_meta_info_header->vb[vb] == pb) {
			/** 
			* 1. check dirty bit
			* 2. if clean, direct write
			* 3. if dirty, wait till dirty bit is clean
			* 4. write to local , set pb and set dirty bit
			* 5. send to process queue
			*/
			while(is_vb_dirty(vb, g_meta_info_header->dirty_bit_map)); 
			memcpy(k_buf + start, u_buf + offset, len);
			set_vb_dirty(vb, g_meta_info_header->dirty_bit_map);
			/**
			* TODO: send to process queue
			*/
		} 
        start += size;
		offset += size;
		k_buf += size;
		u_buf += size;
        len -= size;
	}
}

static blk_qc_t nupa_fops_submit_bio(struct bio *bio)
{
	int offset;
	unsigned long start;
	struct bio_vec bvec;
	struct bvec_iter iter;

	start = bio->bi_iter.bi_sector << SECTOR_SHIFT;

	bio_for_each_segment(bvec, bio, iter) {
		unsigned int len = bvec.bv_len;
		struct page* page = bvec.bv_page;
		void* dst = kmap(page);
		offset = bvec.bv_offset;
		if (bio_op(bio) == REQ_OP_READ) {
#if CONFIG_SUPPORT_WAF
			nupa_process_read(dst, g_nupa_dev->nupa_buf, start, &bvec);
#else
			memcpy(dst + offset, g_nupa_dev->nupa_buf + start, len);
#endif
		} else if (bio_op(bio) == REQ_OP_WRITE) {
#if CONFIG_SUPPORT_WAF
			nupa_process_write(dst, g_nupa_dev->nupa_buf, start, &bvec);
#else
			memcpy(g_nupa_dev->nupa_buf + start, dst + offset, len);
#endif
		}
		start += len;
		kunmap(page);
	}
	bio_endio(bio);
	return BLK_QC_T_NONE;
}


static const struct block_device_operations nupa_fops = {
	.owner = THIS_MODULE,
	.open = nupa_fops_open,
	.release = nupa_fops_release,
	.ioctl = nupa_fops_ioctl,
	.getgeo = nupa_fops_getgeo,
	.submit_bio = nupa_fops_submit_bio,
};

static int nupa_register_disk(struct nupa_dev* nupa_dev)
{
	struct gendisk *disk;
	int err;

	disk = blk_alloc_disk(NUMA_NO_NODE);
	if (IS_ERR(disk))
		return PTR_ERR(disk);

	disk->major = g_nupa_dev->major;
	disk->first_minor = 0;
	disk->minors = 1;
	disk->fops = &nupa_fops;

	sprintf(disk->disk_name, "nupa");

	nupa_dev->nupa_gendisk = disk;
	err = add_disk(disk);
	if (err)
		put_disk(disk);
	set_capacity(disk, (NUPA_DISK_SIZE >> 9));
	return err;
}

#if DEBUG
static void simple_buf_test(void* buf, long size)
{
	if(size > NUPA_DISK_SIZE)
		size = NUPA_DISK_SIZE;
	memset(buf, 'A', size);
}
#endif

static int __init blockdev_init(void)
{
	int ret;
	g_nupa_dev = kmalloc(sizeof(struct nupa_dev), GFP_KERNEL);
	if(!g_nupa_dev) {
		printk("[Error] Create nupa_dev failed \r\n");
		goto out;
	}
	g_nupa_dev->major = register_blkdev(0, DEVICE_NAME);
#if LOCAL_RAMDISK_TEST
	g_nupa_dev->nupa_buf = kmalloc(NUPA_DISK_SIZE, GFP_KERNEL);
#else
	g_nupa_dev->nupa_buf = ioremap(RESERVE_MEM_START, NUPA_DISK_SIZE);
#endif
#if DEBUG
	simple_buf_test(g_nupa_dev->nupa_buf, 1024 * 1024);
	printk("[Info] nupa_buf = %p \r\n", g_nupa_dev->nupa_buf);
#endif	
    ret = nupa_register_disk(g_nupa_dev);
    if (ret) {
		goto out;
	}
	nupa_meta_data_init();
	nupa_uio_init();
	return 0;
out:
	return ret;
}

static void __exit blockdev_exit(void)
{
	unregister_blkdev(g_nupa_dev->major, DEVICE_NAME);
    del_gendisk(g_nupa_dev->nupa_gendisk);
    put_disk(g_nupa_dev->nupa_gendisk);
#if LOCAL_RAMDISK_TEST
	kfree(g_nupa_dev->nupa_buf);
#else
	iounmap(g_nupa_dev->nupa_buf);
#endif
	kfree(g_nupa_dev);
	nupa_uio_exit();
	return;
}

module_init(blockdev_init);
module_exit(blockdev_exit);
MODULE_LICENSE("GPL");
