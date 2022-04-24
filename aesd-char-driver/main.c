/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
#include <linux/slab.h> 
#include <linux/uaccess.h> 
#include <linux/string.h>


int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Ananth Deshpande"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
	PDEBUG("open");
	/**
	 * TODO: handle open
	 */
	struct aesd_dev* char_dev_ptr = NULL;
	char_dev_ptr = container_of(inode->i_cdev, struct aesd_dev, cdev);
	filp->private_data = char_dev_ptr;

	return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
	PDEBUG("release");
	
	/**
	 * TODO: handle release
	 */

	return 0;
}
 
ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
	/**
	 * TODO: handle read
	 */
	struct aesd_dev *dev = filp->private_data;
	size_t bytes_to_read = 0;
	size_t entry_offset_byte = 0;
	size_t available_bytes = 0;
	ssize_t retVal = 0; 

	PDEBUG("read %zu bytes with offset %lld",count,*f_pos);

        /*lock the device, since it can be read from multiple processes*/
	if(mutex_lock_interruptible(&dev->lock))
	{
	   retVal = -ERESTARTSYS;
           return retVal; 
	}

        /*obtain the position where provided offset exists*/
	struct aesd_buffer_entry* aesd_buf_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->cir_buf, *f_pos, &entry_offset_byte);
	if(aesd_buf_entry == NULL) 
	{
	   retVal = 0;
	   goto out;
	}
	
	/*to find out how many bytes we can send in the current aesd_buffer_entry*/
	available_bytes = aesd_buf_entry->size - entry_offset_byte;
	
	/*
	   we shall only send the maximum possible bytes in current aesd_buffer_entry
	   and update the offset. The user process can call repeatedly until the number
	   of bytes it intends to read
	*/
	if(available_bytes > count)
	{
	   bytes_to_read = count;
	}
	else
	{ 
	   bytes_to_read = available_bytes;
	}

        /*copy the data to be read from driver into the user space buffer*/
	int ret = copy_to_user(buf, aesd_buf_entry->buffptr + entry_offset_byte, bytes_to_read);

	if(ret)
	{
	   retVal = -EFAULT;
	   goto out;
	}
	
	/*return the number of bytes actually read*/
	retVal = bytes_to_read;

        /*and update the file offset position*/
	*f_pos += bytes_to_read;

        /*handles the clean up activity as well as returning bytes read*/
	out:
	    mutex_unlock(&dev->lock);
	    return retVal;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
	ssize_t retVal = -ENOMEM;
	PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
	/**
	 * TODO: handle write
	 */
	 
	 const char* ret_entry = NULL;
	 
	/*obtain pointer to device*/ 
	struct aesd_dev* device = filp->private_data;
	
        /*to lock the device, since the write can happen from multiple processes*/
	if(mutex_lock_interruptible(&device->lock))
	{
	   retVal = -ERESTARTSYS;
	   return retVal;
	}
	
	/*allocate memory for aesd_buffer_entry of count size*/
	if(device->buffer_entry.size == 0)
	{
	   device->buffer_entry.buffptr = kzalloc(count, GFP_KERNEL);
	}
	/*to reallocate memory for the same buffer entry until we recieve a '\n'*/
	else 
	{
	   device->buffer_entry.buffptr = krealloc(device->buffer_entry.buffptr, device->buffer_entry.size + count, GFP_KERNEL);
	}

        /*to check if kzalloc was successful*/
	if(device->buffer_entry.buffptr == NULL)
	{
	   retVal = -ENOMEM;
	   goto out;
	} 

        /*to store the number of writes to read into retVal, this would be the case
          when we are able to successfully write the count number of bytes*/
	retVal = count;

        /*to copy the contents of the user space buffer provided via buf parameter into the allocated aesd_buffer_entry memory*/
	size_t error_bytes_num = copy_from_user((void*)(&device->buffer_entry.buffptr[device->buffer_entry.size]), buf, count);

       /*if the copy_from_user returned a nonzero value => there was a partial copy, wkt copy_from_user returns the number of
         bytes pending to be copied*/
	if(error_bytes_num)
	{
	   /*thus update retVal accordingly, here retVal is the current size of the written string*/
	   retVal = retVal - error_bytes_num;
	}

        /*thus update the size of respective buffer_entry string with retVal*/
	device->buffer_entry.size += retVal;

        /*
          to check for '\n' character in the current buffer_entry, if found it implies that
          we have obtained the complete string.
          Thus we can now add the buffer_entry to circular buffer and
          reset the buffer_entry parameters to next write call
        */
	if(strchr((char*)(device->buffer_entry.buffptr), '\n'))
	{
	   /*ret_entry is valid if the circular buffer was full and the oldest entry was overwritten*/
	   ret_entry = aesd_circular_buffer_add_entry(&device->cir_buf, &device->buffer_entry);
	   
	   /*thus, we need to clean the buffer in the previous entry*/
	   if(ret_entry)
	   {
	      kfree(ret_entry);
	   }
	 
	   /*reset the buffer_entry parameters for next write cycle*/
	   device->buffer_entry.buffptr = NULL;
	   device->buffer_entry.size = 0; 
	}

        /*to clean up/release resources*/
	out: 
	     mutex_unlock(&device->lock);
	     return retVal;

}
struct file_operations aesd_fops = {
	.owner =    THIS_MODULE,
	.read =     aesd_read,
	.write =    aesd_write,
	.open =     aesd_open,
	.release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
	int err, devno = MKDEV(aesd_major, aesd_minor);

	cdev_init(&dev->cdev, &aesd_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &aesd_fops;
	err = cdev_add (&dev->cdev, devno, 1);
	if (err) {
		printk(KERN_ERR "Error %d adding aesd cdev", err);
	}
	return err;
}



int aesd_init_module(void)
{
	dev_t dev = 0;
	int result;
	result = alloc_chrdev_region(&dev, aesd_minor, 1,
			"aesdchar");
	aesd_major = MAJOR(dev);
	if (result < 0) {
		printk(KERN_WARNING "Can't get major %d\n", aesd_major);
		return result;
	}
	memset(&aesd_device,0,sizeof(struct aesd_dev));

	/**
	 * TODO: initialize the AESD specific portion of the device
	 */
	mutex_init(&aesd_device.lock);
	result = aesd_setup_cdev(&aesd_device);

	if( result ) {
		unregister_chrdev_region(dev, 1);
	}
	return result;

}

void aesd_cleanup_module(void)
{
	dev_t devno = MKDEV(aesd_major, aesd_minor);

	cdev_del(&aesd_device.cdev);

	/**
	 * TODO: cleanup AESD specific poritions here as necessary
	 */
	aesd_circular_buffer_cleanup(&aesd_device.cir_buf);
	unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
