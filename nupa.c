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

#define DEBUG                                           1
#define DEVICE_NAME                                     "BLOCKDEVRAM"
#define RESERVE_MEM_START                               (0x100000000)
#define RESERVE_MEM_SIZE                                (0x100000000)
#define LOCAL_RAMDISK_TEST                              0
#if LOCAL_RAMDISK_TEST
#define NUPA_BLOCK_SIZE                                 (1024 * 1024)
#else
#define NUPA_BLOCK_SIZE                                 RESERVE_MEM_SIZE
#endif
#define SECTORS_PER_PAGE_SHIFT	(PAGE_SHIFT - SECTOR_SHIFT)
#define SECTORS_PER_PAGE	(1 << SECTORS_PER_PAGE_SHIFT)

static struct gendisk *nupa_gendisk;
static void* nupa_buf;
static int major;

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

static blk_qc_t nupa_fops_submit_bio(struct bio *bio)
{
	int offset;
	long start;
	struct bio_vec bvec;
	struct bvec_iter iter;

	start = bio->bi_iter.bi_sector << SECTOR_SHIFT;

	bio_for_each_segment(bvec, bio, iter) {
		unsigned int len = bvec.bv_len;
		struct page* page = bvec.bv_page;
		void* dst = kmap(page);
		offset = bvec.bv_offset;
		if (bio_op(bio) == REQ_OP_READ) {
			memcpy(dst + offset, nupa_buf + start, len);
		} else if (bio_op(bio) == REQ_OP_WRITE) {
			memcpy(nupa_buf + start, dst + offset, len);
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

static int nupa_register_disk(void)
{
	struct gendisk *disk;
	int err;

	disk = blk_alloc_disk(NUMA_NO_NODE);
	if (IS_ERR(disk))
		return PTR_ERR(disk);

	disk->major = major;
	disk->first_minor = 0;
	disk->minors = 1;
	disk->fops = &nupa_fops;

	sprintf(disk->disk_name, "nupa");

	nupa_gendisk = disk;
	err = add_disk(disk);
	if (err)
		put_disk(disk);
	set_capacity(disk, (NUPA_BLOCK_SIZE >> 9));
	return err;
}

#if DEBUG
#define PRINT printk
static void print_buf(char* buf, int size)
{
	int i ,j;
	PRINT("**********************************************************************\r\n");
    PRINT("   ");
	for(i = 0; i < 16; i++)
		PRINT("%4X",i);
    PRINT("\n======================================================================");
	for(j = 0; j < size; j++) {
		if(j % 16 == 0)
			PRINT("\n%4X||",j);
		PRINT("%4X",buf[j]);
	}
	PRINT("\n**********************************************************************\r\n");
}

static void simple_buf_test(void* buf, long size)
{
	if(size > NUPA_BLOCK_SIZE)
		size = NUPA_BLOCK_SIZE;
	memset(buf, 'A', size);
}
#endif

static int __init blockdev_init(void)
{
	int ret;

	major = register_blkdev(0, DEVICE_NAME);
#if LOCAL_RAMDISK_TEST
	nupa_buf = kmalloc(NUPA_BLOCK_SIZE, GFP_KERNEL);
#else
	nupa_buf = ioremap(RESERVE_MEM_START, RESERVE_MEM_SIZE);
#endif
#if DEBUG
	simple_buf_test(nupa_buf, 1024 * 1024);
	printk("[Info] nupa_buf = %p \r\n", nupa_buf);
#endif	
    ret = nupa_register_disk();
    if (ret) {
		goto out;
	}
        
	return 0;

out:
	return ret;
}

static void __exit blockdev_exit(void)
{
	unregister_blkdev(major, DEVICE_NAME);

    del_gendisk(nupa_gendisk);
    put_disk(nupa_gendisk);
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
