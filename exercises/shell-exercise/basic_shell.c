#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#define EXIT_OK 0 // Successful exit (no errors)
#define ERR_USAGE 64 // Command line usage error
#define ERR_DATA 65 // Data format error
#define ERR_NO_INPUT 66 // Cannot open input file
#define ERR_UNAVAILABLE 69 // Resource unavailable
#define EX_FAILURE 1 // General catch-all error

#define PATH_SEPERATOR '/'
#define PROMPT_BUFFER_SIZE 1000

/**
 * @brief Clear the terminal screen.
 *
 * Uses ANSI escape sequences to reposition the cursor and clear the display.
 */
void clear_screen() {
    printf("\033[H\033[J");
}

/**
 * @brief Determine whether a path refers to an executable regular file.
 *
 * @param path Filesystem path to test.
 * @return 1 if the path is a regular file and is executable by the current
 *         process, 0 otherwise.
 */
int is_executable_file(const char *path) {
    struct stat sb;
    
    if (stat(path, &sb) != 0) return 0;
    if (S_ISDIR(sb.st_mode)) return 0; // It's a directory, don't try to execute it
    if (S_ISREG(sb.st_mode) && (access(path, X_OK) == 0)) return 1; // Safe to pass to execv()
    
    return 0;
}

/**
 * @brief Join a directory and filename into a single path string.
 *
 * The function will insert a path separator between `dir` and `file` if
 * required. The returned string is heap-allocated and must be freed by the
 * caller.
 *
 * @param dir Directory component (may be an empty string).
 * @param file Filename component.
 * @return Newly allocated string containing the joined path, or NULL on
 *         allocation failure.
 */
char* join_path(const char *dir, const char *file) {
    size_t dir_len = strlen(dir);
    size_t file_len = strlen(file);
    int needs_sep = (dir_len > 0 && dir[dir_len - 1] != PATH_SEPERATOR);

    // Allocate memory: dir + separator (optional) + file + null terminator
    char *result = calloc(dir_len + needs_sep + file_len + 1, sizeof(char));
    if (!result) return NULL;

    strcpy(result, dir);
    if (needs_sep) {
        result[dir_len] = PATH_SEPERATOR;
        result[dir_len + 1] = '\0'; // Placeholder for the `strcat` function
    }
    
    strcat(result, file);
    return result;
}

/**
 * @brief Search the PATH environment for an executable matching `filename`.
 *
 * Iterates over directories in the PATH environment variable and checks
 * whether an executable file named `filename` exists in any of them.
 *
 * @param filename Name of the command to look up (not a path).
 * @return Heap-allocated full path to the executable if found (caller must
 * free), or NULL if not found or on error.
 */
char* construct_path(char* filename) {
    char* path_env = getenv("PATH");
    char *path_copy = NULL, *dir = NULL, *full_path = NULL;

    if (!path_env || !filename) return NULL;

    path_copy = strdup(path_env);
    if (!path_copy) return NULL;

    // Use standard strtok as requested by the strict instructions
    dir = strtok(path_copy, ":");
    
    while (dir != NULL) {
        full_path = join_path(dir, filename);
        if (is_executable_file(full_path)) {
            free(path_copy);
            return full_path;
        }

        free(full_path);
        dir = strtok(NULL, ":");
    }

    free(path_copy);
    return NULL;
}

/**
 * @brief Fork and execute the program at `path` with arguments `args`.
 *
 * This function forks the current process; the child calls `execv` to
 * replace itself with the requested program. The parent waits for the child
 * to terminate.
 *
 * @param path Path to executable.
 * @param args NULL-terminated argument vector; `args[0]` should typically be
 *             the program name.
 */
void execute(char* path, char** args) {
    pid_t pid;
    pid = fork();

    if (pid < 0) {
        perror("Unable to spawn child process");
        exit(1);
    } else if (pid == 0) {
        // We are running inside the child process now.
        if (execv(path, args) == -1) {
            perror("Unable to execute the command");
            exit(1);
        }
        /*  
            In case the execution was successful, the child process will be replaced by the new process, 
            so we won't reach this point.
        */
    } else {
        // Should wait for the child process to finish before continuing.
        wait(NULL);
    }
}

/**
 * @brief Extract the command name (first token) from a command line.
 *
 * Copies the first whitespace-delimited token from `term` into the
 * supplied `filename` buffer. The function modifies neither `term` nor
 * `filename` beyond writing the filename characters.
 *
 * @param term Input command line string.
 * @param filename Output buffer to receive the extracted filename. The
 *                 caller is responsible for ensuring it is sufficiently
 *                 large.
 * @return Pointer to the position in `term` immediately following the
 *         extracted filename (or NULL if the entire term was the filename).
 */
char* extract_filename(char* term, char* filename) {
    char *result = strchr(term, ' ');
    if (result) {
        size_t len = result - term;
        strncpy(filename, term, len);
        filename[len] = '\0'; // Explicitly add null terminator
    } else {
        strcpy(filename, term);
    }
    
    return result;
}

/**
 * @brief Parse a space-separated argument string into an argv-style array.
 *
 * Tokenizes `args_str` in-place using `strtok` and stores pointers to each
 * token into `args`. The resulting array is NULL-terminated.
 *
 * @param args_str Mutable string containing arguments separated by spaces.
 * @param args Output array of char* pointers; must be large enough to hold
 *             all tokens plus a terminating NULL.
 */
void parse_args(char* args_str, char** args) {
    int i = 0;
    char* token;

    // Tokenize remaining args; leave room for terminating NULL
    if (args_str) {
        token = strtok(args_str, " ");
        while (token != NULL) {
            args[i++] = token;
            token = strtok(NULL, " "); // Iterating to the next one
        }
    }

    args[i] = NULL;
}

/**
 * @brief Read a line from stdin into `buffer`, removing the newline.
 *
 * @param buffer Destination buffer for the line.
 * @param size Size of `buffer` in bytes.
 * 
 * @return `buffer` on success, or NULL on EOF or error.
 */
char* read_command(char* buffer, size_t size) {
    if (fgets(buffer, size, stdin) == NULL) {
        return NULL;
    }
    
    // Remove trailing newline
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }
    
    return buffer;
}

int main() {    
    char term[PROMPT_BUFFER_SIZE];
    char* filename = calloc(PROMPT_BUFFER_SIZE, sizeof(char));
    char* full_path = NULL;
    char* filename_last_char = NULL;
    char** args = malloc(PROMPT_BUFFER_SIZE * sizeof(char*));

    clear_screen();
    while (1) {
        printf("(my-shell)$ ");
        fflush(stdout); // Updates the terminal

        if (read_command(term, sizeof(term)) == NULL) break;
        if (strlen(term) == 0) continue; // Ignore empty input
        if (strcmp(term, "leave") == 0) break;
        
        extract_filename(term, filename);
        full_path = construct_path(filename);
        if (!full_path) {
            fprintf(stderr, "Command not found\n");
            continue;
        }

        parse_args(term, args);
        execute(full_path, args);
        
        free(full_path);
        full_path = NULL;

        memset(args, 0, PROMPT_BUFFER_SIZE * sizeof(char*)); // Clear the args buffer
    }
    
    // Cleanup space
    free(args);
    return EXIT_OK;
}