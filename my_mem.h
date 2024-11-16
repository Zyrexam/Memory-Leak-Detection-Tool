#ifndef MY_MEM_H
#define MY_MEM_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_PROCESSES 1024 

// Struct to store info of memory
typedef struct {
    pid_t pid;                          // Process ID
    char process_name[256];             // Process name
    size_t allocated_size;               // Total allocated size
    size_t deallocated_size;             // Total deallocated size
    size_t memory_leak;                 // Calculated memory leak
} ProcessMemoryInfo;

static ProcessMemoryInfo *process_map = NULL; // Pointer for shared memory of process_map
static int *process_count = NULL; // Pointer for shared memory to store process count
int process_index = 0; 

// Initialize shared memory for process tracking
void initialize_memory_tracker() {
    int count_shm_id = shmget(ftok("/tmp/shmfile_count", 64), sizeof(int), IPC_CREAT | 0666);
    if (count_shm_id < 0) {
        perror("shmget for process count failed");
        exit(1);
    }
    process_count = (int *)shmat(count_shm_id, NULL, 0);
    if (process_count == (int *)(-1)) {
        perror("shmat for process count failed");
        exit(1);
    }
    *process_count = 0; // Initialize process count to 0

    int shm_id = shmget(1234, sizeof(ProcessMemoryInfo) * MAX_PROCESSES, IPC_CREAT | 0666);
    if (shm_id < 0) {
        perror("shmget for process map failed");
        exit(1);
    }
    process_map = (ProcessMemoryInfo *)shmat(shm_id, NULL, 0);
    if (process_map == (ProcessMemoryInfo *)(-1)) {
        perror("shmat for process map failed");
        exit(1);
    }
    memset(process_map, 0, sizeof(ProcessMemoryInfo) * MAX_PROCESSES); // Initialize the shared memory
}

// Cleanup shared memory (detach)
void cleanup_memory_tracker() {
    if (process_map) shmdt(process_map);  
    if (process_count) shmdt(process_count);
}

// Getting name of the binary file 
void get_process_name(pid_t pid, char *process_name) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    FILE *file = fopen(path, "r");
    if (file) {
        fgets(process_name, 256, file);
        process_name[strcspn(process_name, "\n")] = '\0';  // Remove newline
        fclose(file);
    } else {
        snprintf(process_name, 256, "Unknown");
    }
}

// Finding Process data from Shared memory
ProcessMemoryInfo* find_process(pid_t pid) {
    for (int i = 0; i < *process_count; i++) {
        if (process_map[i].pid == pid) {
            return &process_map[i];
        }
    }
    return NULL;
}

// Removing process from shared memory
void remove_process(pid_t pid) {
    for (int i = 0; i < *process_count; i++) {
        if (process_map[i].pid == pid) {
            memmove(&process_map[i], &process_map[i + 1], (*process_count - i - 1) * sizeof(ProcessMemoryInfo));
            (*process_count)--; // Decrement the count
            break;
        }
    }
}

// Update shared memory based on allocation/deallocation
void update_process_memory(pid_t pid, size_t size, int is_allocated) {
    ProcessMemoryInfo *process_info = find_process(pid);
    if (process_info == NULL) {
        if (*process_count == MAX_PROCESSES) {
            remove_process(process_map[0].pid);
        }
        process_info = &process_map[*process_count];
        process_info->pid = pid;
        get_process_name(pid, process_info->process_name);
        process_info->allocated_size = 0;
        process_info->deallocated_size = 0;
        (*process_count)++;
    }
    if (is_allocated) {
        process_info->allocated_size += size;
    } else {
        process_info->deallocated_size += size;
    }
    process_info->memory_leak = process_info->allocated_size - process_info->deallocated_size;
}

// Memory allocation and deallocation functions
void my_free(void *ptr, size_t size) {
    if (ptr) {
        update_process_memory(getpid(), size, 0);
        free(ptr);
    }
}



void *my_calloc(size_t num, size_t size) {
    size_t total_size = num * size;
    void *ptr = malloc(total_size);
    if (ptr) {
        memset(ptr, 0, total_size);
        update_process_memory(getpid(), total_size, 1);
        return ptr;
    }
    return NULL;
}

void *my_malloc(size_t size) {
    // Allocate memory using system malloc
    void *ptr = malloc(size);

    // Get the PID of the current process
    pid_t pid = getpid();

    // Find the process in the memory tracker
    ProcessMemoryInfo *process_info = find_process(pid);

    if (!process_info) {
        // Process not found, insert new process into the circular buffer
        process_info = &process_map[process_index];  // Use circular buffer index
        process_info->pid = pid;
        snprintf(process_info->process_name, sizeof(process_info->process_name), "Process %d", pid);
    }

    // Update the allocated memory size for the current process
    process_info->allocated_size += size;

    // Increment the process count if it's less than the maximum
    if (*process_count < MAX_PROCESSES) {
        (*process_count)++;
    } else {
        // If count reaches the limit, move the index to the next process (circular)
        process_index = (process_index + 1) % MAX_PROCESSES;
    }

    return ptr;  // Return allocated memory
}


void *my_realloc(void *ptr, size_t old_size, size_t new_size) {
    if (!process_map) initialize_memory_tracker();
    void *new_ptr = realloc(ptr, new_size);
    if (new_ptr) {
        update_process_memory(getpid(), new_size - old_size, 1);
        printf("Reallocated memory for PID %d from %zu to %zu bytes\n", getpid(), old_size, new_size);
        return new_ptr;
    }
    return NULL;
}

void print_memory_info() {
    printf("PID\tProcess Name\tAllocated (Bytes)\tDeallocated (Bytes)\tMemory Leak\n");
    printf("------------------------------------------------------------------------------------------------\n");

    for (int i = 0; i < *process_count; i++) {
        ProcessMemoryInfo *p = &process_map[i];
        if (p->pid) {
            size_t leak = p->memory_leak;
            double leak_percent = p->allocated_size ? (double)leak / p->allocated_size * 100 : 0;

            // Determine color for memory leak
            char *color = "\033[0m"; // Default color
            if (leak_percent > 50.0) {
                color = "\033[1;31m"; // Red
            } else if (leak_percent > 10.0) {
                color = "\033[1;33m"; // Yellow
            } else {
                color = "\033[1;32m"; // Green
            }

            // Use fixed-width formatting while wrapping the memory leak column with the color
            printf("%d\t%-15s\t%-20zu\t%-20zu\t%s%-zu (%.2f%%)\033[0m\n",
                   p->pid,                     // PID
                   p->process_name,            // Process name
                   p->allocated_size,          // Allocated memory
                   p->deallocated_size,        // Deallocated memory
                   color,                      // Color start
                   p->memory_leak,             // Memory leak value
                   leak_percent);              // Leak percentage
        }
    }
    printf("\n");
}

#endif