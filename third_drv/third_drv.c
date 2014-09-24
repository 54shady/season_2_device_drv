#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/cdev.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>

#define EINT	0x2
#define THIRD_DRIVER_COUNT	1
#define REGISTER_BIT_WIDTH 2
#define GPGCON 0x56000060

static int major;
static struct cdev third_drv_cdev;
static struct class *cls;
static unsigned char key_val;
static DECLARE_WAIT_QUEUE_HEAD(third_waitqueue);

/* 中断事件标志, 中断服务程序将它置1，third_drv_read将它清0 */
static volatile int ev_press = 0;

/* 这里留有一个疑点,不配置GPIO引脚为中断引脚也可以工作 */
/* There is no need to define the CONFIG_GPG */
#ifdef CONFIG_GPG	
volatile unsigned *gpgcon = NULL;
volatile unsigned *gpgdat = NULL;

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
#endif

/*
 * EINT8 --GPG0--k1
 * EINT11--GPG3--k2
 * EINT13--GPG5--k3
 * EINT14--GPG6--k4
 * EINT15--GPG7--k5
 * EINT19--GPG11--k6
 * 键值: 按下时分别自定义为: 0x01, 0x02, 0x03, 0x04 ,0x05, 0x06 
 * 键值: 松开时分别自定义为: 0x81, 0x82, 0x83, 0x84 ,0x85, 0x86 
 */
struct pin_desc{
	unsigned int INT_NO;
	char key_name[4];
	unsigned int pin;
	unsigned int key_val;
};

struct pin_desc pindesc[6] = {
	{IRQ_EINT8,  "k1", S3C2410_GPG0,   0x01},
	{IRQ_EINT11, "k2", S3C2410_GPG3,  0x02},
	{IRQ_EINT13, "k3", S3C2410_GPG5,  0x03},
	{IRQ_EINT14, "k4", S3C2410_GPG6,  0x04},
	{IRQ_EINT15, "k5", S3C2410_GPG7,  0x05},
	{IRQ_EINT19, "k6", S3C2410_GPG11, 0x06},
};

static irqreturn_t third_irq_handle(int irq, void *dev_id)
{
	struct pin_desc *pindesc = (struct pin_desc*)dev_id;
	unsigned int pinVal;

	/* 读取引脚值 */
	pinVal = s3c2410_gpio_getpin(pindesc->pin);

	/* 确定引脚值是高电平还是低电平 */
	if (pinVal)
	{
		/* pin 脚是高电平时表明按键松开 */
		key_val = 0x80 | pindesc->key_val;
	}
	else
	{
		/* pin 脚是低电平时表明按键按下 */
		key_val = pindesc->key_val;
	}

	ev_press = 1;
    wake_up_interruptible(&third_waitqueue);   /* 唤醒休眠的进程 */

	return IRQ_HANDLED;
}

static int third_drv_open(struct inode *inode, struct file *file)
{
	int i;
#ifdef CONFIG_GPG
	*gpgcon &= ~(EINT << REGISTER_BIT_WIDTH*GPG0) | \
				(EINT << REGISTER_BIT_WIDTH*GPG3) | \
				(EINT << REGISTER_BIT_WIDTH*GPG5) | \
				(EINT << REGISTER_BIT_WIDTH*GPG6) | \
				(EINT << REGISTER_BIT_WIDTH*GPG7) | \
				(EINT << REGISTER_BIT_WIDTH*GPG11);
#endif
	for (i = 0; i < sizeof(pindesc) / sizeof(pindesc[0]); i++)
	{
		if (request_irq(pindesc[i].INT_NO, third_irq_handle, IRQT_BOTHEDGE, pindesc[i].key_name, &pindesc[i])) {
			printk(KERN_ERR "Unable to request interrupt %d\n", pindesc[i].INT_NO);
			return -EINVAL;
		}
	}

	return 0;
}

ssize_t third_drv_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{

	if (size != 1)
		return -EINVAL;

	wait_event_interruptible(third_waitqueue, ev_press);

	if (copy_to_user(buf, &key_val, 1))
		return -EINVAL;

	ev_press = 0;

	return 1;
}

static int third_drv_release(struct inode *inode, struct file *file)
{
	int i;

	for (i = 0; i < sizeof(pindesc) / sizeof(pindesc[0]); i++)
		free_irq(pindesc[i].INT_NO,  &pindesc[i]);

	return 0;
}

static struct file_operations third_drv_fops = {
    .owner  =   THIS_MODULE,
    .open   =   third_drv_open,     
    .read	= 	third_drv_read,
	.release =  third_drv_release,
};

static int third_drv_init(void)
{
	dev_t dev_id;
	int retval;
	
	if (major) {
		dev_id = MKDEV(major, 0);
		retval = register_chrdev_region(dev_id, THIRD_DRIVER_COUNT, "third");
	} else {
		retval = alloc_chrdev_region(&dev_id, 0, THIRD_DRIVER_COUNT, "third");
		major = MAJOR(dev_id);
	}

	if (retval) {
		printk("register or alloc region error\n");
		return -1;
	}

	cdev_init(&third_drv_cdev, &third_drv_fops);
	cdev_add(&third_drv_cdev, dev_id, THIRD_DRIVER_COUNT);

	cls = class_create(THIS_MODULE, "third_drv");
	if (IS_ERR(cls))
		return -EINVAL;
	
	class_device_create(cls, NULL, MKDEV(major, 0), NULL, "buttons");/* /dev/buttons */

#ifdef CONFIG_GPG
	gpgcon = ioremap(GPGCON, 4);
	if (!gpgcon) {
		printk(KERN_ERR "unable to remap memory\n");
		retval = -EAGAIN;
	}

	gpgdat = gpgcon + 1;
#endif	

	return 0;
}

static void third_drv_exit(void)
{
	dev_t dev_id = MKDEV(major, 0);

#ifdef CONFIG_GPG
	iounmap(gpgcon);
#endif
	class_device_destroy(cls, dev_id);
	class_destroy(cls);
	cdev_del(&third_drv_cdev);
	unregister_chrdev_region(dev_id, THIRD_DRIVER_COUNT);
}

module_init(third_drv_init);
module_exit(third_drv_exit);
MODULE_LICENSE("GPL");
