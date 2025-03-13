#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/crypto.h>   // encryption
#include <linux/slab.h>      // memory allocation
#include <linux/mm.h>        // shared memory
#include <linux/device.h> // for device registeration and device file


#define DEVICE_NAME "Simple IPC"
#define SHM_SIZE 1024

MODULE_LICENSE("GPL");
MODULE_AUTHOR("");
MODULE_DESCRIPTION("A simple IPC driver");
MODULE_VERSION("1.0");

static int major_number; // Store dynamically allocated major number
static char *shared_mem; // replacing buffer with shared memmory

DEFINE_MUTEX(w_mutex); // mutex for writers

struct semaphore r_counter; // semaphore for readers
int  r_count = 0; //counter to track readers

static struct class *ipc_class = NULL;
static struct device *ipc_device;


// Open func
static int device_open(struct inode *inode, struct file *file) {
    printk(KERN_INFO "Device opened\n");
    return 0;
}

// Close func
static int device_closed(struct inode *inode, struct file *file) {
    printk(KERN_INFO "Device closed\n");
    return 0;
}

// Read
static ssize_t device_read(struct file *file, char __user *user_buffer, size_t len, loff_t *offset) {
    size_t bytes_to_read = min(len, SHM_SIZE);

    down_interruptible(&r_counter);
    r_count++;

    if (r_count == 1) {     
        mutex_lock(&w_mutex);
    }

    up(&r_counter);

    shared_mem[bytes_to_read] = '\0'; 
    
    if (copy_to_user(user_buffer, shared_mem, bytes_to_read + 1)) {
        return -EFAULT;
    }

    printk(KERN_INFO "Device read %zu bytes\n", bytes_to_read);

    down_interruptible(&r_counter);
    r_count--;

    if(r_count == 0) {   
        mutex_unlock(&w_mutex);
    }  

    up(&r_counter);

    return bytes_to_read;
}

// Write
static ssize_t device_write(struct file *file, const char __user *user_buffer, size_t len, loff_t *offset) {
    size_t bytes_to_write = min(len, SHM_SIZE);

    if (bytes_to_write >= SHM_SIZE) {
        printk(KERN_ERR "Write attempt exceeds buffer size\n");
        return -EFAULT;
    }

    mutex_lock(&w_mutex);

    if (copy_from_user(shared_mem, user_buffer, bytes_to_write)) {
        shared_mem[bytes_to_write] = '\0';  
        mutex_unlock(&w_mutex);
        return -EFAULT;
    }

    printk(KERN_INFO "Device wrote %zu bytes\n", bytes_to_write);

    mutex_unlock(&w_mutex);

    return bytes_to_write;
}

static struct file_operations fops = {
    .open = device_open,
    .release = device_closed,
    .read = device_read,
    .write = device_write,
};

// intialising
static int __init device_init(void) {
    major_number = register_chrdev(0, DEVICE_NAME, &fops);  
    if (major_number < 0) {
        printk(KERN_ALERT "Device registration failed\n");
        return major_number;
    }

    ipc_class = class_create("ipc_class"); 
    ipc_device = device_create(ipc_class, NULL, MKDEV(major_number, 0), NULL, "Simple_IPC");
    
    sema_init(&r_counter, 1);  

    shared_mem = kmalloc(SHM_SIZE, GFP_KERNEL);
    if (!shared_mem) {
        printk(KERN_ALERT "memory allocation failed\n");
        return -ENOMEM;
    }

    printk(KERN_INFO "Device registered with major number %d\n", major_number);
    return 0;
}

// Cleaning up the device
static void __exit device_exit(void) {
    unregister_chrdev(major_number, DEVICE_NAME);  
    kfree(shared_mem); 
    printk(KERN_INFO "Device unregistered\n");
}

module_init(device_init);
module_exit(device_exit);
