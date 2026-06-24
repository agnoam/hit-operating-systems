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

/**
 * @brief Convert a string argument to an integer.
 *
 * @param arg Pointer to the input string.
 * 
 * @return Converted integer value on success, -1 on invalid input.
 */
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

/**
 * @brief Parse the command line arguments.
 *
 * @param argc Argument count from main.
 * @param argsv Argument vector from main.
 * @param target_number Output pointer for the target number to print.
 * @param semaphore_count Output pointer for the number of semaphores/processes.
 * 
 * @return 1 if arguments are valid, 0 otherwise.
 */
int parse_input_args(int argc, char** argsv, int* target_number, int* semaphore_count) {
    if (argc != VALID_ARGS_COUNT) 
        return 0;

    // These arguments have to be positive numbers. so (-1) is a parse error
    *target_number = to_number(argsv[1]);
    *semaphore_count = to_number(argsv[2]);

    return (*target_number >= 0 && *target_number < 10000) && (*semaphore_count > 0 && *semaphore_count <= 500);
}

/**
 * @brief Run a worker process to print every n-th number in sequence.
 *
 * Each worker waits for its semaphore and prints numbers assigned to it.
 *
 * @param process_id Worker process index (0-based).
 * @param target_number Last number to print.
 * @param n_processes Total number of worker processes.
 * @param sem_array Shared semaphore array used for synchronization.
 * 
 * @return 0 always.
 */
void run_worker(int process_id, int target_number, int n_processes, sem_t* sem_array) {
    // Determine the starting number for this process
    // Process 0 prints 1, Process 1 prints 2 ... Process N-1 prints N
    int current_num = process_id + 1; 

    while (current_num <= target_number) {
        // Waiting for the semaphore for its turn and decrementing our semaphore by `n_processes - 1`
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

/**
 * @brief Spawn the worker processes for printing numbers.
 *
 * @param n_procceses Number of worker processes to create.
 * @param target_number Last number to print.
 * @param sem_array Shared semaphore array used for synchronization.
 * 
 * @return 0 if successful, 1 on fork failure.
 */
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

    return 0;
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