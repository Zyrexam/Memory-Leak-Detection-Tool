#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include "my_mem.h"

#define MAX_PROCESSES 1024


// Circular buffer index
int process_index = 0;  // Tracks where to insert the new process in the circular buffer

// Shared memory for process info
ProcessMemoryInfo *process_map;
int *process_count;

int main() {
    // Initialize memory tracker and shared memory for process count and process map
    initialize_memory_tracker();

    // Get shared memory for process count
    int count_shm_id = shmget(ftok("/tmp/shmfile_count", 64), sizeof(int), IPC_CREAT | 0666);
    if (count_shm_id == -1) {
        perror("shmget failed for process_count");
        return 1;
    }

    process_count = (int *)shmat(count_shm_id, NULL, 0);
    if (process_count == (void *)-1) {
        perror("shmat failed for process_count");
        return 1;
    }

    // Get shared memory for process map
    int shm_id = shmget(1234, sizeof(ProcessMemoryInfo) * MAX_PROCESSES, IPC_CREAT | 0666);
    if (shm_id == -1) {
        perror("shmget failed for process_map");
        return 1;
    }

    process_map = (ProcessMemoryInfo *)shmat(shm_id, NULL, 0);
    if (process_map == (void *)-1) {
        perror("shmat failed for process_map");
        return 1;
    }

    // Initialize process_count if it's the first time running
    if (*process_count == 0) {
        memset(process_map, 0, sizeof(ProcessMemoryInfo) * MAX_PROCESSES);  // Zero out process map
    }

    while (1) {
        printf("\033[H\033[J");  // Clear screen

        // Iterate through all tracked processes and print memory info
        for (int i = 0; i < *process_count; i++) {
            if (process_map[i].pid != 0) {  // Check if the PID is valid
                printf("PID: %d | Name: %s | Allocated: %zu | Deallocated: %zu | Leak: %.2f%%\n",
                    process_map[i].pid,
                    process_map[i].process_name,
                    process_map[i].allocated_size,
                    process_map[i].deallocated_size,
                    (float)(process_map[i].allocated_size - process_map[i].deallocated_size) /
                    process_map[i].allocated_size * 100);
            }
        }

        // If process count exceeds MAX_PROCESSES, reset to the first index
        if (*process_count >= MAX_PROCESSES) {
            process_index = process_index % MAX_PROCESSES;  // Reset index to 0 when max is reached
        }

        // Sleep for 2 seconds before updating
        sleep(2);
    }

    return 0;
}
