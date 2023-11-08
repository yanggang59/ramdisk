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
#include <asm/setup.h>


#define DEVICE_NAME                "BLOCKDEVRAM"
#define RAM_BLOCK_SIZE             (1024 * 1024)  

static DEFINE_SPINLOCK(blockdevram_lock);

static struct gendisk *blockdevram_gendisk;

static char* ramdisk_buf;

static int major;

static void do_request(struct request *req)
{	
	unsigned long start = blk_rq_pos(req) << 9;

    struct bio_vec bv;
    struct req_iterator iter;
    void* buf = ramdisk_buf + start;
    printk("[do_request] start = %lld , len = %d\r\n", blk_rq_pos(req), blk_rq_sectors(req));

    if(rq_data_dir(req) == READ) {
        rq_for_each_segment(bv, req, iter) {
            struct page* page = bv.bv_page;
            unsigned int len = bv.bv_len;
            unsigned int offset = bv.bv_offset;
            void* dst = kmap(page);
            memcpy(dst + offset, buf, len);
			buf += bv.bv_len;
            kunmap(page);
	    }
    } else if(rq_data_dir(req) == WRITE) {
        rq_for_each_segment(bv, req, iter) {
            struct page* page = bv.bv_page;
            unsigned int len = bv.bv_len;
            unsigned int offset = bv.bv_offset;
            void* dst = kmap(page);
            memcpy(buf, dst + offset, len);
			buf += bv.bv_len;
            kunmap(page);
        }
    }
}

static blk_status_t blockdev_queue_rq(struct blk_mq_hw_ctx *hctx,
				const struct blk_mq_queue_data *bd)
{
	struct request *req;
	req = bd->rq;
	blk_mq_start_request(req);

	spin_lock_irq(&blockdevram_lock);

	do_request(req);

	spin_unlock_irq(&blockdevram_lock);
	blk_mq_end_request(req, BLK_STS_OK);
	return BLK_STS_OK;
}

static int blockdev_open(struct block_device *bdev, fmode_t mode)
{
	printk("[Info] blockdev_open \r\n");
	return 0;
}

static void blockdev_release(struct gendisk *disk, fmode_t mode)
{
	return ;
}

int blockdev_ioctl(struct block_device *bdev, fmode_t mode, unsigned cmd, unsigned long arg)
{
    printk("[Info] ioctl cmd 0x%08x\n", cmd);

    return -ENOTTY;
}

static int blockdev_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	geo->heads = 2;
	geo->cylinders = 32;
	geo->sectors = (RAM_BLOCK_SIZE / geo->heads / geo->cylinders);
	return 0;
}

static const struct block_device_operations blockdev_fops = {
	.owner = THIS_MODULE,
	.open = blockdev_open,
	.release = blockdev_release,
	.ioctl = blockdev_ioctl,
	.getgeo = blockdev_getgeo,
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

	disk->major = major;
	disk->first_minor = 0;
	disk->minors = 1;
	//disk->flags |= GENHD_FL_NO_PART_SCAN;
	disk->fops = &blockdev_fops;

	sprintf(disk->disk_name, "blockdevram");

	blockdevram_gendisk = disk;
	err = add_disk(disk);
	if (err)
		put_disk(disk);
	set_capacity(disk, (RAM_BLOCK_SIZE >> 9));
	return err;
}

static int __init blockdev_init(void)
{
	int ret;

	major = register_blkdev(0, DEVICE_NAME);

	ramdisk_buf = kmalloc(RAM_BLOCK_SIZE,GFP_KERNEL);

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
    if (ret) {
		goto out_free_tagset;
	}
        
	return 0;

out_free_tagset:
	blk_mq_free_tag_set(&tag_set);
out_unregister_blkdev:
	unregister_blkdev(major, DEVICE_NAME);
	return ret;
}

static void __exit blockdev_exit(void)
{
	unregister_blkdev(major, DEVICE_NAME);

    del_gendisk(blockdevram_gendisk);
    put_disk(blockdevram_gendisk);
	blk_mq_free_tag_set(&tag_set);
	kfree(ramdisk_buf);
	return;
}

module_init(blockdev_init);
module_exit(blockdev_exit);
MODULE_LICENSE("GPL");
