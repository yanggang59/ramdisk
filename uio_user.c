#include <stdio.h>  
#include <fcntl.h>  
#include <stdlib.h>  
#include <unistd.h>  
#include <sys/mman.h>  
#include <errno.h> 
#include <string.h> 
  
#define UIO_DEV "/dev/uio0"  
#define UIO_ADDR "/sys/class/uio/uio0/maps/map0/addr"  
#define UIO_SIZE "/sys/class/uio/uio0/maps/map0/size"  
  
static char uio_addr_buf[30], uio_size_buf[30];

#define PRINT printf
static void print_buf(char* buf, int size)
{
	int i ,j;
  PRINT("print size = %d \r\n", size);
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
  
int main(void)  
{  
  int uio_fd, addr_fd, size_fd;  
  long uio_size;  
  void* uio_addr, *access_address;  
   
  uio_fd = open(UIO_DEV, /*O_RDONLY*/O_RDWR);  
  addr_fd = open(UIO_ADDR, O_RDONLY);  
  size_fd = open(UIO_SIZE, O_RDONLY);  
  if( addr_fd < 0 || size_fd < 0 || uio_fd < 0) {  
       fprintf(stderr, "open: %s\n", strerror(errno));  
       exit(-1);  
  }  
  read(addr_fd, uio_addr_buf, sizeof(uio_addr_buf));  
  read(size_fd, uio_size_buf, sizeof(uio_size_buf));  
  uio_addr = (void*)strtoul(uio_addr_buf, NULL, 0);  
  uio_size = (long)strtol(uio_size_buf, NULL, 0);
  printf("uiofd = %d \r\n", uio_fd);
  printf(" uio_addr_buf = %s\n uio_size_buf = %s\n uio_addr = %p\n uio_size = 0x%lX\n", uio_addr_buf, uio_size_buf, uio_addr, uio_size);
  access_address = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, uio_fd, 0);  
  if ( access_address == (void*) -1) {  
      fprintf(stderr, "mmap: %s\n", strerror(errno));  
      exit(-1);  
  }  
  printf("The device address %p (lenth 0x%lX)\n"  
         "can be accessed over\n"  
         "logical address %p\n", uio_addr, uio_size, access_address);  
  print_buf(access_address, 4096);
  //munmap(access_address, uio_size);
  close(uio_fd);
  close(addr_fd);
  close(size_fd);
  return 0;  
}  