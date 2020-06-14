EXTRA_CFLAGS += -I$(obj) -I$(obj)/../include
obj-m += vscsihba.o
vscsihba-objs := hba.o device.o

all:	modules

modules:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

