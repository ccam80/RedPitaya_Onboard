#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/ioctl.h>

#define CMA_ALLOC _IOWR('Z', 0, u32)

int main() {
    int fd;
    u32 size = 1024 * 1024; // Example size of 1MB

    fd = open("/dev/cma", O_RDWR);
    if (fd < 0) {
        perror("Failed to open /dev/cma");
        return -1;
    }

    // Allocate contiguous memory block
    if (ioctl(fd, CMA_ALLOC, &size) < 0) {
        perror("CMA_ALLOC ioctl failed");
        close(fd);
        return -1;
    }

    // Read the allocated memory address
    u32 mem_address;
    if (read(fd, &mem_address, sizeof(mem_address)) != sizeof(mem_address)) {
        perror("Failed to read memory address");
        close(fd);
        return -1;
    }

    printf("Allocated memory address: %u\n", mem_address);

    close(fd);
    return 0;
}

