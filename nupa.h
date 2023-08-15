#ifndef __NUPA_H_
#define __NUPA_H_


#define DEBUG                                           1
#define DEVICE_NAME                                     "nupa"
#define RESERVE_MEM_START                               (0x100000000)
#define RESERVE_MEM_SIZE                                (0x100000000)
#define NUPA_BLOCK_SIZE                                 (4 * 1024 * 1024)
#define NUPA_BLOCK_SIZE_MASK                            (NUPA_BLOCK_SIZE -1)
#define NUPA_META_DATA_LEN                              (24 * (NUPA_BLOCK_SIZE))
#define NUPA_DATA_SIZE                                  (RESERVE_MEM_SIZE - NUPA_META_DATA_LEN)
#define CACHE_BLOCK_NUM                                 (NUPA_DATA_SIZE / NUPA_BLOCK_SIZE)
#define BITMAP_SIZE                                     ((CACHE_BLOCK_NUM + sizeof(unsigned long) -1 )/sizeof(unsigned long))
#define LOCAL_RAMDISK_TEST                              0
#if LOCAL_RAMDISK_TEST
#define NUPA_DISK_SIZE                                  (1024 * 1024)
#else
#define NUPA_DISK_SIZE                                  RESERVE_MEM_SIZE
#endif
#define INVALID_ADDR                                    ULONG_MAX


#define SECTORS_PER_PAGE_SHIFT	                        (PAGE_SHIFT - SECTOR_SHIFT)
#define SECTORS_PER_PAGE	                            (1 << SECTORS_PER_PAGE_SHIFT)


#define CONFIG_SUPPORT_WAF                              1

struct nupa_dev {
	struct device dev; 
	struct gendisk *nupa_gendisk;
	void* nupa_buf;
	int major;
	long virtual_capacity;
};

struct nupa_meta_info_header {
	unsigned long dirty_bit_map[BITMAP_SIZE];
	unsigned long vb[CACHE_BLOCK_NUM];
};

/**********************************************************************/

#define QUEUE_SIZE	      128

struct nupa_queue_entry{
	unsigned long pb;
	bool is_write; 
};

#endif