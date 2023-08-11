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

static struct nupa_dev* g_nupa_dev;

struct uio_info nupa_uio_info = {
    .name = "nupa_uio",
    .version = "1.0",
    .irq = UIO_IRQ_NONE,
};

static int nupa_uio_probe(struct platform_device *pdev) {
  struct device *dev = &pdev->dev;
  nupa_uio_info.mem[0].name = "area1";
  nupa_uio_info.mem[0].addr = RESERVE_MEM_START;//(unsigned long)g_nupa_dev->nupa_buf;
  nupa_uio_info.mem[0].memtype = UIO_MEM_PHYS;
  nupa_uio_info.mem[0].size = NUPA_BLOCK_SIZE;
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
			memcpy(dst + offset, g_nupa_dev->nupa_buf + start, len);
		} else if (bio_op(bio) == REQ_OP_WRITE) {
			memcpy(g_nupa_dev->nupa_buf + start, dst + offset, len);
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
	set_capacity(disk, (NUPA_BLOCK_SIZE >> 9));
	return err;
}

#if DEBUG
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
	g_nupa_dev = kmalloc(sizeof(struct nupa_dev), GFP_KERNEL);
	if(!g_nupa_dev) {
		printk("[Error] Create nupa_dev failed \r\n");
		goto out;
	}
	g_nupa_dev->major = register_blkdev(0, DEVICE_NAME);
#if LOCAL_RAMDISK_TEST
	g_nupa_dev->nupa_buf = kmalloc(NUPA_BLOCK_SIZE, GFP_KERNEL);
#else
	g_nupa_dev->nupa_buf = ioremap(RESERVE_MEM_START, RESERVE_MEM_SIZE);
#endif
#if DEBUG
	simple_buf_test(g_nupa_dev->nupa_buf, 1024 * 1024);
	printk("[Info] nupa_buf = %p \r\n", g_nupa_dev->nupa_buf);
#endif	
    ret = nupa_register_disk(g_nupa_dev);
    if (ret) {
		goto out;
	}
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
