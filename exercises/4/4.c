#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <pwd.h>
#include <string.h>

void show_help() {
    printf("Usage: ./4 [OPTIONAL OPTION]\n");
    printf("Options:\n");
    printf("  --use-password-file, -p   Use the password file to fetch all available user ids and set nice value for them [default]\n");
    printf("  --brute-force, -b   Use brute-force method to set nice value for all possible user ids (0 - 65535)\n");
    printf("  every other argument         Show this help message\n");
}

/* 
    In order to fetch a uid to set the Nice value (priority) for a different user than the current user,
    the program must to run via `sudo` or a privileged user.
    
    If so, instead of trying all the available uids by iterating over all of the possible uids which is not efficient (0 - 65535).
    The script assumes that all of the relevant users exists in the password database and will directly access. 
    which contains all of the users information to fetch all the available uids without iterating over non-existent ones.
    
    Then, iterate all the found uids and set the nice value for each of them.
    For demonstration purpose, the script only set the nice value to be `5`.
*/
void change_uid_priority(int user_id, int nice_value) {
    int res = setpriority(PRIO_USER, user_id, nice_value); 
    if (res == -1) {
        /* 
            The availble `errno` values are: `EPERM` - No permission, `ESRCH` - No such process.
            Instead of writing custom message, I'm using the built-in function to convert the error code to a human-readable string.
        */
        printf("Error number (%d): %s\n", errno, strerror(errno));
    }
}

void show_priority(int user_id) {
    int pri = getpriority(PRIO_USER, user_id);
    printf("[user_id - %d]: [priority - %d]\n", user_id, pri);
}

void use_password_file(int nice_value) {
    struct passwd *pwd;

    // Iterating over all of the available users in the system
    while ((pwd = getpwent()) != NULL) {
        printf("priority before change:\n");
        show_priority(pwd->pw_uid);
        change_uid_priority(pwd->pw_uid, nice_value);
        show_priority(pwd->pw_uid);
        printf("\n\n");
    }
    
    endpwent();
}

void brute_force(int nice_value) {
    for (int user_id = 0; user_id <= 65535; user_id++) {
        printf("priority before change:\n");
        show_priority(user_id);
        change_uid_priority(user_id, nice_value);
        show_priority(user_id);
        printf("\n\n");
    }
}

int main(int argc, char *argv[]) {
    int nice_value = 5;
    int mode = 1; // default mode is to use the password file
    
    if (argc > 1) {
        if (strcmp(argv[1], "--brute-force") == 0 || strcmp(argv[1], "-b") == 0) {
            mode = 0;
        } else if(strcmp(argv[1], "--use-password-file") == 0 || strcmp(argv[1], "-p") == 0) {
            mode = 1;
        } else {
            show_help();
            return 0;
        }
    }

    printf("The program is running under user id: %d\n", getuid());
    printf("--- Starting to update NICE values for ALL users ---\n");
    
    switch (mode) {
        case 0:
            brute_force(nice_value);
            break;

        default:
            use_password_file(nice_value);   
    }
    
    return 0;
}
