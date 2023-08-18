#ifndef __NUPA_H_
#define __NUPA_H_

#include "queue.h"

/**
* Note: This is the public header for both driver and app
*/

#define RESERVE_MEM_SIZE                                (0x100000000UL)
#define NUPA_BLOCK_SIZE                                 (unsigned long)(4UL * 1024UL * 1024UL)
#define NUPA_BLOCK_SIZE_MASK                            (NUPA_BLOCK_SIZE -1)
#define NUPA_META_DATA_LEN                              (24UL * (NUPA_BLOCK_SIZE))
#define NUPA_DATA_SIZE                                  (unsigned long)(RESERVE_MEM_SIZE - NUPA_META_DATA_LEN)
#define CACHE_BLOCK_NUM                                 (NUPA_DATA_SIZE / NUPA_BLOCK_SIZE)
#define BIT_PER_BYTE                                    8 
#define BITMAP_SIZE                                     ((CACHE_BLOCK_NUM + BIT_PER_BYTE -1 )/BIT_PER_BYTE)
#define LOCAL_RAMDISK_TEST                              0
#if LOCAL_RAMDISK_TEST
#define NUPA_DISK_SIZE                                  (1024UL * 1024UL)
#else
#define NUPA_DISK_SIZE                                  (unsigned long)RESERVE_MEM_SIZE
#endif
#define INVALID_ADDR                                    ULONG_MAX


struct nupa_meta_info_header {
    char dirty_bit_map[BITMAP_SIZE];
    unsigned long vb[CACHE_BLOCK_NUM];
};

/**************************************************subq and comq*************************************************/

#define QUEUE_SIZE          128
enum req_type { 
    REQ_READ = 1, 
    REQ_WRITE,  
};

struct nupa_queue_entry {
    unsigned long pb;
    enum req_type req; 
};

static bool is_vb_dirty(unsigned long vb, char* dirty_bit_map)
{
    unsigned long byte = vb / sizeof(unsigned long);
    unsigned long offset = vb % sizeof(unsigned long);
    char value = dirty_bit_map[byte];
    return (value & (1 << offset));
}

static void set_vb_dirty(unsigned long vb, char* dirty_bit_map)
{
    unsigned long byte = vb / sizeof(unsigned long);
    unsigned long offset = vb % sizeof(unsigned long);
    dirty_bit_map[byte] |= 1 << offset;
}

#ifdef USER_APP
static void clr_vb_dirty(unsigned long vb, char* dirty_bit_map)
{
    unsigned long byte = vb / sizeof(unsigned long);
    unsigned long offset = vb % sizeof(unsigned long);
    dirty_bit_map[byte] &= ~(1 << offset);
}
#endif

#endif