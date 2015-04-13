/*
 * Author: Pierre Pfister <pierre pfister@darou.fr>
 *
 * Copyright (c) 2014 Cisco Systems, Inc.
 *
 */

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
	struct bier6_dev *dev;
	char *buf = NULL;
	char *tokens[10];
	struct in6_addr addr, dst;
	u8 add, plen;
	int err = 0;

	if (!(buf = kmalloc(sizeof(char) * (count + 1), GFP_KERNEL)))
		return -ENOMEM;

	if (copy_from_user(buf, buffer, count)) {
		kfree(buf);
		return -EFAULT;
	}
	buf[count] = '\0';

	if(tokenize(buf, tokens, 10) || !tokens[1]) {
		err = -EINVAL;
	} else  if (!strcmp(tokens[0], "rib")) {
		if(bier6_netdev_goc(b, tokens[1], 0, &dev) || !tokens[2]) {
			err = -EINVAL;
			goto out;
		}

		if(!strcmp(tokens[2], "add") || !strcmp(tokens[2], "del")) {
			if(!tokens[3] || ipv6_parse_prefix(tokens[3], &addr, &plen)) {
				err = -EINVAL;
				goto out;
			}
			add = !strcmp(tokens[2], "add");
			printk("Là1\n");
			if(add && (!tokens[4] || ipv6_parse_prefix(tokens[4], &dst, NULL))) {
				err = -EINVAL;
				goto out;
			}
			printk("Là2\n");
			return bier6_rib_set(dev, &addr, plen, add?&dst:NULL);
		} else {
			err = -EINVAL;
		}
	} else if (!strcmp(tokens[0], "dev")) {
		if(!tokens[2]) {
			err = -EINVAL;
		} else if(!strcmp(tokens[1], "add")) {
			err = bier6_netdev_goc(b, tokens[2], 1, &dev);
		} else if(!strcmp(tokens[1], "del")) {
			if(!(err = bier6_netdev_goc(b, tokens[2], 0, &dev)))
				bier6_netdev_destroy(dev);
		}
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
