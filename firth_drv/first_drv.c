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
#include <linux/device.h>

#define FIRST_DRIVER_COUNT	1

static int major;
static struct cdev first_drv_cdev;
static struct class *cls;
volatile unsigned long *gpbcon = NULL;
volatile unsigned long *gpbdat = NULL;

struct my_led
{
	char swich;
	char name;
};

static int first_drv_open(struct inode *inode, struct file *file)
{
	*gpbcon &= ~((0x3<<(5*2)) | (0x3<<(6*2)) | (0x3<<(7*2)) | (0x3<<(8*2)));
	*gpbcon |= ((0x1<<(5*2)) | (0x1<<(6*2)) | (0x1<<(7*2)) | (0x1<<(8*2)));

	return 0;
}

static ssize_t first_drv_write(struct file *file, const char __user *buf, size_t count, loff_t * ppos)
{
	struct my_led ml;

	if (copy_from_user(&ml, buf, count) != 0)
		return -1;

#ifdef DEBUG	
	printk("first_drv_write, ml.swich = %d, ml.name = %d\n", ml.swich, ml.name);
#endif

	if (ml.swich == 1)
	{
		switch (ml.name)
		{
			case 1:
				*gpbdat &= ~(1 << 5);
				break;
			case 2:
				*gpbdat &= ~(1 << 6);
				break;
			case 3:
				*gpbdat &= ~(1 << 7);
				break;
			case 4:
				*gpbdat &= ~(1 << 8);
				break;
			case 5:
				*gpbdat &= ~((1<<5) | (1<<6) | (1<<7) | (1<<8));
				break;
			default:
				printk("Error\n");
				break;
		};
	}
	else
	{
		switch (ml.name)
		{
			case 1:
				*gpbdat |= (1 << 5);
				break;
			case 2:
				*gpbdat |= (1 << 6);
				break;
			case 3:
				*gpbdat |= (1 << 7);
				break;
			case 4:
				*gpbdat |= (1 << 8);
				break;
			case 5:
				*gpbdat |= (1<<5) | (1<<6) | (1<<7) | (1<<8);
				break;
			default:
				printk("Error\n");
				break;
		};
	}
		
	return 0;
}

static struct file_operations first_drv_fops = {
	.owner	= THIS_MODULE,
	.open 	= first_drv_open,
	.write  = first_drv_write,
};

static int __init first_drv_init(void)
{
	dev_t dev_id;
	int retval;

	/* 分配主编号 */
	if (major) {
		dev_id = MKDEV(major, 0);
		retval = register_chrdev_region(dev_id, FIRST_DRIVER_COUNT, "first");
	} else {
		retval = alloc_chrdev_region(&dev_id, 0, FIRST_DRIVER_COUNT, "first");
		major = MAJOR(dev_id);
	}

	if (retval) {
		printk("register or alloc region error\n");
		return -1;
	}

	/* 字符设备的注册 */
	/* 内核在内部使用类型 struct cdev 的结构来代表字符设备 */

	/* 初始化 */
	cdev_init(&first_drv_cdev, &first_drv_fops);

	/* 注册到内核 */
	cdev_add(&first_drv_cdev, dev_id, FIRST_DRIVER_COUNT);

	cls = class_create(THIS_MODULE, "first_drv");
	if (IS_ERR(cls))
		return -EINVAL;
	
	class_device_create(cls, NULL, MKDEV(major, 0), NULL, "leds");/* /dev/leds */

	gpbcon = ioremap(0x56000010, 4);
	if (!gpbcon) {
		printk(KERN_ERR "unable to remap memory\n");
		retval = -EAGAIN;
	}

	gpbdat = gpbcon + 1;
	
	return 0;
}

static void __exit first_drv_exit(void)
{
	dev_t dev_id = MKDEV(major, 0);

	iounmap(gpbcon);
	class_device_destroy(cls, dev_id);
	class_destroy(cls);
	cdev_del(&first_drv_cdev);
	unregister_chrdev_region(dev_id, FIRST_DRIVER_COUNT);
}

module_init(first_drv_init);
module_exit(first_drv_exit);
MODULE_LICENSE("GPL");
