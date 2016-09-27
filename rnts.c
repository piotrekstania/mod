#include <linux/fs.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/time.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>

#define RNTS_INPUT		21

static uint32_t frames = 0;
static uint32_t incomplete = 0;
static uint32_t crc = 0;
static uint64_t last = 0;
static uint8_t rnts_buf[50];
static uint8_t bit, byte;
static int irq;

static int16_t b_t, s_t;
static int16_t b_h, s_h;
static int16_t b_p;

uint8_t crc8(uint8_t *buffer, uint8_t size) {

	uint8_t	 crc = 0;
	uint8_t loop_count;
	uint8_t  bit_counter;
	uint8_t  data;
	uint8_t  feedback_bit;

	for (loop_count = 0; loop_count != size; loop_count++)
	{
		data = buffer[loop_count];

		bit_counter = 8;
		do {
			feedback_bit = (crc ^ data) & 0x01;

			if ( feedback_bit == 0x01 ) {
				crc = crc ^ 0x18;	             //0X18 = X^8+X^5+X^4+X^0
			}
			crc = (crc >> 1) & 0x7F;
			if ( feedback_bit == 0x01 ) {
				crc = crc | 0x80;
			}

			data = data >> 1;
			bit_counter--;

		} while (bit_counter > 0);
	}

	return crc;
}

static irqreturn_t rnts_isr(int irq, void *data) {
	struct timespec tv;
	uint64_t ts;

	getnstimeofday(&tv);
	ts = tv.tv_sec;
	ts *= 1000000;
	ts += (tv.tv_nsec/1000);

	if(gpio_get_value(RNTS_INPUT) == 0) {
		last = ts;
		return IRQ_HANDLED;

	} else {
		ts -= last;

		if((ts > 200) && (ts < 400)) {
			//bit 0 - najczęściej
			rnts_buf[byte] &= ~(1<<bit++);
			if(bit == 8) {
				bit = 0;
				byte++;
				if(byte == 0) byte = 0;
			}
		} else if((ts > 500) && (ts < 700)) {
			rnts_buf[byte] |= (1<<bit++);
			if(bit == 8) {
				bit = 0;
				byte++;
				if(byte == 0) byte = 0;
			}
		} else if((ts > 1900) && (ts < 2100)) {
			bit = 0;
			byte = 0;
		} else if((ts > 2900) && (ts < 3100)) {
			frames++;

			if(bit != 0) {
				incomplete++;
				return IRQ_HANDLED;
			}

			if(rnts_buf[byte-1] != crc8(rnts_buf, byte-1)) {
				crc++;
				return IRQ_HANDLED;
			}

			b_t = rnts_buf[4];
			b_t <<= 8;
			b_t |= rnts_buf[5];

			s_t = rnts_buf[6];
			s_t <<= 8;
			s_t |= rnts_buf[7];

			b_h = rnts_buf[8];
			b_h <<= 8;
			b_h |= rnts_buf[9];

			s_h = rnts_buf[10];
			s_h <<= 8;
			s_h |= rnts_buf[11];

			b_p = rnts_buf[12];
			b_p <<= 8;
			b_p |= rnts_buf[13];

		}


	}


	return IRQ_HANDLED;
}

static ssize_t rnts_read(struct file * file, char * buf, size_t count, loff_t *ppos) {

	char buffer[200];
	int len;


	sprintf(buffer, "\nframes: %zu, incomplete: %zu, crc: %zu\n\nBMP/SHT: %d.%lu | %d.%lu °C\nBMP/SHT: %d.%d | %d.%d %c\nBMP: %d.%d hPa\n\n", frames, incomplete, crc, b_t/10, abs(b_t%10), s_t/10, abs(s_t%10), b_h/10, b_h%10, s_h/10, s_h%10, '%', b_p/10, b_p%10);
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

	if(ret) printk(KERN_ERR "Unable to register rnts misc device\n");

	ret = gpio_request_one(RNTS_INPUT, GPIOF_IN, "rnts_input");

	if(ret) printk(KERN_ERR "Unable to request GPIOs: %d\n", ret);

	irq = gpio_to_irq(RNTS_INPUT);
	ret = request_irq(irq, rnts_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "rntsmod", NULL);

	return ret;
}



static void __exit rnts_exit(void) {
	free_irq(irq, NULL);
	gpio_free(RNTS_INPUT);
	misc_deregister(&rnts_dev);
}

module_init(rnts_init);
module_exit(rnts_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Piotr Stania <piotrekstania@gmail.com>");
MODULE_DESCRIPTION("Ronter Sensor Driver");
MODULE_VERSION("0.1");
