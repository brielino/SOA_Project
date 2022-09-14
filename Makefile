obj-m += multi_flow.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

install:
	insmod multi_flow.ko

uninstall:
	rmmod multi_flow