#KSRC ?= /lib/modules/`uname -r`/build
KSRC ?= /home/ubuntu/workspace/debug_linux5.0_x86

GDB ?= -g

all:
	make -C $(KSRC)/ M=`pwd` modules
%:
	make -C $(KSRC)/ M=$(CURDIR) $@

obj-m += ramdisk_5.0.o
