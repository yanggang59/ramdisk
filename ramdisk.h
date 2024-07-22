#ifndef __PCI_DISK_H__
#define __PCI_DISK_H__

#include <linux/types.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/poll.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/aio.h>
#include <linux/splice.h>
#include <linux/version.h>
#include <linux/spinlock_types.h>
#include <linux/stddef.h>
#include <asm/byteorder.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/string.h>
#include <linux/pagemap.h>
#include <linux/bitops.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <linux/capability.h>
#include <linux/list.h>
#include <linux/reboot.h>
#include <net/checksum.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>
#include <linux/libnvdimm.h>


#undef DEBUG_THIS_MODULE
#define DEBUG_THIS_MODULE                 1
#if DEBUG_THIS_MODULE
#define NUPA_DEBUG(fmt,...)               printk("[NUPA DEBUG] "fmt, ##__VA_ARGS__)
#define NUPA_ERROR(fmt,...)               printk("[NUPA ERROR] "fmt, ##__VA_ARGS__)
#else
#define NUPA_DEBUG(fmt,...)
#define NUPA_ERROR(fmt,...)
#endif

struct ramdisk_dev {
    struct device dev;
    struct gendisk *disk;
    void* virt_addr;
    unsigned long long size;
    int disk_major;
};

#endif
