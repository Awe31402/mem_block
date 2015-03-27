#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/smp.h>
#include <linux/wait.h>
#include <linux/uaccess.h>
#include <linux/time.h>
#include <linux/moduleparam.h>
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
int delay	    = DELAY;
module_param(mem_block_major, int, S_IRUGO);
module_param(mem_block_minor, int, S_IRUGO);
module_param(delay, int, S_IRUGO);

struct mem_block {
	struct	cdev cdev;
	char	buf[MEM_SIZE];
	int	curr_ptr;
	struct	semaphore sem;
	struct  timer_list timer;
	wait_queue_head_t r_wait;
};

struct mem_block my_device;

ssize_t mem_block_read(struct file *file,
		char __user *buf, size_t count, loff_t *f_pos)
{
	int retval;
	struct mem_block *dev = file->private_data;

	pr_debug("[%s] LINE %d, curr_ptr = %d\n",
			__func__, __LINE__, dev->curr_ptr);

	/* get the lock of resource*/
	if (down_interruptible(&dev->sem)) {
		pr_debug("[%s] LINE %d , Can't get the lock\n",
				__func__, __LINE__);
		return -ERESTARTSYS;
	}

	while (dev->curr_ptr == 0) {
		DEFINE_WAIT(wait);
		/* resource can't access yet, free the lock*/
		up(&dev->sem);
		pr_debug("[%s] LINE %d , current process is going to sleep\n",
				__func__, __LINE__);

		set_current_state(TASK_INTERRUPTIBLE);
		add_wait_queue(&dev->r_wait, &wait);
		schedule();

		/* the process is wakenup by timer function*/
		finish_wait(&dev->r_wait, &wait);
		if (signal_pending(current))
			return -ERESTARTSYS;

		if (down_interruptible(&dev->sem))
			return -ERESTARTSYS;
	}

	pr_debug("[%s] LINE %d, curr_ptr = %d\n",
			__func__, __LINE__, dev->curr_ptr);
	if (count < 0)
		return -EINVAL;

	count = (count > dev->curr_ptr) ? dev->curr_ptr : count;

	if (copy_to_user(buf, dev->buf, count)) {
		retval = -EFAULT;
		goto out;
	}

	*f_pos += count;
	retval = count;
out:
	dev->curr_ptr = 0;
	up(&dev->sem);
	return retval;
}

void write_mem_block_timer(unsigned long arg)
{
	int write_len;

	struct mem_block *dev = (struct mem_block *) arg;

	pr_debug("[%s]: LINE %d, Now in the timer\n",
			__func__, __LINE__);

	/* first write to local buffer to checkout the write lenth*/
	write_len = sprintf(dev->buf,
			"processor ID: %d, comm: %s\njiffies: %ld\n",
			smp_processor_id(), current->comm, jiffies);

	dev->curr_ptr = write_len;

	dev->timer.expires = jiffies + delay * HZ;
	dev->timer.function = write_mem_block_timer;
	add_timer(&dev->timer);

	pr_debug("[%s]: LINE %d, After write %d bytes\n",
			__func__, __LINE__, write_len);

	/* wake up waiting procecces*/
	wake_up_interruptible(&dev->r_wait);
}

int mem_block_open(struct inode *inode, struct file *file)
{
	struct mem_block *dev;

	pr_debug("[%s]: LINE %d open function\n",
			__func__, __LINE__);
	dev = container_of(inode->i_cdev, struct mem_block, cdev);
	file->private_data = dev;
	init_timer(&dev->timer);
	init_waitqueue_head(&dev->r_wait);

	/*
	 * when the device file is open, then add a timer
	 * to update device mem_block(dev->buf)
	 */
	dev->timer.data = (unsigned long)&my_device;
	dev->timer.function = write_mem_block_timer;
	dev->timer.expires = jiffies + delay * HZ;
	add_timer(&dev->timer);
	return 0;
}

int mem_block_release(struct inode *inode, struct file *file)
{
	struct mem_block *dev = file->private_data;

	del_timer(&dev->timer);
	return 0;
}

static const struct file_operations mem_block_fops = {
	.owner = THIS_MODULE,
	.open = mem_block_open,
	.release = mem_block_release,
	.read = mem_block_read,
};

static void mem_block_setup_cdev(struct mem_block *dev, int index)
{
	int err, devno = MKDEV(mem_block_major, mem_block_minor);

	/* Initialize device data structure. */
	cdev_init(&dev->cdev, &mem_block_fops);
	dev->curr_ptr = 0;
	memset(dev->buf, 0, MEM_SIZE);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &mem_block_fops;

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
		goto setup_failed;
	}

	sema_init(&my_device.sem, 1);
	mem_block_setup_cdev(&my_device, 0);

	pr_debug("[%s] LINE %d, regitster mem_block success\n",
			__func__, __LINE__);
	return res;

setup_failed:
	pr_debug("[%s] LINE %d, regitster mem_block failed\n",
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
