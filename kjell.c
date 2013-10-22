#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>

/**
* Check if there is an error, if yes, exit with 1.
*/ 
void checkerr(int err, char *message) {
    if (err == -1) {
        perror(message);
        exit(1); 
    }
}

void execute(char **program) {
    execvp(program[0], program);
        
    if (errno == ENOENT) {
        printf("kjell: command not found: %s\n", program[0]);
    } else {
        printf("kjell: failed to start command: %s\n", program[0]);
    }
    
    exit(1);
}

void run_foreground(char** program) {
    pid_t pid = fork();
    checkerr(pid, "Cannot fork");

    if (pid == 0) {
        /* Allow SIGINT in child */
        signal(SIGINT, SIG_DFL);
        execute(program);
    } else {       
        int status;
        int r = waitpid(pid, &status, 0);
        checkerr(r, "waitpid() failed unexpectedly");
    }
}

pid_t run_background(char **program) {
    pid_t pid = fork();
    checkerr(pid, "Cannot fork");

    if (pid == 0) {
        execute(program);
    }

    return pid;
}

typedef struct {
    char line[128];
    char *argv[10];
    int background;
} command_t;

int read_command(command_t *command) {
    if (fgets(command->line, sizeof command->line, stdin)) {
        int argc = 0;
        char **ap, **argv = command->argv, *input = command->line;
        for (ap = argv; (*ap = strsep(&input, " \t\n")) != NULL;)
            if (**ap != '\0') {
                argc++;
                if (++ap >= &argv[sizeof command->argv]) break;
            }

        if (argc == 0) {
            return -1;
        }

        if (strcmp(argv[argc - 1], "&") == 0) {
            command->background = 1;
            argv[argc - 1] = '\0';
        } else {
            command->background = 0;
        }

        return argc;
    }

    return -1;
}

int check_bg_processes(int num_processes) {
    int status;
    pid_t pid = waitpid(-1, &status, WNOHANG);
    checkerr(pid, "waitpid() failed unexpectedly");
    if (pid > 0) {
        if (WEXITSTATUS(status) == 0) {
            printf("[?] %d done\n", pid);
        } else {
            printf("[?] %d exit %d\n", pid, WEXITSTATUS(status));
        }
        num_processes--;
    }

    return num_processes;
}

void cd(char *dir) {
    if (chdir(dir) == -1) {
        if (errno == ENOENT) {
            printf("kjell: no such file or directory: %s\n", dir);
        } else {
            printf("kjell: failed to change directory\n");
        }
    }
}

int execute_builtin(char *name, char **argv) {
    if (strcmp(name, "cd") == 0) {
        cd(argv[1]);
    } else if (strcmp(name, "exit") == 0) {
        exit(1);
    } else {
        return 0;
    }

    return 1;
}

char* resolve_home(char* path) {
    char* home = getenv("HOME");
    if (home == NULL) {
        return path;
    }

    size_t home_len = strlen(home);
    if (strncmp(path, home, home_len) == 0) {
        path[home_len - 1] = '~';
        return &path[home_len - 1];
    } else {
        return path;
    }
}

void print_prompt() {
    char path[128];
    if (getcwd(path, 128) != NULL) {
        printf("%s$ ", resolve_home(path));
    } else {
        printf("$ ");
    }
}

int main(int argc, char** argv) {
    signal(SIGINT, SIG_IGN);

    int bg_processes = 0;

    while (1) {
        command_t command;

        if (bg_processes > 0) {
            bg_processes = check_bg_processes(bg_processes);
        }

        print_prompt();
        if (read_command(&command) == -1) {
            continue;
        }

        if (!execute_builtin(command.argv[0], command.argv)) {
            if (command.background) {
                pid_t pid = run_background(command.argv);
                printf("[?] %d\n", pid);
                bg_processes++;
            } else {
                run_foreground(command.argv);
            }
        }
    }
    return 0;
}
