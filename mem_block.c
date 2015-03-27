#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/smp.h>
#include <linux/wait.h>
#include <linux/uaccess.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/semaphore.h>

#define MOD_NAME "mem_block"
#define DEV_NAME "mem_block"
#define MEM_SIZE 1024
#define MEM_BLOCK_MAJOR 60
#define MEM_BLOCK_MINOR 0
#define DELAY 10

int mem_block_major = MEM_BLOCK_MAJOR;
int mem_block_minor = MEM_BLOCK_MINOR;

struct mem_block {
	struct	cdev cdev;
	char	buf[MEM_SIZE];
	int	read_ptr;
	struct	semaphore sem;
	struct  timer_list timer;
};

struct mem_block my_device;
 
void write_mem_block_timer(unsigned long arg)
{
	int write_len;
	char buf[256];
	struct mem_block *dev = (struct mem_block *) arg;

	/* first write to local buffer to checkout the write lenth*/
	write_len = sprintf(buf, 
			"processor ID: %d, comm: %s\njiffie: %ld\n",
			smp_processor_id(), current->comm, jiffies);

	/* if there's enough space, write it to device buffer */
	if((MEM_SIZE - strlen(dev->buf)) > write_len) {
		strcat(dev->buf, buf);
		dev->read_ptr += write_len;
	}
	dev->timer.expires = jiffies + DELAY * HZ;
	add_timer(&dev->timer);
}

int mem_block_open(struct inode *inode, struct file *file)
{
	struct mem_block *dev;
	
	dev = container_of(inode->i_cdev, struct mem_block, cdev);
	file->private_data = dev;
	init_timer(&dev->timer);

	/* 
	 * when the device file is open, then add a timer
	 * to update device mem_block(dev->buf)
	 */
	dev->timer.data = (unsigned long)&my_device;
	dev->timer.function = write_mem_block_timer;
	dev->timer.expires = jiffies + DELAY * HZ;

	return 0;
}

int mem_block_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations mem_block_fops = {
	.owner = THIS_MODULE,
	.open = mem_block_open,
	.release = mem_block_release,
	//.read = mem_block_read,
	//.write = mem_block_write,
};

static void mem_block_setup_cdev(struct mem_block *dev, int index)
{
	int err, devno = MKDEV(mem_block_major, mem_block_minor);
	
	/* Initialize device data structure. */
	cdev_init(&dev->cdev, &mem_block_fops);
	dev->read_ptr = 0;
	memset(dev->buf, 0, MEM_SIZE);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &mem_block_fops;
	sema_init(&dev->sem, 0);

	err = cdev_add(&dev->cdev, devno, 1);

	if (err) 
		pr_debug("[MEM_BLOCK]: LINE %d can't get major\n",
			__LINE__);
}

static int __init mem_block_init(void)
{
	int res, devno = MKDEV(mem_block_major, mem_block_minor);
	res = register_chrdev_region(devno, 1, DEV_NAME);

	if (res < 0) {
		pr_debug("[MEM_BLOCK]: LINE %d Can't register char device\n",
			__LINE__);
		return res;
	}

	mem_block_setup_cdev(&my_device, 0);

	pr_debug("[%s] LINE %d, regitster mem_block success\n",
			__func__, __LINE__);
	return res;
}

static void __exit mem_block_exit(void)
{
	int devno = MKDEV(mem_block_major, mem_block_minor);
	
	cdev_del(&my_device.cdev);
	unregister_chrdev_region(devno, 1);
	
	pr_debug("[%s] LINE %d, unregitster mem_block success\n",
			__func__, __LINE__);
}

module_init(mem_block_init);
module_exit(mem_block_exit);

MODULE_LICENSE("Dual BSD/GPL");
