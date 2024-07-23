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
#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/aer.h>
#include <linux/memremap.h>
#include "ramdisk.h"


struct ramdisk_dev g_ramdisk_dev;

#define DEVICE_NAME                 "ramdisk"
#define DRV_MODULE_NAME             "ramdisk_module"
#define RAMDISK_SIZE                (1 * 1024 * 1024)

MODULE_AUTHOR("< YangGang 1449381582@qq.com >");
MODULE_LICENSE("Dual BSD/GPL");


static void write_ramdisk(void *addr, struct page *page, unsigned int off, unsigned int len)
{
	unsigned int chunk;
	void *mem;

	while (len) {
		mem = kmap_atomic(page);
		chunk = min_t(unsigned int, len, PAGE_SIZE - off);
		memcpy_flushcache(addr, mem + off, chunk);
		kunmap_atomic(mem);
		len -= chunk;
		off = 0;
		page++;
		addr += chunk;
	}
}

static blk_status_t read_ramdisk(struct page *page, unsigned int off, void *addr, unsigned int len)
{
	unsigned int chunk;
	unsigned long rem;
	void *mem;

	while (len) {
		mem = kmap_atomic(page);
		chunk = min_t(unsigned int, len, PAGE_SIZE - off);
		rem = copy_mc_to_kernel(mem + off, addr, chunk);
		kunmap_atomic(mem);
		if (rem)
			return BLK_STS_IOERR;
		len -= chunk;
		off = 0;
		page++;
		addr += chunk;
	}
	return BLK_STS_OK;
}

static blk_status_t ramdisk_do_read(struct ramdisk_dev* r_dev, struct page *page, unsigned int page_off, sector_t sector, unsigned int len)
{
	blk_status_t rc;
	phys_addr_t off = sector * 512 ;
	void *addr = r_dev->virt_addr + off;
	rc = read_ramdisk(page, page_off, addr, len);
	flush_dcache_page(page);
	return rc;
}

static blk_status_t ramdisk_do_write(struct ramdisk_dev* r_dev, struct page *page, unsigned int page_off, sector_t sector, unsigned int len)
{
	phys_addr_t off = sector * 512 ;
	void *addr = r_dev->virt_addr + off;
	flush_dcache_page(page);
	write_ramdisk(addr, page, page_off, len);
	return BLK_STS_OK;
}

static int ramdisk_rw_page(struct block_device *bdev, sector_t sector, struct page *page, unsigned int op)
{
	struct ramdisk_dev* r_dev = bdev->bd_disk->private_data;
	blk_status_t rc;
	if (op_is_write(op))
		rc = ramdisk_do_write(r_dev, page, 0, sector, thp_size(page));
	else
		rc = ramdisk_do_read(r_dev, page, 0, sector, thp_size(page));
	if (rc == 0)
		page_endio(page, op_is_write(op), 0);
	return blk_status_to_errno(rc);
}

static blk_qc_t ramdisk_submit_bio(struct bio *bio)
{
	int ret = 0;
	blk_status_t rc = 0;
	bool do_acct;
	unsigned long start;
	struct bio_vec bvec;
	struct bvec_iter iter;
	struct ramdisk_dev* r_dev = bio->bi_bdev->bd_disk->private_data;

	do_acct = blk_queue_io_stat(bio->bi_bdev->bd_disk->queue);
	if (do_acct)
		start = bio_start_io_acct(bio);
	bio_for_each_segment(bvec, bio, iter) {
		if (op_is_write(bio_op(bio)))
			rc = ramdisk_do_write(r_dev, bvec.bv_page, bvec.bv_offset,
				iter.bi_sector, bvec.bv_len);
		else
			rc = ramdisk_do_read(r_dev, bvec.bv_page, bvec.bv_offset,
				iter.bi_sector, bvec.bv_len);
		if (rc) {
			bio->bi_status = rc;
			break;
		}
	}
	if (do_acct)
		bio_end_io_acct(bio, start);

	if (ret)
		bio->bi_status = errno_to_blk_status(ret);

	bio_endio(bio);
	return BLK_QC_T_NONE;
}

static const struct block_device_operations ramdisk_fops = {
	.owner =		THIS_MODULE,
	.submit_bio =	ramdisk_submit_bio,
	.rw_page =		ramdisk_rw_page,
};

static int disk_init(struct ramdisk_dev* r_dev)
{
	struct gendisk *disk;
	struct request_queue *q;
	disk = blk_alloc_disk(0);
	if (!disk)
		return -ENOMEM;
	q = disk->queue;
	r_dev->disk = disk;
	blk_queue_physical_block_size(q, PAGE_SIZE);
	blk_queue_logical_block_size(q, 512);
	blk_queue_max_hw_sectors(q, UINT_MAX);
	blk_queue_flag_set(QUEUE_FLAG_NONROT, q);
	disk->fops		= &ramdisk_fops;
	disk->private_data	= r_dev;
	sprintf(disk->disk_name, "ramdisk");
	set_capacity(disk, (r_dev->size) / 512);
	device_add_disk(&r_dev->dev, disk, NULL);
	return 0;
}


static int __init ramdisk_init(void)
{
    int err;
    device_initialize(&g_ramdisk_dev.dev);
    dev_set_name(&g_ramdisk_dev.dev, "ramdisk");
    err = device_add(&g_ramdisk_dev.dev);
    if(err) {
        return err;
    }
	g_ramdisk_dev.size = RAMDISK_SIZE;
    g_ramdisk_dev.virt_addr = kmalloc(RAMDISK_SIZE, GFP_KERNEL);
    return disk_init(&g_ramdisk_dev);
}

static __exit void ramdisk_exit(void)
{
	del_gendisk(g_ramdisk_dev.disk);
	blk_cleanup_disk(g_ramdisk_dev.disk);
}


module_init(ramdisk_init);
module_exit(ramdisk_exit);
MODULE_LICENSE("GPL");
