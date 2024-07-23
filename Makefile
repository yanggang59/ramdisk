SHELL = /bin/bash

ifneq ($(version), 5.15)
	KSRC ?= /home/ubuntu/workspace/debug_linux5.0_x86
	obj-m += ramdisk_5.0.o
else
	KSRC ?= /lib/modules/`uname -r`/build
	obj-m += ramdisk_5.15.o
endif

GDB ?= -g

all:
	make -C $(KSRC)/ M=`pwd` modules
%:
	make -C $(KSRC)/ M=$(CURDIR) $@
