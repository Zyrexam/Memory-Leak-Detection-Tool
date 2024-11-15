// 100% Memory Leak

#include <stdio.h>
#include <stdlib.h>
#include "my_mem.h"


int main() {
    initialize_memory_tracker();  // Ensure shared memory is initialized

    size_t num_elements = 5;
    int *array = (int *)my_malloc(num_elements * sizeof(int));
    
    if (array == NULL) {
        perror("Memory allocation failed");
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < num_elements; i++) {
        array[i] = (int)(i + 1);
    }
    
    // Reallocating memory
    array = (int *)my_realloc(array, 5 * sizeof(int), 6 * sizeof(int));

    // Freeing memory
    my_free(array, 6 * sizeof(int));

    // Print memory info
    print_memory_info();

    return 0;
}

