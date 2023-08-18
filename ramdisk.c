#include <linux/major.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/blk-mq.h>
#include <linux/bitops.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/hdreg.h>


#define DEVICE_NAME "ramdisk"

#define RAMDISK_SIZE 1024*1024*2

static DEFINE_SPINLOCK(ramdisk_lock);

static struct gendisk *ramdisk_gendisk;
static struct request_queue *ramdisk_queue;

static int major;
static char* ramdisk_buf;


static int ramdisk_getgeo(struct block_device *dev, struct hd_geometry *geo)
{
    /* 这是相对于机械硬盘的概念 */
    geo->heads = 2;            /* 磁头 */
    geo->cylinders = 32;    /* 柱面 */
    geo->sectors = RAMDISK_SIZE / (2 * 32 *512); /* 一个磁道上的扇区数量 */
    return 0;
}


static const struct block_device_operations ramdisk_fops =
{
    .owner        = THIS_MODULE,
    .getgeo = ramdisk_getgeo,
};

static void ramdisk_transfer(struct request *req)
{    
    unsigned long start = blk_rq_pos(req) << 9;      /* blk_rq_pos获取到的是扇区地址，左移9位转换为字节地址 */
    unsigned long len  = blk_rq_cur_bytes(req);        /* 大小   */

    static int w_cnt = 0;
    static int r_cnt = 0;
    void *buffer = bio_data(req->bio);        
    
    if(rq_data_dir(req) == READ) {
        printk("[Info] do_ramdisk_request read %d \r\n", ++r_cnt);
        memcpy(buffer, ramdisk_buf + start, len);
    } else if(rq_data_dir(req) == WRITE) {
        printk("[Info] do_ramdisk_request write %d \r\n", ++w_cnt);
        memcpy(ramdisk_buf + start, buffer, len);
    }

}


static void do_ramdisk_request(struct request_queue *q)
{
    int err = 0;
    struct request *req;

    req = blk_fetch_request(q);
    while(req != NULL) {

        ramdisk_transfer(req);

        if (!__blk_end_request_cur(req, err))
            req = blk_fetch_request(q);
    }
}


static int __init ramdisk_init(void)
{
    ramdisk_gendisk = alloc_disk(16);

    ramdisk_queue = blk_init_queue(do_ramdisk_request, &ramdisk_lock);

    major = register_blkdev(0, DEVICE_NAME);

    ramdisk_gendisk->major = major;
    ramdisk_gendisk->first_minor = 0;
    ramdisk_gendisk->fops = &ramdisk_fops;
    sprintf(ramdisk_gendisk->disk_name, "ramdisk");

    ramdisk_gendisk->queue = ramdisk_queue;

    add_disk(ramdisk_gendisk);
    set_capacity(ramdisk_gendisk, RAMDISK_SIZE/512);

    ramdisk_buf = kzalloc(RAMDISK_SIZE, GFP_KERNEL);

    return 0;
}


static void __exit ramdisk_exit(void)
{
    unregister_blkdev(major, DEVICE_NAME);
    del_gendisk(ramdisk_gendisk);
    put_disk(ramdisk_gendisk);
    blk_cleanup_queue(ramdisk_queue);
}


module_init(ramdisk_init);
module_exit(ramdisk_exit);
MODULE_LICENSE("GPL");
