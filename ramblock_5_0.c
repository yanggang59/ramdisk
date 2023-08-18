#define DEVICE_NAME "RAMBLOCK"

#include <linux/major.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/blk-mq.h>
#include <linux/bitops.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include <asm/setup.h>
#include <asm/pgtable.h>

#define RAMBLOCK_MINOR_CNT         (8) /* Move this down when adding a new minor */
#define RAM_BLOCK_SIZE             (1024 * 1024)
static DEFINE_MUTEX(ramblock_mutex);
static int major = 0;

static struct gendisk *ramblock_gendisk;
void* ramblock_buf;

static int ramblock_open(struct block_device *bdev, fmode_t mode)
{
    printk("[Info] ramblock_open \r\n");
    return 0;
}

static void ramblock_release(struct gendisk *disk, fmode_t mode)
{
    printk("[Info] ramblock_release \r\n");
}

static const struct block_device_operations ramblock_fops =
{
    .owner        = THIS_MODULE,
    .open        = ramblock_open,
    .release    = ramblock_release,
};

static struct kobject *ramblock_find(dev_t dev, int *part, void *data)
{
    *part = 0;
    return get_disk_and_module(ramblock_gendisk);
}

static struct request_queue *ramblock_queue;
static struct blk_mq_tag_set tag_set;


static void __ramblock_make_request(struct bio *bio)
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
            memcpy(dst + offset, ramblock_buf + start, len);
        } else if (bio_op(bio) == REQ_OP_WRITE) {
            memcpy(ramblock_buf + start, dst + offset, len);
        }
        start += len;
        kunmap(page);
    }
    bio_endio(bio);
}

static blk_qc_t ramblock_make_request(struct request_queue *queue, struct bio *bio)
{
    __ramblock_make_request(bio);
    return BLK_QC_T_NONE;
}

static int __init ramblock_init(void)
{
    int ret;
    major = register_blkdev(0, DEVICE_NAME);
    ramblock_buf = kmalloc(RAM_BLOCK_SIZE, GFP_KERNEL);
    ret = -ENOMEM;
    ramblock_gendisk = alloc_disk(1);
    if (!ramblock_gendisk)
        goto out_disk;
    ramblock_queue = blk_alloc_queue(GFP_KERNEL);
    if (IS_ERR(ramblock_queue)) {
        ret = PTR_ERR(ramblock_queue);
        ramblock_queue = NULL;
        goto out_queue;
    }
    blk_queue_make_request(ramblock_queue, ramblock_make_request);
    ramblock_gendisk->major = major;
    ramblock_gendisk->first_minor = 0;
    ramblock_gendisk->fops = &ramblock_fops;
    sprintf(ramblock_gendisk->disk_name, "ramblock");
    ramblock_gendisk->queue = ramblock_queue;
    add_disk(ramblock_gendisk);
    blk_register_region(MKDEV(major, 0), 0, THIS_MODULE, ramblock_find, NULL, NULL);
    set_capacity(ramblock_gendisk, RAM_BLOCK_SIZE >> 9);
    return 0;

out_queue:
    put_disk(ramblock_gendisk);
out_disk:
    unregister_blkdev(major, DEVICE_NAME);
    return ret;
}

static void __exit ramblock_exit(void)
{
    blk_unregister_region(MKDEV(major, 0), RAMBLOCK_MINOR_CNT);
    unregister_blkdev(major, DEVICE_NAME);
    del_gendisk(ramblock_gendisk);
    put_disk(ramblock_gendisk);
    blk_cleanup_queue(ramblock_queue);
    blk_mq_free_tag_set(&tag_set);
    return;
} 

module_init(ramblock_init);
module_exit(ramblock_exit);
MODULE_LICENSE("GPL");
