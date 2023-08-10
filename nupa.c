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
#define LOCAL_RAMDISK_TEST                              1
#if LOCAL_RAMDISK_TEST
#define NUPA_BLOCK_SIZE                                 (1024 * 1024)
#else
#define NUPA_BLOCK_SIZE                                 RESERVE_MEM_SIZE
#endif
#define SECTORS_PER_PAGE_SHIFT	(PAGE_SHIFT - SECTOR_SHIFT)
#define SECTORS_PER_PAGE	(1 << SECTORS_PER_PAGE_SHIFT)

static DEFINE_SPINLOCK(nupa_lock);
static struct gendisk *nupa_gendisk;
static void* nupa_buf;
static int major;

static void do_request(struct request *req)
{	
#if 1
	unsigned long start = blk_rq_pos(req) << 9;	
	unsigned long len  = blk_rq_cur_bytes(req);
	void *buffer = bio_data(req->bio);	
	printk("[Info] start = %ld , len = %ld \r", start, len);
	
	
	if(rq_data_dir(req) == READ) {
        //printk("[Info] do_request read sector %ld \r\n", start);
		memcpy(buffer, nupa_buf + start, len);
    } else if(rq_data_dir(req) == WRITE) {
        //printk("[Info] do_request write sector %ld \r\n", start);
        memcpy(nupa_buf + start, buffer, len);
    }
#else
	struct bio *bio = req->bio;
	struct bio_vec bvec;
	struct bvec_iter iter;
    void *dsk_mem;
    //获得块设备内存的起始地址，bi_sector代表起始扇区
    dsk_mem = nupa_buf + (bio->bi_iter.bi_sector << 9);
    bio_for_each_segment(bvec, bio, iter) {//遍历每一个块
        void *iovec_mem;
        switch (bio_op(bio)) {
            case REQ_OP_READ:
                //page代表高端内存无法直接访问，需要通过kmap映射到线性地址
                iovec_mem = kmap(bvec.bv_page) + bvec.bv_offset;//页数加偏移量获得对应的内存地址
                memcpy(iovec_mem, dsk_mem, bvec.bv_len);//将数据拷贝到内存中
                kunmap(bvec.bv_page);//归还线性地址
                break;
            case REQ_OP_WRITE:
                iovec_mem = kmap(bvec.bv_page) + bvec.bv_offset; 
                memcpy(dsk_mem, iovec_mem, bvec.bv_len); 
                kunmap(bvec.bv_page);
                break;
            default:
                printk("unknown value of bio_rw: %lu\n", bio_op(bio));
                bio_endio(bio);//报错
                return;
        }
        dsk_mem += bvec.bv_len;//移动地址
    }

	bio_endio(bio);
	return;
#endif
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

static void nupa_fops_submit_bio(struct bio *bio)
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
	//print_buf(buf, size);
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
