#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <uapi/asm-generic/errno-base.h>

#include "bier6.h"

#define DEVICE_NAME "bier6"
#define CLASS_NAME "bier6_class"

static int bier6_major;
static struct class *bier6_class = NULL;
static struct device *bier6_device = NULL;


static int bier6_dev_read(struct seq_file *m, void *v)
{
	return 0;
}

static int bier6_dev_open(struct inode *inode, struct file *file)
{
	return single_open(file, bier6_dev_read, NULL);
}

static long bier6_dev_ioctl(struct file *filp, unsigned int cmd, unsigned long args)
{
	/*switch(cmd) {
	case IOCTL_CMD:
		printk(KERN_ALERT "\n %s \n", (char *)args);
		break;
	}*/
	return 0;
}

static int bier6_dev_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t bier6_dev_write(struct file *file, const char __user *buffer,
                              size_t count, loff_t *ppos)
{
	return -ENOMEM;
}

static struct file_operations fops = {
	.open           = bier6_dev_open,
	.release        = bier6_dev_release,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.write          = bier6_dev_write,
	.unlocked_ioctl = bier6_dev_ioctl
};

int bier6_dev_ctrl(int term)
{
	int retval = 0;

	if(term) {
		printk(KERN_ERR"failed to register device: error %d\n", bier6_major);
		goto term;
	}

	if ((bier6_major = register_chrdev(0, DEVICE_NAME, &fops)) < 0) {
		printk(KERN_ERR"failed to register device: error %d\n", bier6_major);
		retval = bier6_major;
		goto failed_chrdevreg;
	}

	if (IS_ERR((bier6_class = class_create(THIS_MODULE, CLASS_NAME)))) {
		printk(KERN_ERR"failed to register device class '%s'\n", CLASS_NAME);
		retval = PTR_ERR(bier6_class);
		goto failed_classreg;
	}

	if (IS_ERR((bier6_device = device_create(bier6_class, NULL, MKDEV(bier6_major, 0), NULL, DEVICE_NAME)))) {
		printk(KERN_ERR"failed to create device '%s'\n", DEVICE_NAME);
		retval = PTR_ERR(bier6_device);
		goto failed_devreg;
	}

	return 0;
term:
	device_unregister(bier6_device);
	device_destroy(bier6_class,MKDEV(bier6_major,0));
failed_devreg:
	class_unregister(bier6_class);
	class_destroy(bier6_class);
failed_classreg:
	unregister_chrdev(bier6_major, DEVICE_NAME);
failed_chrdevreg:
	return retval;
}

int bier6_dev_init(void)
{
	return bier6_dev_ctrl(0);
}

void bier6_dev_term(void)
{
	bier6_dev_ctrl(1);
}
