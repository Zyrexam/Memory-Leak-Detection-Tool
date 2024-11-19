// Some Memory Leak

#include "my_mem.h"

int main() {
    initialize_memory_tracker();

    void *ptr1 = my_malloc(1024);  // Allocate 1 KB
    void *ptr2 = my_malloc(512);   // Allocate 512 bytes
    my_free(ptr1, 1024);           // Free 1 KB
    void *ptr3 = my_malloc(256);   // Allocate 256 bytes
    my_free(ptr2, 512);            // Free 512 bytes

    // Forgot to free `ptr3`
    print_memory_info();
    cleanup_memory_tracker();

    return 0;
}

