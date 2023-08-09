/*
** blockdevram - Amiga pseudo-driver to access 16bit-RAM in ZorroII space
**         as a block device, to be used as a RAM disk or swap space
** 
** Copyright (C) 1994 by Ingo Wilken (Ingo.Wilken@informatik.uni-oldenburg.de)
**
** ++Geert: support for zorro_unused_blockdevram, better range checking
** ++roman: translate accesses via an array
** ++Milan: support for ChipRAM usage
** ++yambo: converted to 2.0 kernel
** ++yambo: modularized and support added for 3 minor devices including:
**          MAJOR  MINOR  DESCRIPTION
**          -----  -----  ----------------------------------------------
**          37     0       Use Zorro II and Chip ram
**          37     1       Use only Zorro II ram
**          37     2       Use only Chip ram
**          37     4-7     Use memory list entry 1-4 (first is 0)
** ++jskov: support for 1-4th memory list entry.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

#define DEVICE_NAME "BLOCKDEVRAM"
#define BLOCKDEVRAM_MAJOR 250

#include <linux/major.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/blk-mq.h>
#include <linux/bitops.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/pgtable.h>

#include <asm/setup.h>
//#include <asm/amigahw.h>

//#include <linux/zorro.h>

#define BLOCKDEVMINOR_COMBINED      (0)
#define BLOCKDEVMINOR_BLOCKDEVONLY        (1)
#define BLOCKDEVMINOR_CHIPONLY      (2)
#define BLOCKDEVMINOR_MEMLIST1      (4)
#define BLOCKDEVMINOR_MEMLIST2      (5)
#define BLOCKDEVMINOR_MEMLIST3      (6)
#define BLOCKDEVMINOR_MEMLIST4      (7)
#define BLOCKDEVMINOR_COUNT         (8)	/* Move this down when adding a new minor */

#define BLOCKDEVRAM_CHUNK1024       ( BLOCKDEVRAM_CHUNKSIZE >> 10 )

static DEFINE_MUTEX(blockdevram_mutex);

static u_long blockdevram_size = 0;

static DEFINE_SPINLOCK(blockdevram_lock);

static struct gendisk *blockdevram_gendisk;

static blk_status_t blockdev_queue_rq(struct blk_mq_hw_ctx *hctx,
				const struct blk_mq_queue_data *bd)
{
	struct request *req = bd->rq;
	unsigned long start = blk_rq_pos(req) << 9;
	unsigned long len = blk_rq_cur_bytes(req);

	blk_mq_start_request(req);

	if (start + len > blockdevram_size) {
		pr_err(DEVICE_NAME ": bad access: block=%llu, "
		       "count=%u\n",
		       (unsigned long long)blk_rq_pos(req),
		       blk_rq_cur_sectors(req));
		return BLK_STS_IOERR;
	}

	spin_lock_irq(&blockdevram_lock);

	while (len) {
		unsigned long addr = start ;
		unsigned long size = len;
		void *buffer = bio_data(req->bio);

		if (rq_data_dir(req) == READ)
			memcpy(buffer, (char *)addr, size);
		else
			memcpy((char *)addr, buffer, size);
		start += size;
		len -= size;
	}

	spin_unlock_irq(&blockdevram_lock);
	blk_mq_end_request(req, BLK_STS_OK);
	return BLK_STS_OK;
}

static int blockdev_open(struct block_device *bdev, fmode_t mode)
{
	return 0;
}

static void blockdev_release(struct gendisk *disk, fmode_t mode)
{
	return ;
}

static const struct block_device_operations blockdev_fops = {
	.owner = THIS_MODULE,
	.open = blockdev_open,
	.release = blockdev_release,
};

static struct blk_mq_tag_set tag_set;

static const struct blk_mq_ops blockdev_mq_ops = {
	.queue_rq = blockdev_queue_rq,
};

static int blockdevram_register_disk(void)
{
	struct gendisk *disk;
	int err;

	disk = blk_mq_alloc_disk(&tag_set, NULL);
	if (IS_ERR(disk))
		return PTR_ERR(disk);

	disk->major = BLOCKDEVRAM_MAJOR;
	disk->first_minor = 0;
	disk->minors = 1;
	disk->flags |= GENHD_FL_NO_PART;
	disk->fops = &blockdev_fops;

	sprintf(disk->disk_name, "blockdevram");

	blockdevram_gendisk = disk;
	err = add_disk(disk);
	if (err)
		put_disk(disk);
	return err;
}

static int __init blockdev_init(void)
{
	int ret;

	if (register_blkdev(BLOCKDEVRAM_MAJOR, DEVICE_NAME))
		return -EBUSY;

	tag_set.ops = &blockdev_mq_ops;
	tag_set.nr_hw_queues = 1;
	tag_set.nr_maps = 1;
	tag_set.queue_depth = 16;
	tag_set.numa_node = NUMA_NO_NODE;
	tag_set.flags = BLK_MQ_F_SHOULD_MERGE;
	ret = blk_mq_alloc_tag_set(&tag_set);
	if (ret)
		goto out_unregister_blkdev;

    ret = blockdevram_register_disk();
    if (ret)
        goto out_free_tagset;

	return 0;

out_free_tagset:
	blk_mq_free_tag_set(&tag_set);
out_unregister_blkdev:
	unregister_blkdev(BLOCKDEVRAM_MAJOR, DEVICE_NAME);
	return ret;
}

static void __exit blockdev_exit(void)
{
	unregister_blkdev(BLOCKDEVRAM_MAJOR, DEVICE_NAME);

    del_gendisk(blockdevram_gendisk);
    put_disk(blockdevram_gendisk);
	blk_mq_free_tag_set(&tag_set);

	return;
}

module_init(blockdev_init);
module_exit(blockdev_exit);
MODULE_LICENSE("GPL");
