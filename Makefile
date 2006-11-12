SHELL=/bin/sh

ifndef LINUXDIR
	LINUXDIR := /root/kernel/linux-2.6.14/
endif

CFLAGS += -Wall -g

chromefb-objs := chrome_driver.o via_ramctrl.o chrome_io.o
obj-m += chromefb.o

all: modules

modules:
	make -C $(LINUXDIR) M=`pwd` modules

clean:
	rm -f *.o *.ko *~ *.mod.c .*.cmd
	rm -Rf .tmp_versions

install: modules
	make -C $(LINUXDIR) M=`pwd` modules_install
