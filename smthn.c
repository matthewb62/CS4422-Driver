#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

// Function that uses modular exponentiation to compute (base^exp) % mod
// calculates big numbers raised to a power and divided by another number, using a smart method called square-and-multiply.
static long long mod_exp(long long base, long long exp, long long mod) {
    long long result = 1;
    while (exp > 0) {
        if (exp % 2 == 1) // If exponent is odd
            result = (result * base) % mod;
        base = (base * base) % mod;
        exp /= 2;
    }
    return result;
}

// Function that uses the extended Euclidean algorithm to compute modular inverse
// Finds the number 'd' such that (e * d) % phi = 1, needed for decryption
static long long mod_inverse(long long e, long long phi) {
    long long t = 0, newt = 1, r = phi, newr = e;
    while (newr != 0) {
        long long quotient = r / newr;
        long long temp = t;
        t = newt;
        newt = temp - quotient * newt;
        temp = r;
        r = newr;
        newr = temp - quotient * newr;
    }
    return (t < 0) ? t + phi : t;
}

// Function that reads a file and stores content in a buffer (Kernel space version)
static void read_file(const char *filename, char *buffer, int size) {
    struct file *file;
    mm_segment_t old_fs;
    ssize_t ret;

    file = filp_open(filename, O_RDONLY, 0);
    if (IS_ERR(file)) {
        printk(KERN_ERR "Error opening file %s\n", filename);
        return;
    }

    old_fs = get_fs();
    set_fs(KERNEL_DS); // Allow kernel to access user space

    ret = kernel_read(file, buffer, size - 1, &file->f_pos);
    if (ret > 0) {
        buffer[ret] = '\0'; // Null-terminate the string
    } else {
        printk(KERN_ERR "Failed to read file %s\n", filename);
    }

    set_fs(old_fs);
    filp_close(file, NULL);
}

static int __init rsa_module_init(void) {
    printk(KERN_INFO "RSA Kernel Module Loaded\n");

    // RSA key generation
    long long p = 61, q = 53; // Prime numbers
    long long n = p * q;
    long long phi = (p - 1) * (q - 1);
    long long e = 17; // Public exponent
    long long d = mod_inverse(e, phi); // Private key

    printk(KERN_INFO "Public Key: (e=%lld, n=%lld)\n", e, n);
    printk(KERN_INFO "Private Key: (d=%lld, n=%lld)\n", d, n);

    // Reads a message from text.txt
    char message[256] = {0};
    read_file("/text.txt", message, sizeof(message)); // Ensure correct path
    printk(KERN_INFO "Original Message: %s\n", message);

    // Encrypt message
    long long encrypted[256];
    int len = strlen(message);
    printk(KERN_INFO "Encrypted: ");
    for (int i = 0; i < len; i++) {
        encrypted[i] = mod_exp((long long)message[i], e, n);
        printk(KERN_CONT "%lld ", encrypted[i]); // Use KERN_CONT for continuation
    }
    printk(KERN_INFO "\n");

    // Decrypt message
    char decrypted[256] = {0};
    printk(KERN_INFO "Decrypted Message: ");
    for (int i = 0; i < len; i++) {
        decrypted[i] = (char)mod_exp(encrypted[i], d, n);
        printk(KERN_CONT "%c", decrypted[i]);
    }
    printk(KERN_INFO "\n");

    return 0;
}

static void __exit rsa_module_exit(void) {
    printk(KERN_INFO "RSA Kernel Module Unloaded\n");
}

module_init(rsa_module_init);
module_exit(rsa_module_exit);
