// 50% Mem Leak

#include "my_mem.h"

int main() {
    initialize_memory_tracker();

    void *ptr1 = my_malloc(1024);  // Allocate 1 KB
    void *ptr2 = my_malloc(1024);  // Allocate another 1 KB
    my_free(ptr1, 1024);           // Free the first 1 KB

    // Forgot to free `ptr2`
    print_memory_info();
    cleanup_memory_tracker();

    return 0;
}
