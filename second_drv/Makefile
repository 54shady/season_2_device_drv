KERN_DIR = /opt/FriendlyARM/mini2440/linux-2.6.22.6_jz2440/

all:
	make -C $(KERN_DIR) M=`pwd` modules 
	arm-linux-gcc -otest test.c
clean:
	make -C $(KERN_DIR) M=`pwd` modules clean
	rm -rf modules.order Module.symvers test


obj-m	+= second_drv.o
