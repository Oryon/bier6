#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/inet.h>
#include <uapi/asm-generic/errno-base.h>

#include "bier6.h"

#define DEVICE_NAME "bier6"
#define CLASS_NAME "bier6_class"

static int bier6_major;
static struct class *bier6_class = NULL;
static struct device *bier6_device = NULL;
static struct bier6_device *bier6_dev = NULL;

int ipv6_parse_prefix(const char *str, struct in6_addr *prefix, u8 *plen) {
	unsigned long int i;
	const char *str_plen = NULL;
	if(plen) {
		if((str_plen = strchr(str, '/'))) {
			if(kstrtoul(str_plen + 1, 10, &i) < 0 || i > 128)
				return -EINVAL;

			*plen = i;
		} else {
			*plen = 128;
		}
	}

	if(1 != in6_pton(str, str_plen?((int) (str_plen - str)):strlen(str), (u8 *)prefix, -1, NULL))
		return -EINVAL;

	return 0;
}

char *next_token(char *str, char *end) {
	while(str != end && *str == '\0')
		++str;
	while(str != end && *str != '\0')
		++str;
	while(str != end && *str == '\0')
		++str;
	return (str == end) ? NULL : str;
}

static int bier6_dev_read(struct seq_file *m, void *v)
{
	return bier6_fib_dump(bier6_dev, m);
}

static int bier6_dev_open(struct inode *inode, struct file *file)
{
	return single_open(file, bier6_dev_read, NULL);
}

static ssize_t bier6_dev_write(struct file *file, const char __user *buffer,
					size_t count, loff_t *ppos)
{
	char *buf = NULL;
	char *i;
	char *tail = NULL;
	struct in6_addr addr;
	unsigned long int bit;

	if (!(buf = kmalloc(sizeof(char) * (count + 1), GFP_KERNEL)))
		return -ENOMEM;

	if (copy_from_user(buf, buffer, count)) {
		kfree(buf);
		return -EFAULT;
	}
	tail = buf + count;
	*tail = '\0';

	for(i=buf; i!=tail; i++)
		if(*i == ' ' || *i == '\n')
			*i = '\0';

	i = buf;
	if(!strcmp(i, "bit")) {
		if((i = next_token(i, tail)) &&
				!kstrtoul(i, 10, &bit) &&
				bit >= 0 && bit < RIB_BITS &&
				(i = next_token(i, tail))) {
			if(!strcmp(i, "null")) {
				bier6_fib_set_bit(bier6_dev, bit, NULL);
			} else if(!ipv6_parse_prefix(i, &addr, NULL)) {
				bier6_fib_set_bit(bier6_dev, bit, &addr);
			} else {
				goto err;
			}
		} else {
			goto err;
		}
	} else {
		goto err;
	}

	kfree(buf);
	return count;
err:
	kfree(buf);
	return -EIO;
}

static struct file_operations fops = {
	.open           = bier6_dev_open,
	.release        = single_release,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.write          = bier6_dev_write
};

int bier6_dev_ctrl(int term)
{
	int retval = 0;

	if(term)
		goto term;

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

int bier6_dev_init(struct bier6_device *dev)
{
	bier6_dev = dev;
	return bier6_dev_ctrl(0);
}

void bier6_dev_term(void)
{
	bier6_dev_ctrl(1);
}
