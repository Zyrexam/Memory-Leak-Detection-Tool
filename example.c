#include <stdio.h>
#include <stdlib.h>
#include "my_mem.h"


int main() {
    
    size_t num_elements = 5;
    int *array = (int *)my_malloc(num_elements * sizeof(int));
    
    // Check if malloc succeeded
    if (array == NULL) {
        perror("Failed to allocate memory");
        return EXIT_FAILURE; 
    }

    for (size_t i = 0; i < num_elements; i++) {
        array[i] = (int)(i + 1);
    }
    
    array = (int *)my_realloc(array,5 * sizeof(int), 6 * sizeof(int));

    return 0;
}

