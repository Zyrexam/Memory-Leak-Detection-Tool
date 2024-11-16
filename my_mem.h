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
#include <time.h>
#include <sys/types.h>

#define MAX_PROCESSES 1024 

// Struct to store info of memory
typedef struct {
    pid_t pid;                          // Process ID
    char process_name[256];             // Process name
    size_t allocated_size;               // Total allocated size
    size_t deallocated_size;             // Total deallocated size
    size_t memory_leak;                  // Calculated memory leak  
    int date;
    int month;
    int year;
    int hour;
    int min;
} ProcessMemoryInfo;

static ProcessMemoryInfo *process_map = NULL; // Pointer for shared memory of process_map
static int *process_count = NULL; // Pointer for shared memory to store process count

// Access Shared memory when a new process wants memory
void initialize_process(){
    int count_shm_id = shmget(ftok("/tmp/shmfile_count",64), sizeof(int), IPC_CREAT | 0666);
    if (count_shm_id < 0) {
        perror("shmget for process count failed");
        exit(1);
    }
    process_count = (int *)shmat(count_shm_id, NULL, 0);
    if (process_count == (int *)(-1)) {
        perror("shmat for process count failed");
        exit(1);
    }
    int shm_id = shmget(12345, sizeof(ProcessMemoryInfo) * MAX_PROCESSES, IPC_CREAT | 0666);
    if (shm_id < 0) {
        perror("shmget for process map failed");
        exit(1);
    }
    process_map = (ProcessMemoryInfo *)shmat(shm_id, NULL, 0);
    if (process_map == (ProcessMemoryInfo *)(-1)) {
        perror("shmat for process map failed");
        exit(1);
    }
}
void initialize_memory_tracker() {
    *process_count = 0; // Initialize process count to 0
    memset(process_map, 0, sizeof(ProcessMemoryInfo) * MAX_PROCESSES); // Initialize the shared memory
}

void cleanup_memory_tracker() { // Detaching Shared Memory
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
            // Shift remaining processes down
            memmove(&process_map[i], &process_map[i + 1], (*process_count - i - 1) * sizeof(ProcessMemoryInfo));
            (*process_count)--; // Decrement the count
            break;
        }
    }
}

// Updating Shared Memory Based on Allocation and Deallocation 
void update_process_memory(pid_t pid, size_t size, int is_allocated) {
    ProcessMemoryInfo *process_info = find_process(pid);
    if (process_info == NULL) {
        if (*process_count == MAX_PROCESSES) {  // If MAX_PROCESSES is reached, remove the first process
            remove_process(process_map[0].pid);
        }
        process_info = &process_map[*process_count];
        process_info->pid = pid;
        get_process_name(pid, process_info->process_name);
        process_info->allocated_size = 0;
        process_info->deallocated_size = 0;
        (*process_count)++; // Increment process count
    }

    // Update allocated or deallocated sizes
    if (is_allocated) {
        process_info->allocated_size += size;
    } else {
        process_info->deallocated_size += size;
    }

    process_info->memory_leak = process_info->allocated_size - process_info->deallocated_size;
    time_t now = time(NULL);              // Getting the current time
    struct tm *current_time = localtime(&now);
    process_info->date = current_time->tm_mday;
    process_info->year = current_time->tm_year + 1900;
    process_info->month = current_time->tm_mon + 1;
    process_info->hour = current_time->tm_hour;
    process_info->min = current_time->tm_min;
}

void *my_malloc(size_t size) {
    if(!process_map) initialize_process();
    void *ptr = malloc(size);
    if (ptr) {
        update_process_memory(getpid(), size, 1); // Shared Memory Allocate
        return ptr; 
    }
    return NULL;
}

void my_free(void *ptr, size_t size) {
    if (ptr) {
        update_process_memory(getpid(), size, 0); // Mark as deallocated
        free(ptr); 
    }
}

void *my_realloc(void *ptr, size_t old_size, size_t new_size) {
    if(!process_map) initialize_process();
    void *new_ptr = realloc(ptr, new_size); // Reallocate memory
    if (new_ptr) {
        update_process_memory(getpid(), new_size - old_size, 1); 
        return new_ptr;
    }
    return NULL;
}

void *my_calloc(size_t num, size_t size) {
    size_t total_size = num * size;
    void *ptr = malloc(total_size); 
    if (ptr) {
        memset(ptr, 0, total_size); // Set allocated memory to zero
        update_process_memory(getpid(), total_size, 1); 
        return ptr;
    }
    return NULL;
}


// Monitor Printing
void print_memory_info() {
    printf("PID\tProcess Name\tAllocated (Bytes)\tDeallocated (Bytes)\tMemory Leak \t  Date \t\tTime\n");
    printf("---------------------------------------------------------------------------------------------------------------------\n");

    for (int i = 0; i < *process_count; i++) {
        ProcessMemoryInfo *p = &process_map[i];
        if (p->pid) {
            size_t leak = p->memory_leak;
            double leak_percent = p->allocated_size ? (double)leak / p->allocated_size * 100 : 0;

            // Determine color for memory leak
            const char *color = "\033[0m"; // Default color
            if (leak_percent > 50.0) {
                color = "\033[1;31m"; // Red
            } else if (leak_percent > 10.0) {
                color = "\033[1;33m"; // Yellow
            } else {
                color = "\033[1;32m"; // Green
            }

            // Use fixed-width formatting while wrapping the memory leak column with the color
            printf("%d\t%-15s\t%-20zu\t%-20zu\t%s%-zu (%.2f%%)\033[0m\t %02d-%02d-%04d\t%02d:%02d\n",
                     p->pid,                     // PID
                     p->process_name,            // Process name
                     p->allocated_size,          // Allocated memory
                     p->deallocated_size,        // Deallocated memory
                     color,                      // Color start
                     p->memory_leak,             // Memory leak value
                     leak_percent,               // Memory leak percentage
                     p->date,                     // Day
                     p->month,                   // Month
                     p->year,                    // Year
                     p->hour,                    // Hour
                     p->min);                    // Minute
        }
    }
    printf("\n");
}

#endif 

