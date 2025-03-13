#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/crypto.h>   // Encryption
#include <linux/slab.h>      // Memory allocation
#include <linux/mm.h>        // Shared memory
#include <linux/cdev.h>


#define DEVICE_NAME "Simple IPC"
#define MAJOR_DEVICE_NUMBER 42
#define MINOR_DEVICE_NUMBER 0

#define SHM_SIZE 1024 // Shared Memory Size
#define MAX_READER_COUNT 4 // The maximum amount of readers at any one time


MODULE_LICENSE("GPL");
MODULE_AUTHOR("");
MODULE_DESCRIPTION("A simple IPC driver");
MODULE_VERSION("1.0");

static struct class *ipc_class = NULL;
static struct device *ipc_device = NULL;

static char *shared_mem;
static int data_written = 0; //to track if message has been written or not

// https://0xax.gitbooks.io/linux-insides/content/SyncPrim/linux-sync-5.html
// https://oscourse.github.io/slides/semaphores_waitqs_kernel_api.pdf
static DEFINE_SEMAPHORE(rw_sem, MAX_READER_COUNT); // Semaphore for read/write


// Function prototypes
static int device_open(struct inode *inode, struct file *file);
static int device_closed(struct inode *inode, struct file *file);
static ssize_t device_read(struct file *file, char __user *user_buffer, size_t len, loff_t *offset);
static ssize_t device_write(struct file *file, const char __user *user_buffer, size_t len, loff_t *offset);

static struct file_operations fops = {
    .open = device_open,
    .release = device_closed,
    .read = device_read,
    .write = device_write,
};

// intialising
static int __init device_init(void) {
    int retval;
    retval = register_chrdev(MAJOR_DEVICE_NUMBER, DEVICE_NAME, &fops);  // register the device

    if (retval == 0) {
        printk("dev_testdr registered to major number %d and minor number %d\n", MAJOR_DEVICE_NUMBER, MINOR_DEVICE_NUMBER);
    } else {
        printk("Could not register dev_testdr\n");
        return retval;
    }

    ipc_class = class_create("ipc_class");
    if (IS_ERR(ipc_class)) {
        unregister_chrdev(MAJOR_DEVICE_NUMBER, DEVICE_NAME);
        printk(KERN_ALERT "Failed to create device class\n");
        return PTR_ERR(ipc_class);
    }

    // Create device node - this makes the device appear in /dev/
    ipc_device = device_create(ipc_class, NULL, MKDEV(MAJOR_DEVICE_NUMBER, 0), NULL, "ipc_device");
    if (IS_ERR(ipc_device)) {
        class_destroy(ipc_class);
        unregister_chrdev(MAJOR_DEVICE_NUMBER, DEVICE_NAME);
        printk(KERN_ALERT "Failed to create device\n");
        return PTR_ERR(ipc_device);
    }

    //semaphore
    sema_init(&rw_sem, MAX_READER_COUNT);  // 1 for single reader

    // allocating dynamic memory for shared memory using kmalloc
    shared_mem = kmalloc(SHM_SIZE, GFP_KERNEL);
    if (!shared_mem) {
        printk(KERN_ALERT "memory allocation failed\n");
        return -ENOMEM;  // out of memory
    }

    printk(KERN_INFO "Device registered with major number %d\n", MAJOR_DEVICE_NUMBER);
    return 0;
}

// Cleaning up the device
static void __exit device_exit(void) {
    device_destroy(ipc_class, MKDEV(MAJOR_DEVICE_NUMBER, 0)); // Remove the device
    class_destroy(ipc_class); // Remove the device class
    unregister_chrdev(MAJOR_DEVICE_NUMBER, DEVICE_NAME);  // Unregister the device

    kfree(shared_mem); // free the memory
    printk(KERN_INFO "Device unregistered\n");
}


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

    if (data_written == 0) {
        printk(KERN_INFO "No data available to read\n");
        return 0; 
    } 

    if (down_interruptible(&rw_sem)) {
        printk(KERN_ALERT "Semaphore down interruptible failed\n");
        return -EINTR;
    }

    //encrypt data

    printk(KERN_INFO "Reader acquired semaphore\n");


    if (copy_to_user(user_buffer, shared_mem, bytes_to_read)) { 
        printk(KERN_ERR "Failed to copy data to uesr space\n");
        up(&rw_sem);  // to ensure its released or else it gets stuck
        return -EFAULT;
    }

    printk(KERN_INFO "Device read %zu bytes\n", bytes_to_read); // log device logging upon read

    data_written = 0;

    up(&rw_sem);
    printk(KERN_INFO "Reader released semaphore\n");;

    return bytes_to_read;
}

// Write
static ssize_t device_write(struct file *file, const char __user *user_buffer, size_t len, loff_t *offset) {
    size_t bytes_to_write = min(len, SHM_SIZE);

    // Decrement the semaphore by the max amount of readers.
    // This ensure that when the writer is writing, no readers are reading.

   for (int i = 0; i < MAX_READER_COUNT; i++) {
        if (down_interruptible(&rw_sem)) {
            printk(KERN_ALERT "Semaphore down interruptible failed\n");
            return -EINTR;
        }
        

    if (copy_from_user(shared_mem, user_buffer, bytes_to_write)) {
        up(&rw_sem);
        return -EFAULT;
    }


    data_written = 1;

    wake_up_interruptible(&wait_queue);

    // encrypt data

    printk(KERN_INFO "Device wrote %zu bytes\n", bytes_to_write);

    memset(shared_mem + bytes_to_write, 0, SHM_SIZE - bytes_to_write); //clearing buffer
    
    for (int i = 0; i < MAX_READER_COUNT; i++) {
        up(&rw_sem);
    }

    return bytes_to_write;
}

module_init(device_init);
module_exit(device_exit);
