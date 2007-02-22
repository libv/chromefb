SHELL=/bin/sh

ifndef LINUXDIR
	LINUXDIR := /usr/src/linux-source/
endif

CFLAGS += -Wall -g -O0

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
