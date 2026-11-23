#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static volatile sig_atomic_t got_sigterm = 0;

static void handle_sigterm(int sig)
{
    (void)sig;
    got_sigterm = 1;
    const char msg[] = "Child: caught SIGTERM gracefully!\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
}

int main(void)
{
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        struct sigaction sa = {0};
        sa.sa_handler = handle_sigterm;
        sigemptyset(&sa.sa_mask);

        if (sigaction(SIGTERM, &sa, NULL) == -1) {
            perror("sigaction");
            _exit(EXIT_FAILURE);
        }

        printf("Child (PID=%d): waiting for SIGTERM...\n", getpid());
        fflush(stdout);

        while (!got_sigterm) {
            pause();
        }

        printf("Child: exiting after handling signal\n");
        _exit(EXIT_SUCCESS);
    }

    sleep(2);
    printf("Parent: sending SIGTERM to child %d\n", pid);
    kill(pid, SIGTERM);

    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        printf("Parent: child exited with status %d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        printf("Parent: child killed by signal %d\n", WTERMSIG(status));
    }

    return EXIT_SUCCESS;
}
