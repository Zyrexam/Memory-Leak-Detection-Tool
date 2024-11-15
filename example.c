#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include "my_mem.h"

// Get a single character input from the terminal without pressing Enter
char getch() {
    struct termios oldt, newt;
    char ch;
    tcgetattr(STDIN_FILENO, &oldt);           // Save terminal settings
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);         // Disable canonical mode and echo
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);  // Apply new terminal settings
    ch = getchar();                           // Get a single character
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);  // Restore terminal settings
    return ch;
}

int main() {
    initialize_memory_tracker();

    // Example processes (simulate real behavior)
    int *array1 = (int *)my_malloc(5 * sizeof(int));
    sleep(1); // Simulate time delay
    int *array2 = (int *)my_malloc(10 * sizeof(int));
    my_free(array1, 5 * sizeof(int));
    remove_process(getpid()); // Simulate process completion

    // Simulate memory leak
    my_free(array2, 5 * sizeof(int)); // Incorrect size
    remove_process(getpid());

    // Navigation loop
    char choice;
    do {
        clear_screen();
        printf("Memory Tracker Dashboard\n");
        printf("1. View Active Processes\n");
        printf("2. View Finished Processes\n");
        printf("q. Quit\n");
        printf("Enter your choice: ");
        choice = getch();

        switch (choice) {
            case '1':
                print_active_processes();
                break;
            case '2':
                print_finished_processes();
                break;
            case 'q':
                printf("Exiting...\n");
                break;
            default:
                printf("Invalid choice. Try again.\n");
                break;
        }
    } while (choice != 'q');

    cleanup_memory_tracker();
    return 0;
}
