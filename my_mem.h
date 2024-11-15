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

typedef struct {
    pid_t pid;                          // Process ID
    char process_name[256];             // Process name
    size_t allocated_size;               // Total allocated size
    size_t deallocated_size;             // Total deallocated size
    size_t memory_leak;                 // Calculated memory leak
} ProcessMemoryInfo;

static ProcessMemoryInfo *process_map = NULL; // Pointer for shared memory
static int *process_count = NULL; // Pointer for shared memory to store process count

void initialize_memory_tracker() {
    // Allocate shared memory for the process count
    int count_shm_id = shmget(ftok("shmfile", 66), sizeof(int), IPC_CREAT | 0666);
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

    // Allocate shared memory for the process map
    int shm_id = shmget(ftok("shmfile", 65), sizeof(ProcessMemoryInfo) * MAX_PROCESSES, IPC_CREAT | 0666);
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

void cleanup_memory_tracker() {
    shmdt(process_map);
    shmdt(process_count);
}

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

ProcessMemoryInfo* find_process_info(pid_t pid) {
    for (int i = 0; i < *process_count; i++) {
        if (process_map[i].pid == pid) {
            return &process_map[i];
        }
    }
    return NULL;
}

void update_process_memory(pid_t pid, size_t size, int is_allocated) {
    ProcessMemoryInfo *process_info = find_process_info(pid);
    // If not found, create a new entry if there is space
    if (process_info == NULL) {
        if (*process_count < MAX_PROCESSES) {
            process_info = &process_map[*process_count];
            process_info->pid = pid;
            get_process_name(pid, process_info->process_name);
            process_info->allocated_size = 0;
            process_info->deallocated_size = 0;
            (*process_count)++; // Increment the count
        } else {
            fprintf(stderr, "Maximum process limit reached.\n");
            return;
        }
    }
    
    // Update allocated or deallocated sizes
    if (is_allocated) {
        process_info->allocated_size += size;
    } else {
        process_info->deallocated_size += size;
    }
    
    process_info->memory_leak = process_info->allocated_size - process_info->deallocated_size;
}

void remove_process(pid_t pid) {
    for (int i = 0; i < *process_count; i++) {
        if (process_map[i].pid == pid) {
            // Shift remaining processes down
            memmove(&process_map[i], &process_map[i + 1], (*process_count - i - 1) * sizeof(ProcessMemoryInfo));
            (*process_count)--; // Decrement the count
            break;
        }
    }
}

void *custom_malloc(size_t size) {
    void *ptr = malloc(size + sizeof(size_t));
    if (ptr) {
        if(process_map == NULL) initialize_memory_tracker();
        *((size_t *)ptr) = size;
        update_process_memory(getpid(), size, 1);
        return (char *)ptr + sizeof(size_t); // Return a pointer to the memory after the size
    }
    return NULL;
}

void custom_free(void *ptr) {
    if (ptr) {
        void *original_ptr = (char *)ptr - sizeof(size_t);
        size_t size = *((size_t *)original_ptr);
        update_process_memory(getpid(), size, 0); // Mark as deallocated
        free(original_ptr); // Free the original pointer
    }
}

void print_memory_info() {
    printf("PID\tProcess Name\tAllocated\tDeallocated\tMemory Leak\n");
    printf("--------------------------------------------------------\n");
    for (int i = 0; i < *process_count; i++) {
        ProcessMemoryInfo *p = &process_map[i];
        if (p->pid) {
            printf("%d\t%s \t\t%zu bytes \t%zu bytes \t%zu bytes\n",
                   p->pid,
                   p->process_name,
                   p->allocated_size,
                   p->deallocated_size,
                   p->memory_leak);
        }
    }
    printf("\n"); // Add a newline for better readability
}

#endif // MY_MEM_H

