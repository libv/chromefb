SHELL=/bin/sh

ifndef LINUXDIR
	LINUXDIR := /usr/src/linux-source-2.6.18/
endif

CFLAGS += -Wall -g

chromefb-objs := chrome_driver.o chrome_host.o chrome_io.o chrome_mode.o
obj-m += chromefb.o

all: modules

modules:
	make -C $(LINUXDIR) M=`pwd` modules

clean:
	rm -f *.o *.ko *~ *.mod.c .*.cmd
	rm -Rf .tmp_versions

install: modules
	make -C $(LINUXDIR) M=`pwd` modules_install
