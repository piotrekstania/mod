#include <linux/fs.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/time.h>

#include <asm/uaccess.h>

static ssize_t rnts_read(struct file * file, char * buf, size_t count, loff_t *ppos) {

	struct timespec tv;

	char buffer[100];
	int len;

	getnstimeofday(&tv);

	sprintf(buffer, "%lu\n", tv.tv_sec);
	len = strlen(buffer);

	/*
	 * We only support reading the whole string at once.
	 */
	if(count < len) return -EINVAL;
	/*
	 * If file position is non-zero, then assume the string has
	 * been read and indicate there is no more data to be read.
	 */
	if(*ppos != 0) return 0;
	/*
	 * Besides copying the string to the user provided buffer,
	 * this function also checks that the user has permission to
	 * write to the buffer, that it is mapped, etc.
	 */
	if(copy_to_user(buf, buffer, len)) return -EINVAL;
	/*
	 * Tell the user how much data we wrote.
	 */
	*ppos = len;

	return len;
}


static const struct file_operations rnts_fops = {
	.owner = THIS_MODULE,
	.read  = rnts_read
};

static struct miscdevice rnts_dev = {
	MISC_DYNAMIC_MINOR,
	"rnts",
	&rnts_fops
};

static int __init rnts_init(void) {
	int ret = misc_register(&rnts_dev);

	if(ret) printk(KERN_ERR "Unable to register \"Hello, world!\" misc device\n");
	return ret;
}



static void __exit rnts_exit(void) {
	misc_deregister(&rnts_dev);
}

module_init(rnts_init);
module_exit(rnts_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Piotr Stania <piotrekstania@gmail.com>");
MODULE_DESCRIPTION("Ronter Sensor Driver");
MODULE_VERSION("0.1");
