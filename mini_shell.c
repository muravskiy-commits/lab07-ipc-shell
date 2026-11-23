#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_LINE 1024
#define MAX_ARGS 64

static int parse_args(char *segment, char **argv)
{
    int argc = 0;
    char *token = strtok(segment, " \t\r\n");
    while (token != NULL && argc < MAX_ARGS - 1) {
        argv[argc++] = token;
        token = strtok(NULL, " \t\r\n");
    }
    argv[argc] = NULL;
    return argc;
}

static void execute_single_command(char *cmd_str)
{
    char *redirect_file = NULL;
    char *gt_pos = strchr(cmd_str, '>');

    if (gt_pos != NULL) {
        *gt_pos = '\0';
        redirect_file = strtok(gt_pos + 1, " \t\r\n");
        if (redirect_file == NULL) {
            fprintf(stderr, "Syntax error: expected file after '>'\n");
            return;
        }
    }

    char *argv[MAX_ARGS];
    int argc = parse_args(cmd_str, argv);
    if (argc == 0) {
        return;
    }

    /* Built-in: exit */
    if (strcmp(argv[0], "exit") == 0) {
        exit(EXIT_SUCCESS);
    }

    /* Built-in: cd */
    if (strcmp(argv[0], "cd") == 0) {
        const char *target = (argc > 1) ? argv[1] : getenv("HOME");
        if (target == NULL || chdir(target) == -1) {
            perror("cd");
        }
        return;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return;
    }

    if (pid == 0) {
        /* Відновлюємо стандартну реакцію на SIGINT у дочірньому процесі */
        struct sigaction sa_default = {0};
        sa_default.sa_handler = SIG_DFL;
        sigemptyset(&sa_default.sa_mask);
        sigaction(SIGINT, &sa_default, NULL);

        if (redirect_file != NULL) {
            int fd = open(redirect_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd == -1) {
                perror("open");
                _exit(EXIT_FAILURE);
            }
            if (dup2(fd, STDOUT_FILENO) == -1) {
                perror("dup2");
                close(fd);
                _exit(EXIT_FAILURE);
            }
            close(fd);
        }

        execvp(argv[0], argv);
        perror("execvp");
        _exit(EXIT_FAILURE);
    }

    if (waitpid(pid, NULL, 0) == -1) {
        perror("waitpid");
    }
}

static void execute_pipeline(char *left_str, char *right_str)
{
    char *left_argv[MAX_ARGS];
    char *right_argv[MAX_ARGS];

    if (parse_args(left_str, left_argv) == 0 ||
        parse_args(right_str, right_argv) == 0) {
        fprintf(stderr, "Syntax error: invalid pipeline\n");
        return;
    }

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return;
    }

    pid_t child1 = fork();
    if (child1 == -1) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return;
    }

    if (child1 == 0) {
        struct sigaction sa_default = {0};
        sa_default.sa_handler = SIG_DFL;
        sigemptyset(&sa_default.sa_mask);
        sigaction(SIGINT, &sa_default, NULL);

        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
            perror("dup2");
            _exit(EXIT_FAILURE);
        }
        close(pipefd[0]);
        close(pipefd[1]);

        execvp(left_argv[0], left_argv);
        perror("execvp");
        _exit(EXIT_FAILURE);
    }

    pid_t child2 = fork();
    if (child2 == -1) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        waitpid(child1, NULL, 0);
        return;
    }

    if (child2 == 0) {
        struct sigaction sa_default = {0};
        sa_default.sa_handler = SIG_DFL;
        sigemptyset(&sa_default.sa_mask);
        sigaction(SIGINT, &sa_default, NULL);

        if (dup2(pipefd[0], STDIN_FILENO) == -1) {
            perror("dup2");
            _exit(EXIT_FAILURE);
        }
        close(pipefd[0]);
        close(pipefd[1]);

        execvp(right_argv[0], right_argv);
        perror("execvp");
        _exit(EXIT_FAILURE);
    }

    close(pipefd[0]);
    close(pipefd[1]);

    if (waitpid(child1, NULL, 0) == -1) {
        perror("waitpid child1");
    }
    if (waitpid(child2, NULL, 0) == -1) {
        perror("waitpid child2");
    }
}

int main(void)
{
    /* Оболонка ігнорує SIGINT (Ctrl+C), щоб переривати лише foreground child */
    struct sigaction sa_ignore = {0};
    sa_ignore.sa_handler = SIG_IGN;
    sigemptyset(&sa_ignore.sa_mask);
    if (sigaction(SIGINT, &sa_ignore, NULL) == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    char line[MAX_LINE];

    while (1) {
        printf("myshell> ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("\n");
            break;
        }

        char *pipe_pos = strchr(line, '|');
        if (pipe_pos != NULL) {
            *pipe_pos = '\0';
            execute_pipeline(line, pipe_pos + 1);
        } else {
            execute_single_command(line);
        }
    }

    return EXIT_SUCCESS;
}
