#include <stdio.h> 
#include <stdlib.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close
#include <string.h>

#define DEVICE_PATH "/dev/ipc_device"

int main() {
    int fd; // fild descriptor
    char buffer[1024];

    // Open 
    fd = open(DEVICE_PATH, O_RDWR); // open() syscall, O_RDWR flag for read/write mode
    if (fd < 0) {
        perror("Failed to open device");
        return EXIT_FAILURE;
    }

    // Write
    const char *message = "Hello, Kernel!";
    write(fd, message, strlen(message)); // write() syscall

    // Read
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer)); // read() syscall
    printf("Message read: %s\n", buffer);

    close(fd);
    return 0;
}
