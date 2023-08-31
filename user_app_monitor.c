#include <stdio.h>  
#include <fcntl.h>  
#include <stdlib.h>  
#include <unistd.h>  
#include <sys/mman.h>  
#include <errno.h> 
#include <string.h>
#include <stdbool.h>
#include <signal.h>


#define UIO_DEV           "/dev/uio0"  
#define UIO_ADDR          "/sys/class/uio/uio0/maps/map0/addr"  
#define UIO_SIZE          "/sys/class/uio/uio0/maps/map0/size"
#define DEBUG              1
#define REG_CC_ENABLE_BIT   0

static char uio_addr_buf[30], uio_size_buf[30];

struct nupa_meta_info_header* g_meta_info_header;
struct queue *g_nupa_sub_queue;
struct queue *g_nupa_com_queue;

static void* g_data_buf;
int g_interrupt = 0;

enum nvme_ctrl_state {
	NVME_CTRL_NEW,
	NVME_CTRL_LIVE,
	NVME_CTRL_ADMIN_ONLY,    /* Only admin queue live */
	NVME_CTRL_RESETTING,
	NVME_CTRL_CONNECTING,
	NVME_CTRL_DELETING,
	NVME_CTRL_DEAD,
};


enum {
	NVME_REG_CAP	= 0x0000,	/* Controller Capabilities */
	NVME_REG_VS	= 0x0008,	/* Version */
	NVME_REG_INTMS	= 0x000c,	/* Interrupt Mask Set */
	NVME_REG_INTMC	= 0x0010,	/* Interrupt Mask Clear */
	NVME_REG_CC	= 0x0014,	/* Controller Configuration */
	NVME_REG_CSTS	= 0x001c,	/* Controller Status */
	NVME_REG_NSSR	= 0x0020,	/* NVM Subsystem Reset */
	NVME_REG_AQA	= 0x0024,	/* Admin Queue Attributes */
	NVME_REG_ASQ	= 0x0028,	/* Admin SQ Base Address */
	NVME_REG_ACQ	= 0x0030,	/* Admin CQ Base Address */
	NVME_REG_CMBLOC = 0x0038,	/* Controller Memory Buffer Location */
	NVME_REG_CMBSZ	= 0x003c,	/* Controller Memory Buffer Size */
	NVME_REG_DBS	= 0x1000,	/* SQ 0 Tail Doorbell */
};

static void sigcb(int signo) 
{
    switch(signo) {
    case SIGHUP:
        printf("Get a signal -- SIGHUP\n");
        break;
    case SIGINT://Ctrl+C
        printf("Get a signal -- SIGINT\n");
        g_interrupt = 1;
        break;
    case SIGQUIT:
        printf("Get a signal -- SIGQUIT\n");
        break;
    }
    return;
}

int main(void)  
{  
    int uio_fd, addr_fd, size_fd, storage_fd;  
    long uio_size;  
    void* uio_addr, *access_address;
    off_t offset = 0; 
    enum nvme_ctrl_stats state = NVME_CTRL_RESETTING;

    signal(SIGHUP, sigcb);
    signal(SIGINT, sigcb);
    signal(SIGQUIT, sigcb);
   
    uio_fd = open(UIO_DEV, /*O_RDONLY*/O_RDWR);  
    addr_fd = open(UIO_ADDR, O_RDONLY);  
    size_fd = open(UIO_SIZE, O_RDONLY);
    if( addr_fd < 0 || size_fd < 0 || uio_fd < 0 || storage_fd < 0) {  
        fprintf(stderr, "open: %s\n", strerror(errno));  
        exit(-1);  
    }  

    g_data_buf = access_address = mmap(NULL, 4 * 1024 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, uio_fd, 0);  
    if ( access_address == (void*) -1) {  
        fprintf(stderr, "mmap: %s\n", strerror(errno));  
        exit(-1);  
    }  
    printf("The device address %p (lenth 0x%lX)\n"  
         "can be accessed over\n"  
         "logical address %p\n", uio_addr, uio_size, access_address); 
// #if DEBUG
//     print_buf(access_address, 256);
// #endif
    for (int i = 0; i < 512; i++) {
        printf("%d : 0x%x  \r\n", i, (int)(((int*)access_address)[i]));
    }
    signal(SIGHUP, sigcb);
    signal(SIGINT, sigcb);
    signal(SIGQUIT, sigcb);

    while (1) {
        switch (state) {
        case NVME_CTRL_RESETTING:

            state = NVME_CTRL_CONNECTING;
            break;
        case NVME_CTRL_CONNECTING:
            break;
        case NVME_CTRL_DELETING:
            break;
        }
    }

    while(1) {
        if(g_interrupt) {
            break;
        }
        usleep(1000000);
        int cc = ((int*)access_address)[0x14/sizeof(int)];
        int csts = ((int*)access_address)[0x1c/sizeof(int)];
        printf("[Info] cc = %x , csts = %x \r\n", cc, csts);
        if(cc & (1 << REG_CC_ENABLE_BIT)) {
            csts |= (1 << 0);
            ((int*)access_address)[0x1c/sizeof(int)] = csts;
        }else {
            csts &= ~(1 << 0);
            ((int*)access_address)[0x1c/sizeof(int)] = csts;
        }

    }

    /**
    * For UIO
    */
    close(uio_fd);
    close(addr_fd);
    close(size_fd);
    return 0;  
}  