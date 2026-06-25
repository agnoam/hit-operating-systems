#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/wait.h>

#define VALID_ARGS_COUNT 3

// Semaphore control sheet
union semun {
    int val;
    struct semid_ds* buf;
    unsigned short* array;
    struct seminfo* __buf;
};

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

int init_semaphores(int n_processes) {
    union semun arg;
    
    // Creating the semaphore in private mode
    int semid = semget(IPC_PRIVATE, n_processes, IPC_CREAT | IPC_EXCL | 0600);
    if (semid < 0)
        return -1;

    // Allocating an array that holds multiple semaphores
    unsigned short* values = malloc(sizeof(unsigned short) * n_processes);
    if (values == NULL) {
        // Removing the semaphore in case of failure
        semctl(semid, 0, IPC_RMID);
        return -1;
    }

    // Setting a value for each semaphore
    for (int i = 0; i < n_processes; i++)
        values[i] = (unsigned short)(n_processes - 1 - i);

    arg.array = values;
    if (semctl(semid, 0, SETALL, arg) < 0) {
        semctl(semid, 0, IPC_RMID);
        free(values);
        return -1;
    }

    free(values);
    return semid;
}

int destroy_semaphores(int semid) {
    return semctl(semid, 0, IPC_RMID);
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
int semaphore_wait(int semid, int semnum) {
    struct sembuf op = { .sem_num = semnum, .sem_op = -1, .sem_flg = 0 };
    return semop(semid, &op, 1);
}

int semaphore_post(int semid, int semnum) {
    struct sembuf op = { .sem_num = semnum, .sem_op = 1, .sem_flg = 0 };
    return semop(semid, &op, 1);
}

void run_worker(int process_id, int target_number, int n_processes, int semid) {
    // Determine the starting number for this process
    // Process 0 prints 1, Process 1 prints 2 ... Process N-1 prints N
    int current_num = process_id + 1;

    while (current_num <= target_number) {
        for (int j = 0; j < n_processes - 1; j++) {
            if (semaphore_wait(semid, process_id) < 0)
                exit(EXIT_FAILURE);
        }

        printf("%d\n", current_num);
        fflush(stdout);

        for (int i = 0; i < n_processes; i++) {
            if (i != process_id) {
                if (semaphore_post(semid, i) < 0)
                    exit(EXIT_FAILURE);
            }
        }

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
int spawn_processes(int n_processes, int target_number, int semid) {
    for (int i = 0; i < n_processes; i++) {
        pid_t pid = fork();
        if (pid < 0) return 1;
        if (pid == 0) {
            // Child process code execution
            run_worker(i, target_number, n_processes, semid);
            exit(EXIT_SUCCESS);
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

    int semid = init_semaphores(semaphore_count);
    if (semid < 0) return EXIT_FAILURE;

    if (spawn_processes(semaphore_count, target_number, semid) != 0) {
        destroy_semaphores(semid);
        return EXIT_FAILURE;
    }
    
    // Waiting for the processes to exit
    for (int i = 0; i < semaphore_count; i++)
        wait(NULL);

    destroy_semaphores(semid);

    return 0;
}