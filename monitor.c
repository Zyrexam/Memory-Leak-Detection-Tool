#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include "my_mem.h"

#define MAX_PROCESSES 1024

int main() {
    initialize_memory_tracker(); // Initialize the memory tracker

    while (1) {
        for (int i = 0; i < *process_count; i++) {
            if (
                process_map[i].allocated_size == process_map[i].deallocated_size) {
                remove_process(process_map[i].pid); // Remove terminated process
            }
        }
        print_memory_info(); // Display the memory usage info
        sleep(2);  // Update every 2 seconds
    }

    cleanup_memory_tracker(); // Cleanup shared memory
    return 0;
}

