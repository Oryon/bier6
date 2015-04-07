#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/inet.h>
#include <uapi/asm-generic/errno-base.h>

#include "bier6.h"

#define DEVICE_NAME "bier6"
#define CLASS_NAME "bier6_class"

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

static void prefix_canonize(struct in6_addr *p, u8 plen)
{
	u8 b = plen/8;
	u8 r = plen%8;
	if(b < 15)
		memset(((u8*)p) + b + 1, 0, 16 - 1 - b);
	((u8*)p)[b] &= (0xff << (8 - r));
}

int tokenize(char *str, char **tokens, int len)
{
	int tok = 0;
	char *c = str;
	int skip_word = 0;
	while(*c != '\0') {
		if(skip_word) {
			if(*c == ' ' || *c == '\n') {
				*c = '\0';
				skip_word = 0;
			}
		} else {
			if(*c == ' ' || *c == '\n') {

			} else if(tok == len) {
				return -EFBIG;
			} else { //Found the next word
				tokens[tok] = c;
				tok++;
				skip_word = 1;
			}
		}
		c++;
	}

	for(;tok < len; tok++) {
		tokens[tok] = NULL;
	}
	return 0;
}

static int bier6_dev_read(struct seq_file *m, void *v)
{
	printk("bier6_dev_read\n");
	return bier6_rib_dump((struct bier6 *) m->private, m);
}

static int bier6_dev_open(struct inode *inode, struct file *file)
{
	struct bier6 *b = container_of(inode->i_cdev, struct bier6, cdev);
	return single_open(file, bier6_dev_read, b);
}

static ssize_t bier6_dev_write(struct file *file, const char __user *buffer,
					size_t count, loff_t *ppos)
{
	struct bier6 *b = ((struct seq_file *)file->private_data)->private;
	char *buf = NULL;
	char *tokens[10];
	struct in6_addr prefix, addr;
	unsigned long int bit;
	struct bier6_prefix *p;
	u8 create, plen;
	int err = 0;

	if (!(buf = kmalloc(sizeof(char) * (count + 1), GFP_KERNEL)))
		return -ENOMEM;

	if (copy_from_user(buf, buffer, count)) {
		kfree(buf);
		return -EFAULT;
	}
	buf[count] = '\0';

	if (tokenize(buf, tokens, 10) || !tokens[1] ||
			strcmp(tokens[0], "rib")) {
		err = -EINVAL;
		goto out;
	}

	if(!strcmp(tokens[1], "add") || !strcmp(tokens[1], "del")) {
		if(!tokens[2] || ipv6_parse_prefix(tokens[2], &prefix, &plen)) {
			err = -EINVAL;
			goto out;
		}

		create = !strcmp(tokens[1], "add");
		prefix_canonize(&prefix, plen);
		if(!(p = bier6_rib_prefix_goc(b, &prefix, plen, create))) {
			err = create?-ENOMEM:0;
			goto out;
		}

		if(!create)
			bier6_rib_prefix_del(b, p);

	} else if(!strcmp(tokens[1], "set")) {
		if(!tokens[4] ||
				ipv6_parse_prefix(tokens[2], &prefix, &plen) ||
				kstrtoul(tokens[3], 10, &bit) || bit + plen >= 128 ||
				((create = !!strcmp(tokens[4], "null")) &&
						ipv6_parse_prefix(tokens[4], &addr, NULL))) {
			err = -EINVAL;
			goto out;
		}
		prefix_canonize(&prefix, plen);
		if(!(p = bier6_rib_prefix_goc(b, &prefix, plen, 0))) {
			err = create?-ENOMEM:-ENOENT;
			goto out;
		}

		err = bier6_rib_bit_set(b, p, bit, create?&addr:NULL);
	} else {
		err = -EINVAL;
	}
out:
	kfree(buf);
	return err?err:count;
}

static struct file_operations fops = {
	.open           = bier6_dev_open,
	.release        = single_release,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.write          = bier6_dev_write
};

int bier6_dev_ctrl(struct bier6 *b, int term)
{
	int retval = 0;

	if(term)
		goto term;

	if ((retval = alloc_chrdev_region(&b->devno, 0, 1, DEVICE_NAME)) < 0) {
		printk(KERN_ERR"failed to register device number: error %d\n", retval);
		goto err;
	}

	if (IS_ERR((b->class = class_create(THIS_MODULE, CLASS_NAME)))) {
		retval = PTR_ERR(b->class);
		printk(KERN_ERR"failed to create class '%s': error %d\n", CLASS_NAME, retval);
		goto err_class;
	}

	if (IS_ERR((b->chardev = device_create(b->class, NULL, b->devno, b, DEVICE_NAME)))) {
		retval = PTR_ERR(b->chardev);
		printk(KERN_ERR"failed to create device '%s': error %d\n", DEVICE_NAME, retval);
		goto err_dev;
	}

	cdev_init(&b->cdev, &fops);
	if ((retval = cdev_add(&b->cdev, b->devno, 1))) {
		printk(KERN_ERR"failed to add cdev '%s': error %d\n", DEVICE_NAME, retval);
		goto cdev_err;
	}

	return 0;

term:
	cdev_del(&b->cdev);
cdev_err:
	device_destroy(b->class, b->devno);
err_dev:
	class_destroy(b->class);
err_class:
	unregister_chrdev_region(b->devno, 1);
err:
	return retval;
}

int bier6_dev_init(struct bier6 *b)
{
	return bier6_dev_ctrl(b, 0);
}

void bier6_dev_term(struct bier6 *b)
{
	bier6_dev_ctrl(b, 1);
}
