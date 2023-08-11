#ifndef __NUPA_H_
#define __NUPA_H_
#define DEBUG                                           1
#define DEVICE_NAME                                     "nupa"
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

struct nupa_dev {
	struct device dev; 
	struct gendisk *nupa_gendisk;
	void* nupa_buf;
	int major;
};
#endif