#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "Simple IPC"
#define BUFFER_SIZE 1024

MODULE_LICENSE("GPL");
MODULE_AUTHOR("");
MODULE_DESCRIPTION("A simple IPC driver");
MODULE_VERSION("1.0");

static int major_number; // Store dynamically allocated major number
static char buffer[BUFFER_SIZE]; // Internal buffer
static size_t buffer_size = 0; // Keeps track of how much data is stored in the buffer

DEFINE_MUTEX(my_mutex);

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

// Read func
static ssize_t device_read(struct file *file, char __user *user_buffer, size_t len, loff_t *offset) {
    size_t bytes_to_read = min(len, buffer_size);
    
    mutex_lock(&my_mutex);
    if (copy_to_user(user_buffer, buffer, bytes_to_read)) { // copy data to user-space
        mutex_unlock(&my_mutex);
        return -EFAULT;
    }

    buffer_size = 0;
    printk(KERN_INFO "Device read %zu bytes\n", bytes_to_read); // log device logging upon read

    mutex_unlock(&my_mutex);
    return bytes_to_read;
}

// Write func
static ssize_t device_write(struct file *file, const char __user *user_buffer, size_t len, loff_t *offset) {
    size_t bytes_to_write = min(len, buffer_size);

    mutex_lock(&my_mutex);

    // Ensure we don't write more than the buffer size
    if (len > BUFFER_SIZE) {
        len = BUFFER_SIZE;
    }

    if (copy_from_user(buffer, user_buffer, bytes_to_write)) {
        mutex_unlock(&my_mutex);
        return -EFAULT;
    }

    buffer_size = len;
    printk(KERN_INFO "Device wrote %zu bytes\n", bytes_to_write);

    mutex_unlock(&my_mutex);
    return bytes_to_write;
}

static struct file_operations fops = {
    .open = device_open,
    .release = device_closed,
    .read = device_read,
    .write = device_write,
};

static int __init device_init(void) {
    major_number = register_chrdev(0, DEVICE_NAME, &fops);  // register the device
    if (major_number < 0) {
        printk(KERN_ALERT "Device registration failed\n");
        return major_number;
    }

    printk(KERN_INFO "Device registered with major number %d\n", major_number);
    return 0;
}

// Cleaning up the device
static void __exit device_exit(void) {
    unregister_chrdev(major_number, DEVICE_NAME);  // Unregister the device
    printk(KERN_INFO "Device unregistered\n");
}

module_init(device_init);
module_exit(device_exit);



