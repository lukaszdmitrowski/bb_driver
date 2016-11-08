#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>
//#include <linux/gpio.h>
//#include <linux/interrupt.h>
//#include <linux/kobject.h>
//#include <linux/kthread.h>
//#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define DEVICE_NAME "lukasz_device"
#define CLASS_NAME "character"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lukasz Dmitrowski");
MODULE_DESCRIPTION("Desc");
MODULE_VERSION("0.1");

#define OE_ADDR 	0x134
#define GPIO_DATAOUT 	0x13C
#define GPIO_DATAIN	0x138
#define GPIO1_ADDR 	0x4804C000
#define GPIO_SIZE 	0x1000

static unsigned long *base_address;

static int major_number;
static char message[256] = {0};

static short size_of_message;
static int number_opens = 0;
static struct class* class = NULL;
static struct device* device = NULL;

static int dev_open(struct inode*, struct file*);
static int dev_release(struct inode*, struct file*);
static ssize_t dev_read(struct file*, char*, size_t, loff_t*);
static ssize_t dev_write(struct file*, const char*, size_t, loff_t*);

static DEFINE_MUTEX(bb_driver_mutex);

static struct file_operations fops = 
{
	.open = dev_open,
	.read = dev_read,
	.write = dev_write,
	.release = dev_release,
};

static int __init dev_init(void)
{
	printk(KERN_INFO "MyDriver: Getting major number...\n");
	major_number = register_chrdev(0, DEVICE_NAME, &fops);
	if (major_number < 0)
	{
		printk(KERN_ALERT "MyDriver: Failed to register a major number\n");
		return major_number;
	}
	printk(KERN_INFO "MyDriver: Major number registered correctly: %d\n", major_number);

	class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(class))
	{
		unregister_chrdev(major_number, DEVICE_NAME);
		printk(KERN_ALERT "MyDriver: Failed to register device class\n");
	}
	printk(KERN_INFO "MyDriver: Device class registered correctly\n");

	device = device_create(class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
	if (IS_ERR(device))
	{
		class_destroy(class);
		unregister_chrdev(major_number, DEVICE_NAME);
		printk(KERN_ALERT "MyDriver: Failed to create the device\n");
		return PTR_ERR(device);
	}
	printk(KERN_INFO "MyDriver: Device class created correctly\n");

	/* Get base address of GPIO1*/
	base_address = (unsigned long*)ioremap(GPIO1_ADDR, GPIO_SIZE);

	/* Set GPIO_47 (P8 PIN 23 - GPIO1_17) as output */
	base_address[OE_ADDR / 4] &= 0xFFFFFFFF ^ (1 << 17);	
	
	mutex_init(&bb_driver_mutex);
	return 0;
}

static void __exit dev_exit(void)
{
	mutex_destroy(&bb_driver_mutex);
	device_destroy(class, MKDEV(major_number, 0));
	class_unregister(class);
	class_destroy(class);
	unregister_chrdev(major_number, DEVICE_NAME);
	printk(KERN_INFO "MyDriver: Goodbye\n");
}

static int dev_open(struct inode *inodep, struct file *filep)
{
	if (!mutex_trylock(&bb_driver_mutex))
	{
		printk(KERN_ALERT "MyDriver: Device in use by another process");
		return -EBUSY;
	}
	++number_opens;
	printk(KERN_INFO "MyDriver: Device has been opened %u time(s)\n", number_opens);
	return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
	int err_count = 0;
	err_count = copy_to_user(buffer, message, size_of_message);

	if (err_count == 0)
	{
		printk(KERN_INFO "MyDriver: Sent %u characters to the user\n", size_of_message);
		return (size_of_message = 0);
	}
	else
	{
		printk(KERN_INFO "MyDriver: Failed to send %u characters to the user\n", err_count);
		return -EFAULT;
	}
}

static ssize_t dev_write(struct file* filep, const char *buffer, size_t len, loff_t * offset)
{
	sprintf(message, "%s", buffer);
	size_of_message = strlen(message);
	printk(KERN_INFO "MyDriver: Received %d characters from the user\n", len);

	if (strcmp(message, "on") == 0)
	{
		base_address[GPIO_DATAOUT / 4] |= (1 << 17);
		printk(KERN_INFO "MyDriver: Set LED to ON");
	}
	else if (strcmp(message, "blink") == 0)
	{
		printk(KERN_INFO "MyDriver: Set LED to BLINK");
	}	
	else if (strcmp(message, "off") == 0)
	{
		base_address[GPIO_DATAOUT / 4] &= ~(1 << 17);
		printk(KERN_INFO "MyDriver: Set LED to OFF");
	}	
	else
	{
		printk(KERN_INFO "MyDriver: Unkown command");
	}

	return len;
}

static int dev_release(struct inode *inodep, struct file *filep)
{
	mutex_unlock(&bb_driver_mutex);
	printk(KERN_INFO "MyDriver: Device successfully closed\n");
	return 0;
}

module_init(dev_init);
module_exit(dev_exit);


