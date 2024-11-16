#include "my_mem.h"

int main() {
    initialize_memory_tracker();
    void *ptr1 = my_malloc(2000); // Allocate 2000 bytes
    void *ptr2 = my_malloc(1500); // Allocate another 1500 bytes

    // Only free a portion of the allocated memory to simulate a large memory leak
    my_free(ptr1, 500); // Free only 500 bytes from the first allocation

    print_memory_info(); // Print memory usage information with color-coded leaks

    return 0;
}
