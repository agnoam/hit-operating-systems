#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <ctype.h>
#include <sys/mman.h>
#include <sys/wait.h>

#define EXIT_OK 0 // Successful exit (no errors)
#define ERR_USAGE 64 // Command line usage error

#define VALID_ARGS_COUNT 3

int to_number(char* arg) {
    char* number_end;
    long num = strtol(arg, &number_end, 10);
    
    if (arg == NULL || *arg == '\0') {
        return -1;
    } else if (*number_end == '\0') {
        return atoi(arg);
    }

    // There are no numbers in the string or the argument contains mix of chars and digits
    return -1;
}

// This function returns whether the args parsed sucessfuly
int parse_input_args(int argc, char** argsv, int* target_number, int* semaphore_count) {
    if (argc != VALID_ARGS_COUNT) 
        return 0;

    // These arguments have to be poistive numbers. so (-1) is an parse error
    *target_number = to_number(argsv[1]);
    *semaphore_count = to_number(argsv[2]);

    return (*target_number >= 0 && *target_number < 10000) && (*semaphore_count > 0 && *semaphore_count <= 500);
}

int run_worker(int process_id, int target_number, int n_processes, sem_t* sem_array) {
    // Determine the starting number for this process
    // Process 0 prints 1, Process 1 prints 2 ... Process N-1 prints N
    int current_num = process_id + 1; 

    while (current_num <= target_number) {
        // waiting for the semaphore for it's turn and decrementing our semaphore by n_processes - 1
        for (int j = 0; j < n_processes - 1; j++)
            sem_wait(&sem_array[process_id]);

        printf("%d\n", current_num);
        fflush(stdout);

        // Increment ALL OTHER semaphores by 1
        for (int i = 0; i < n_processes; i++) {
            if (i != process_id)
                sem_post(&sem_array[i]);
        }

        // Jump to the next number this process is responsible for
        current_num += n_processes;
    }
}

int spawn_processes(int n_procceses, int target_number, sem_t* sem_array) {
    for (int i = 0; i < n_procceses; i++) {
        pid_t pid = fork();
        if (pid < 0) return 1;
        if (pid == 0) {
            // Child process code execution
            run_worker(i, target_number, n_procceses, sem_array);
            exit(EXIT_OK);
        }
    }
}

int main(int argc, char** argsv) {
    int target_number, semaphore_count;
    int valid_values = parse_input_args(argc, argsv, &target_number, &semaphore_count);

    if (!valid_values) {
        fprintf(stderr, "Wrong program usage.\n\n(use: `semaphore_sync <target-number> <semaphore-count>`)\n");
        fprintf(stderr, "Make sure the target-number > 0 and 0 < semaphore-count < 501 \n");
        return EXIT_FAILURE;
    }

    sem_t *sem_array = mmap(NULL, semaphore_count * sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (sem_array == MAP_FAILED) return EXIT_FAILURE;

    // We actually assigning a number to each semaphore inside the `sem_array` to run.
    for (int i = 0; i < semaphore_count; i++) {
        // The second parameter `1` indicates the semaphore is shared between PROCESSES
        if (sem_init(&sem_array[i], 1, semaphore_count - 1 - i) != 0)
            return EXIT_FAILURE;
    }

    spawn_processes(semaphore_count, target_number, sem_array);
    
    // Waiting for the processes to exit
    for (int i = 0; i < target_number; i++)
        wait(NULL);

    // Clean up semaphores and unmap shared memory
    for (int i = 0; i < semaphore_count; i++)
        sem_destroy(&sem_array[i]);

    munmap(sem_array, semaphore_count * sizeof(sem_t));

    return 0;
}