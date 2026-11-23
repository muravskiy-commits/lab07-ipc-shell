#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
    int pipefd[2];

    if (pipe(pipefd) == -1) {
        perror("pipe");
        return EXIT_FAILURE;
    }

    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        /* Правильне закриття невикористаного write end у child для отримання EOF */
        close(pipefd[1]);

        char buffer[64];
        ssize_t n;

        while ((n = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[n] = '\0';
            printf("Child chunk: %s", buffer);
        }

        if (n == -1) {
            perror("read");
            close(pipefd[0]);
            exit(EXIT_FAILURE);
        }

        printf("Child reached EOF cleanly!\n");
        close(pipefd[0]);
        exit(EXIT_SUCCESS);
    }

    close(pipefd[0]);

    const char *msg1 = "First line\n";
    const char *msg2 = "Second line\n";

    write(pipefd[1], msg1, strlen(msg1));
    write(pipefd[1], msg2, strlen(msg2));

    /* Закриття write end у parent сигналізує EOF читачеві */
    close(pipefd[1]);

    waitpid(pid, NULL, 0);

    return EXIT_SUCCESS;
}
