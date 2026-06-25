#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h> // Interprocess communication
#include <sys/sem.h> // Using semaphore
#include <sys/wait.h>
#include <sys/shm.h>

#define VALID_ARGS_COUNT 2 // Including the executable name

// Semaphore control sheet
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
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
 * @brief Parses and validates the command-line arguments to extract the target number.
 *
 * This function ensures that the minimum required number of arguments are passed 
 * and verifies that the provided argument can be converted into a valid, positive integer.
 *
 * @param argc  The total number of command-line arguments provided to the program.
 * @param argsv An array of null-terminated strings representing the command-line arguments.
 *
 * @return The converted integer value N (where N >= 1) on successful validation.
 * @retval 1        Returned if fewer than two arguments are provided (missing argument).
 * @retval EXIT_ERR Returned if the argument fails conversion or evaluates to less than 1.
 *
 * @note This function expects `argsv[1]` to be a numeric string format parseable by `to_number`.
 * @see to_number
 */
int parse_args(int argc, char** argsv) {
    int N;

    if (argc < 2) 
        exit(EXIT_FAILURE);
    
    N = to_number(argsv[1]);
    
    if (N < 1) 
        exit(EXIT_FAILURE);
    return N;
}

/**
 * @brief Initialize a System V semaphore and prepare operation template.
 *
 * This function creates a private semaphore (single semaphore set with
 * one semaphore), initializes its value to unlocked (1) and fills the
 * provided `semaphore_ops` structure with the basic fields needed for
 * subsequent `semop()` calls.
 * 
 * WARNING! The function overwriting the given pointers
 *
 * @param semaphore_id Pointer that receives the created semaphore id.
 * @param semaphore_arg Pointer to a `union semun` used to set the
 *                           initial semaphore value.
 * @param semaphore_ops Pointer to a `struct sembuf` which will be
 *                           configured with `sem_num` and `sem_flg`.
 */
void setup_semaphore(int* semaphore_id, union semun* semaphore_arg, struct sembuf* semaphore_ops) {
    // Creating a application specific "PRIVATE" seamphore
    *semaphore_id = semget(IPC_PRIVATE, 1, 0600);
    semaphore_arg->val = 1; // Setting the semaphore to be unlocked
    semctl(*semaphore_id, 0, SETVAL, *semaphore_arg); // Resetting the semaphore (cleaning the garbage value)

    // Setup the operation configurations
    semaphore_ops->sem_num = 0; // Using just one semaphore (index 0)
    semaphore_ops-> sem_flg = 0; // No special behaviour flags
}


/**
 * @brief Allocate a shared memory segment for a single integer and attach it.
 *
 * Allocates an anonymous (`IPC_PRIVATE`) shared memory segment sized for one
 * `int`, attaches it into the caller's address space and initializes the
 * value to zero.
 *
 * @param global_sum Pointer to an `int*` which will point to the
 *                        attached shared integer on success.
 * 
 * @return The shared memory id (shmid) on success. On failure the
 *         function prints an error and exits the process.
 */
int allocate_shared_memory(int** global_sum) {
    /* 
        Request a block of shared memory large enough to hold 1 integer
        `0600` value grants Read/Write permissions to the owner process
    */
    int shared_memory_id = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0600);

    /* 
        Mapping the segment into the current process's memory space (and it's child processes) using `shmat`
        `NULL` property lets the OS choose the best memory address automatically
        0 means read/write mode
    */
    *global_sum = (int*) shmat(shared_memory_id, NULL, 0);

    // Resetting the value inside the shared memory
    **global_sum = 0;
    return shared_memory_id;
}


/**
 * @brief Fork a child that safely updates the shared sum.
 *
 * The child process locks the semaphore, updates the shared integer by
 * adding `number_to_add`, releases the semaphore, detaches from shared
 * memory and exits. The parent returns immediately after the fork.
 *
 * @param number_to_add Value to add to the shared sum inside the child.
 * @param shared_sum Pointer to the shared integer holding the global sum.
 * @param semaphore_id Semaphore identifier returned by `semget()`.
 * @param semaphore_ops Pointer to a `struct sembuf` used for `semop()`
 *                      operations. Its `sem_op` field will be set by this
 *                      function before calling `semop()`.
 */
void spawn_process(int number_to_add, int* shared_sum, int semaphore_id, struct sembuf* semaphore_ops) {
    pid_t pid = fork();
    if (pid == 0) {
        // Running inside child process

        /* 
            Making the semaphore to be busy by substracting (-1) from the current value of the semaphore (1).
            Hence: (1) + (-1) = 0 -> Which symobls that the semaphore is busy
        */
        semaphore_ops->sem_op = -1;
        semop(semaphore_id, semaphore_ops, 1); // We are changing just one semaphore at a time, so the 3rd arg is 1
        
        // CRITICAL SECTION: Updating the shared sum value inside the pointer
        *shared_sum += number_to_add; 
        
        semaphore_ops->sem_op = 1; // Changing the semaphore to be available (adding one to the semaphore)
        semop(semaphore_id, semaphore_ops, 1); // Releasing the segment
        
        // Detach this child's pointer from the memory
        shmdt(shared_sum);

        exit(EXIT_SUCCESS);
    }
}


/**
 * The program creates a semaphore and a shared integer, then spawns
 * N child processes (where N is the numeric command-line argument).
 * Each child address its index to the shared sum while protected by the
 * semaphore. 
 * 
 * The parent waits for all children, prints the resulting sum
 * and performs cleanup of IPC resources.
 */
int main(int argc, char** argsv) {
    int N, semaphore_id, shared_memory_id, i;
    int* shared_sum;
    union semun semaphore_arg;
    struct sembuf semaphore_ops[1];
    
    N = parse_args(argc, argsv);
    setup_semaphore(&semaphore_id, &semaphore_arg, semaphore_ops);
    shared_memory_id = allocate_shared_memory(&shared_sum);
    for (i = 0; i < N; i++)
        spawn_process(i, shared_sum, semaphore_id, semaphore_ops);
    
    // Parent process waits for the child process to end
    while (wait(NULL) > 0) {}
    printf("%d\n", *shared_sum);
    
    // Cleaning up the created semaphore
    semctl(semaphore_id, 0, IPC_RMID, semaphore_arg);
    
    // Cleaning up the shared memory
    shmdt(shared_sum); // Detaching the shared memory from the parent process
    shmctl(shared_memory_id, IPC_RMID, NULL);  // Releasing the shared memory allocation from the OS

    return 0;
}