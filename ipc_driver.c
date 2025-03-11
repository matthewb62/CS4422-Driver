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
//static char buffer[BUFFER_SIZE]; 
static char *shared_mem; // replacing buffer with shared memmory
//static size_t buffer_size = 0; // Keeps track of how much data is stored in the buffer


DEFINE_MUTEX(w_mutex); // mutex for writers

struct semaphore r_counter; // semaphore for readers
int  r_count = 0; //counter to track readers

static struct class *ipc_class = NULL;
static struct device *ipc_driver = NULL;




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

    //implementing semaphore 
    down_interruptible(&r_counter);
    r_count++;


    // first reader locks the mutex so no writers can write
    if (r_count == 1) {     
        mutex_lock(&w_mutex);
    }

    up(&r_counter);

    //encrypt data
    
    
    if (copy_to_user(user_buffer, shared_mem, bytes_to_read)) { // copy data to user-space
        return -EFAULT;
    }
   
    printk(KERN_INFO "Device read %zu bytes\n", bytes_to_read); // log device logging upon read

    down_interruptible(&r_counter);
    r_count--;

    if(r_count == 0) {   //last reader unlocks mutex to allow writing
        mutex_unlock(&w_mutex);
    }  

    up(&r_counter);

    return bytes_to_read;
}

// Write
static ssize_t device_write(struct file *file, const char __user *user_buffer, size_t len, loff_t *offset) {
    size_t bytes_to_write = min(len, SHM_SIZE);

    mutex_lock(&w_mutex);

    if (copy_from_user(shared_mem, user_buffer, bytes_to_write)) {
        mutex_unlock(&w_mutex);
        return -EFAULT;
    }

    // encrypt data

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
    major_number = register_chrdev(0, DEVICE_NAME, &fops);  // register the device
    if (major_number < 0) {
        printk(KERN_ALERT "Device registration failed\n");
        return major_number;
    }

    // for device class
    ipc_class = class_create(THIS_MODULE, "ipc_class"); 
    ipc_device = device_create(ipc_class, NULL, MKDEV(major_number, 0), NULL, "Simple_IPC");
    
    //semaphore
    sema_init(&r_counter, 1);  // 1 for single reader

    // allocating dynamic memory for shared memory using kmalloc
    shared_mem = kmalloc(SHM_SIZE, GFP_KERNEL);
    if (!shared_mem) {
        printk(KERN_ALERT "memory allocation failed\n");
        return -ENOMEM;  // out of memory
    }

    printk(KERN_INFO "Device registered with major number %d\n", major_number);
    return 0;
}

// Cleaning up the device
static void __exit device_exit(void) {
    unregister_chrdev(major_number, DEVICE_NAME);  // Unregister the device
    kfree(shared_mem); // free the memory
    printk(KERN_INFO "Device unregistered\n");
}

module_init(device_init);
module_exit(device_exit);
