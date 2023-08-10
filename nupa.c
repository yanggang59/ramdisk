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


#define DEVICE_NAME                                      "BLOCKDEVRAM"
#define RESERVER_MEM_START                               (0x758000000)
#define RESERVER_MEM_SIZE                                (0x100000000)
#define LOCAL_RAMDISK_TEST                               0
#if LOCAL_RAMDISK_TEST
#define NUPA_BLOCK_SIZE                                   (1024 * 1024)
#else
#define NUPA_BLOCK_SIZE                                   RESERVER_MEM_SIZE
#endif

static DEFINE_SPINLOCK(nupa_lock);
static struct gendisk *nupa_gendisk;
static char* nupa_buf;
static int major;

static void do_request(struct request *req)
{	
	unsigned long start = blk_rq_pos(req) << 9;  	/* blk_rq_pos获取到的是扇区地址，左移9位转换为字节地址 */
	unsigned long len  = blk_rq_cur_bytes(req);		/* 大小   */

    static int w_cnt = 0;
    static int r_cnt = 0;
	void *buffer = bio_data(req->bio);		
	
	if(rq_data_dir(req) == READ) {
        printk("[Info] do_request read %d \r\n", ++r_cnt);
		memcpy(buffer, nupa_buf + start, len);
    } else if(rq_data_dir(req) == WRITE) {
        printk("[Info] do_request write %d \r\n", ++w_cnt);
        memcpy(nupa_buf + start, buffer, len);
    }

}

static blk_status_t nupa_queue_rq(struct blk_mq_hw_ctx *hctx,
				const struct blk_mq_queue_data *bd)
{
	struct request *req;
	req = bd->rq;
	blk_mq_start_request(req);

	spin_lock_irq(&nupa_lock);

	do_request(req);

	spin_unlock_irq(&nupa_lock);
	blk_mq_end_request(req, BLK_STS_OK);
	return BLK_STS_OK;
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
	geo->sectors = (NUPA_BLOCK_SIZE / geo->heads / geo->cylinders);
	return 0;
}

static const struct block_device_operations nupa_fops = {
	.owner = THIS_MODULE,
	.open = nupa_fops_open,
	.release = nupa_fops_release,
	.ioctl = nupa_fops_ioctl,
	.getgeo = nupa_fops_getgeo,
};

static struct blk_mq_tag_set tag_set;

static const struct blk_mq_ops nupa_mq_ops = {
	.queue_rq = nupa_queue_rq,
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
	disk->fops = &nupa_fops;

	sprintf(disk->disk_name, "nupa");

	nupa_gendisk = disk;
	err = add_disk(disk);
	if (err)
		put_disk(disk);
	set_capacity(disk, (NUPA_BLOCK_SIZE >> 9));
	return err;
}

static int __init blockdev_init(void)
{
	int ret;

	major = register_blkdev(0, DEVICE_NAME);
#if LOCAL_RAMDISK_TEST
	nupa_buf = kmalloc(NUPA_BLOCK_SIZE,GFP_KERNEL);
#else
	nupa_buf = ioremap(RESERVER_MEM_START, RESERVER_MEM_SIZE);
#endif
	tag_set.ops = &nupa_mq_ops;
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

    del_gendisk(nupa_gendisk);
    put_disk(nupa_gendisk);
	blk_mq_free_tag_set(&tag_set);
#if LOCAL_RAMDISK_TEST
	kfree(nupa_buf);
#else
	iounmap(nupa_buf);
#endif
	return;
}

module_init(blockdev_init);
module_exit(blockdev_exit);
MODULE_LICENSE("GPL");
