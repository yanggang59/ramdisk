KSRC ?= /lib/modules/`uname -r`/build

GDB ?= -g

all:
	make -C $(KSRC)/ M=`pwd` modules

%:
	make -C $(KSRC)/ M=$(CURDIR) $@

obj-m +=nupa.o
