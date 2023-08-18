KSRC ?= /lib/modules/`uname -r`/build

GDB ?= -g

all:
	make -C $(KSRC)/ M=`pwd` modules
	gcc user_app.c ${GDB} -o user_app

%:
	make -C $(KSRC)/ M=$(CURDIR) $@

obj-m +=nupa.o
