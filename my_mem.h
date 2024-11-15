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
#include <sched.h>
#include <sys/types.h>
#include <time.h>

// ==== CONSTANTS AND MACROS ====
#define MAX_PROCESSES 1024
#define RESET_COLOR "\033[0m"
#define GREEN_COLOR "\033[32m"
#define RED_COLOR "\033[31m"
#define YELLOW_COLOR "\033[33m"

// ==== STRUCTURES ====
typedef struct {
    pid_t pid;                        // Process ID
    char process_name[256];           // Process name
    size_t allocated_size;            // Total allocated size
    size_t deallocated_size;          // Total deallocated size
    size_t memory_leak;               // Calculated memory leak
    char start_time[32];              // Start time
    char end_time[32];                // End time
} ProcessMemoryInfo;

// ==== GLOBAL VARIABLES ====
static ProcessMemoryInfo finished_process_map[MAX_PROCESSES]; // Circular buffer for finished processes
static int finished_process_count = 0;                        // Number of finished processes
static int finished_process_index = 0;                        // Current index in the circular buffer
static ProcessMemoryInfo *process_map = NULL;                 // Shared memory pointer for active processes
static int *process_count = NULL;                             // Shared memory pointer for process count

// ==== UTILITY FUNCTIONS ====
// Get current timestamp as a string
void get_current_time(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *time_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", time_info);
}

// Clear terminal screen
void clear_screen() {
    system("clear");
}

// Get process name from /proc
void get_process_name(pid_t pid, char *process_name) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    FILE *file = fopen(path, "r");
    if (file) {
        if (fgets(process_name, 256, file) != NULL) {
            process_name[strcspn(process_name, "\n")] = '\0';  // Remove newline
        } else {
            strncpy(process_name, "Unknown", 256);
        }
        fclose(file);
    } else {
        strncpy(process_name, "Unknown", 256);
    }
}

// ==== SHARED MEMORY INITIALIZATION AND CLEANUP ====
// Initialize shared memory for processes
void initialize_process() {
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
}

// Initialize memory tracker
void initialize_memory_tracker() {
    initialize_process();
    if (*process_count == 0) {
        *process_count = 0;
    }
    memset(process_map, 0, sizeof(ProcessMemoryInfo) * MAX_PROCESSES);
}

// Cleanup shared memory
void cleanup_memory_tracker() {
    if (process_map) shmdt(process_map);
    if (process_count) shmdt(process_count);
}

// ==== MEMORY ALLOCATION TRACKING ====
// Find process in shared memory
ProcessMemoryInfo* find_process(pid_t pid) {
    for (int i = 0; i < *process_count; i++) {
        if (process_map[i].pid == pid) {
            return &process_map[i];
        }
    }
    return NULL;
}

// Update memory allocation/deallocation
void update_process_memory(pid_t pid, size_t size, int is_allocated) {
    ProcessMemoryInfo *process_info = find_process(pid);
    if (process_info == NULL) {
        if (*process_count == MAX_PROCESSES) {
            remove_process(process_map[0].pid); // Remove oldest process
        }
        process_info = &process_map[*process_count];
        process_info->pid = pid;
        get_process_name(pid, process_info->process_name);
        process_info->allocated_size = 0;
        process_info->deallocated_size = 0;
        get_current_time(process_info->start_time, sizeof(process_info->start_time));
        (*process_count)++;
    }

    if (is_allocated) {
        process_info->allocated_size += size;
    } else {
        process_info->deallocated_size += size;
    }
    process_info->memory_leak = process_info->allocated_size - process_info->deallocated_size;
}

// Add finished process to circular buffer
void add_finished_process(const ProcessMemoryInfo *process_info) {
    finished_process_map[finished_process_index] = *process_info;
    finished_process_index = (finished_process_index + 1) % MAX_PROCESSES;
    if (finished_process_count < MAX_PROCESSES) {
        finished_process_count++;
    }
}

// Remove process from active list
void remove_process(pid_t pid) {
    for (int i = 0; i < *process_count; i++) {
        if (process_map[i].pid == pid) {
            ProcessMemoryInfo *completed_process = &process_map[i];
            get_current_time(completed_process->end_time, sizeof(completed_process->end_time));
            add_finished_process(completed_process);

            memmove(&process_map[i], &process_map[i + 1], (*process_count - i - 1) * sizeof(ProcessMemoryInfo));
            (*process_count)--;
            break;
        }
    }
}

// ==== MEMORY MANAGEMENT FUNCTIONS ====
void *my_malloc(size_t size) {
    if (!process_map) initialize_process();
    void *ptr = malloc(size);
    if (ptr) update_process_memory(getpid(), size, 1);
    return ptr;
}

void my_free(void *ptr, size_t size) {
    if (ptr) {
        update_process_memory(getpid(), size, 0);
        free(ptr);
    }
}

void *my_realloc(void *ptr, size_t old_size, size_t new_size) {
    if (!process_map) initialize_process();
    void *new_ptr = realloc(ptr, new_size);
    if (new_ptr) update_process_memory(getpid(), new_size - old_size, 1);
    return new_ptr;
}

void *my_calloc(size_t num, size_t size) {
    size_t total_size = num * size;
    void *ptr = malloc(total_size);
    if (ptr) {
        memset(ptr, 0, total_size);
        update_process_memory(getpid(), total_size, 1);
    }
    return ptr;
}

// ==== DISPLAY FUNCTIONS ====
// Print active processes
void print_active_processes() {
    printf(YELLOW_COLOR "Active Processes:\n" RESET_COLOR);
    printf("PID\tProcess Name\tAllocated\tDeallocated\tMemory Leak\tStart Time\n");
    printf("--------------------------------------------------------------------\n");
    for (int i = 0; i < *process_count; i++) {
        ProcessMemoryInfo *p = &process_map[i];
        printf("%d\t%s\t\t%zu bytes\t%zu bytes\t%zu bytes\t%s\n",
               p->pid, p->process_name, p->allocated_size, p->deallocated_size,
               p->memory_leak, p->start_time);
    }
}

// Print finished processes
void print_finished_processes() {
    printf(YELLOW_COLOR "Finished Processes:\n" RESET_COLOR);
    printf("PID\tProcess Name\tAllocated\tDeallocated\tMemory Leak\tStart Time\t\tEnd Time\n");
    printf("--------------------------------------------------------------------------------------------\n");
    for (int i = 0; i < finished_process_count; i++) {
        int index = (finished_process_index + i) % MAX_PROCESSES;
        ProcessMemoryInfo *p = &finished_process_map[index];
        printf("%d\t%s\t\t%zu bytes\t%zu bytes\t%zu bytes\t%s\t%s\n",
               p->pid, p->process_name, p->allocated_size, p->deallocated_size,
               p->memory_leak, p->start_time, p->end_time);
    }
}

// Log memory leak
void log_memory_leak(const ProcessMemoryInfo *process_info) {
    if (process_info->memory_leak > 0) {
        printf(RED_COLOR "Memory Leak Detected:\n" RESET_COLOR);
        printf("PID: %d, Process Name: %s, Leak: %zu bytes\n",
               process_info->pid, process_info->process_name, process_info->memory_leak);
    }
}

#endif
