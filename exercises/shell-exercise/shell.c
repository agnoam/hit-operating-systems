#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#define EXIT_OK 0 // Equivalent to EXIT_SUCCESS
#define ERR_USAGE 64 // Command line usage error
#define ERR_DATA 65 // Data format error
#define ERR_NO_INPUT 66 // Cannot open input file
#define ERR_UNAVAILABLE 69 // Resource unavailable
#define EX_FAILURE 1 // General catch-all error

#define PATH_SEPERATOR '/'
#define PROMPT_BUFFER_SIZE 1000

void clear_screen() {
    // The function should clear the terminal screen.
    // You can use ANSI escape codes to achieve this.
    printf("\033[H\033[J");
}

int is_executable_file(const char *path) {
    struct stat sb;
    
    if (stat(path, &sb) != 0) return 0;
    if (S_ISDIR(sb.st_mode)) return 0; // It's a directory, don't try to execute it
    if (S_ISREG(sb.st_mode) && (access(path, X_OK) == 0)) return 1; // Safe to pass to execv()
    
    return 0;
}

/**
 * 
 */
char* join_path(const char *dir, const char *file) {
    size_t dir_len = strlen(dir);
    size_t file_len = strlen(file);
    int needs_sep = (dir_len > 0 && dir[dir_len - 1] != PATH_SEPERATOR) ? 1 : 0;

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

/*
    The function checks if the command is valid by searching for it in the directories listed in the `PATH` environment variable.
    In case the function found a file with exectuion permissions, it returns 1. Otherwise, it returns 0.

    Args:
        - `filename`: The name of the command to check for validity.
    
    Returns:
        - Full path in case the command is valid
        - NULL if the command is invalid
*/
char* construct_path(char* filename) {
    char* path_env = getenv("PATH");
    if (!path_env) return NULL;

    // Duplicate PATH because strtok_r will modify the string
    char* path_copy = strdup(path_env);
    if (!path_copy) return NULL;

    char* saveptr = NULL;
    char* dir = strtok_r(path_copy, ":", &saveptr);
    char* full_path = NULL;

    while (dir != NULL) {
        full_path = join_path(dir, filename);
        if (full_path == NULL) {
            dir = strtok_r(NULL, ":", &saveptr);
            continue;
        }

        if (is_executable_file(full_path)) {
            free(path_copy);
            return full_path;
        }

        free(full_path);
        full_path = NULL;
        dir = strtok_r(NULL, ":", &saveptr);
    }

    free(path_copy);
    return NULL;
}

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

/*
    Extracts the filename from the command term. 
    The filename is the first word in the term, until the first space character.

    Note: This function modifies the filename array.

    Args:
        - term: The command term from which to extract the filename.
        - filename: The buffer where the extracted filename will be stored.

    Returns:
        The pointer to the end of the extracted filename or NULL in case it just filename
*/
char* extract_filename(char* term, size_t term_size) {
    char *result = strchr(term, ' ');
    if (result && (result - term) < term_size)
        term[result - term] = '\0';

    return result;
}

void parse_args(char* filename, char* args_str, char** args) {
    int i = 0;
    char* token;

    args[i++] = filename;

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

/*
    Reads a command line from stdin, strips the trailing newline.
    
    Args:
        - buffer: Buffer to store the command (must be at least size bytes)
        - size: Size of the buffer
    
    Returns:
        Pointer to buffer on success, or NULL on EOF
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
    char* full_path;
    char* filename_last_char = NULL;
    char** args = malloc(PROMPT_BUFFER_SIZE * sizeof(char*));

    clear_screen();
    while (1) {
        printf("(my-shell)$ ");
        fflush(stdout); // Updates the terminal

        if (read_command(term, sizeof(term)) == NULL) break;
        if (strcmp(term, "leave") == 0) break;
        
        filename_last_char = extract_filename(term, sizeof(term));
        full_path = construct_path(term);
        if (!full_path) {
            fprintf(stderr, "Command not found\n");
            continue;
        }

        if (filename_last_char != NULL) {
            parse_args(term, filename_last_char+1, args);
        } else {
            args[0] = term;
            args[1] = NULL;
        }

        execute(full_path, args);
        
        free(full_path);
        full_path = NULL;

        memset(args, 0, PROMPT_BUFFER_SIZE * sizeof(char*)); // Clear the args buffer
    }
    
    // Cleanup space
    free(args);
    return EXIT_OK;
}