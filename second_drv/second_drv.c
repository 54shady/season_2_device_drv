#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/cdev.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>

static int major;
static struct cdev second_drv_cdev;
static struct class *cls;
volatile unsigned long *gpgcon = NULL;
volatile unsigned long *gpgdat = NULL;
/*
 * EINT8 --GPG0--k1
 * EINT11--GPG3--k2
 * EINT13--GPG5--k3
 * EINT14--GPG6--k4
 * EINT15--GPG7--k5
 * EINT19--GPG11--k6
 */
#define INPUT	0x3
#define SECOND_DRIVER_COUNT	1
#define REGISTER_BIT_WIDTH 2
#define GPGCON	0x56000060

enum
{
	GPG0,
	GPG1,
	GPG2,
	GPG3,
	GPG4,
	GPG5,
	GPG6,
	GPG7,
	GPG8,
	GPG9,
	GPG10,
	GPG11,
	GPG12,
	GPG13,
	GPG14,
	GPG15,
};

static int second_drv_open(struct inode *inode, struct file *file)
{
	/* 设置引脚为输入引脚 */
	*gpgcon &= ~(INPUT << REGISTER_BIT_WIDTH*GPG0) | \
				(INPUT << REGISTER_BIT_WIDTH*GPG3) | \
				(INPUT << REGISTER_BIT_WIDTH*GPG5) | \
				(INPUT << REGISTER_BIT_WIDTH*GPG6) | \
				(INPUT << REGISTER_BIT_WIDTH*GPG7) | \
				(INPUT << REGISTER_BIT_WIDTH*GPG11);
	
	return 0;
}

static ssize_t second_drv_read(struct file * file, char __user * buf, size_t count, loff_t * off)
{
	/* 返回6个引脚的电平 */
	unsigned char key_vals[6];
	int regval;

	if (count != sizeof(key_vals))
		return -EINVAL;

	regval = *gpgdat;
	key_vals[0] = (regval & (1<<0)) ? 1 : 0;
	key_vals[1] = (regval & (1<<3)) ? 1 : 0;
	key_vals[2] = (regval & (1<<5)) ? 1 : 0;
	key_vals[3] = (regval & (1<<6)) ? 1 : 0;
	key_vals[4] = (regval & (1<<7)) ? 1 : 0;
	key_vals[5] = (regval & (1<<11)) ? 1 : 0;

	if (copy_to_user(buf, key_vals, sizeof(key_vals)) == 0)
		return 0;

	return sizeof(key_vals);
}

static struct file_operations second_drv_fops = {
    .owner  =   THIS_MODULE,
    .open   =   second_drv_open,     
    .read	= 	second_drv_read,
};

static int second_drv_init(void)
{
	dev_t dev_id;
	int retval;
	
	if (major) {
		dev_id = MKDEV(major, 0);
		retval = register_chrdev_region(dev_id, SECOND_DRIVER_COUNT, "second");
	} else {
		retval = alloc_chrdev_region(&dev_id, 0, SECOND_DRIVER_COUNT, "second");
		major = MAJOR(dev_id);
	}

	if (retval) {
		printk("register or alloc region error\n");
		return -1;
	}

	cdev_init(&second_drv_cdev, &second_drv_fops);
	cdev_add(&second_drv_cdev, dev_id, SECOND_DRIVER_COUNT);

	cls = class_create(THIS_MODULE, "second_drv");
	if (IS_ERR(cls))
		return -EINVAL;
	
	class_device_create(cls, NULL, MKDEV(major, 0), NULL, "buttons");/* /dev/buttons */

	gpgcon = ioremap(GPGCON, 4);
	if (!gpgcon) {
		printk(KERN_ERR "unable to remap memory\n");
		retval = -EAGAIN;
	}

	gpgdat = gpgcon + 1;
	
	return 0;
}

static void second_drv_exit(void)
{
	dev_t dev_id = MKDEV(major, 0);

	iounmap(gpgcon);
	class_device_destroy(cls, dev_id);
	class_destroy(cls);
	cdev_del(&second_drv_cdev);
	unregister_chrdev_region(dev_id, SECOND_DRIVER_COUNT);
}

module_init(second_drv_init);
module_exit(second_drv_exit);
MODULE_LICENSE("GPL");
