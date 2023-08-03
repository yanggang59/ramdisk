KSRC ?= /home/thomas/RK3568/rk356x_linux/kernel

all:
	make -C $(KSRC)/ M=`pwd` modules

%:
	make -C $(KSRC)/ M=$(CURDIR) $@

obj-m +=ramdisk.o