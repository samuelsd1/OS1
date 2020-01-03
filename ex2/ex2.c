/**
 * Name:    David Samuelson
 * ID:      208788851
 */

#include<stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <wait.h>
#include <memory.h>

/* ppid is the print pid, upid is the update pid */
pid_t ppid, upid;

void start_game(void);

void exec_print(int pipefd0, int pipefd1);
void exec_update(int pipefd0, int pipefd1, pid_t ppid);

void install_sigint_hand(void);
void install_sigalarm_hand(void);

int main(int argc, char *argv[]) {
    unsigned int time;

    if (argc <= 1) {
        exit(EXIT_FAILURE);
    }
    // install the handlers
    install_sigalarm_hand();
    install_sigint_hand();

    // parse the time and set the alarm
    time = (unsigned int) atoi(argv[1]);
    alarm(time);

    // start the game
    start_game();
    return 0;
}

/**
 * handles the start of the game
 */
void start_game(void) {
    int pipefd[2];
    // open pipe
    if (pipe(pipefd) < 0) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    // fork for print pid
    ppid = fork();
    // if failed
    if (ppid < 0) {
        perror("print fork");
        exit(EXIT_FAILURE);
    }

    // if its child process
    if (ppid == 0) {
        // call child process function, does not return
        exec_print(pipefd[0], pipefd[1]);
    }

    // sleep between forks
    sleep(1);

    // parent process. fork for update pid
    upid = fork();
    // if failed
    if (upid < 0) {
        perror("upd fork failed");
        exit(EXIT_FAILURE);
    }

    // child process
    if (upid == 0) {
        // call child process func, does not return
        exec_update(pipefd[0], pipefd[1], ppid);
    }

    // close the pipe file descriptors
    if (close(pipefd[0]) < 0) {
        perror("failed to close pipe fd 0");
        exit(EXIT_FAILURE);
    }
    if (close(pipefd[1]) < 0) {
        perror("failed to close pipe fd 1");
        exit(EXIT_FAILURE);
    }

    // wait to receive sigint, or sigalarm.
    pause();
}

/**
 * executes the print proccess, while handling the correct data flow
 * @param pipefd0 - read side of the pipe
 * @param pipefd1 - write side of the pipe
 */
void exec_print(int pipefd0, int pipefd1) {
    // dup for correct data flow
    if (dup2(pipefd0, STDIN_FILENO) < 0) {
        perror("print dup2");
        exit(EXIT_FAILURE);
    }

    // close unused pipe ends
    if (close(pipefd0) < 0) {
        perror("failed to close pipe fd 0");
        exit(EXIT_FAILURE);
    }
    if (close(pipefd1)) {
        perror("failed to close pipe fd 1");
        exit(EXIT_FAILURE);
    }

    // execute the print program
    execlp("./ex2_inp.out", "./ex2_inp.out", NULL);
    // if returned - it failed.
    perror("print exec failed");
    exit(EXIT_FAILURE);
}


/**
 * executes the update proccess, while handling the correct data flow
 * @param pipefd0 - read side of the pipe
 * @param pipefd1 - write side of the pipe
 * @param ppid - print pid
 */
void exec_update(int pipefd0, int pipefd1, pid_t ppid) {
    char buf[16];
    if (dup2(pipefd1, STDOUT_FILENO) < 0) {
        perror("update dup2");
        exit(EXIT_FAILURE);
    }

    // close the unused pipe ends
    if (close(pipefd0) < 0) {
        perror("failed to close pipe fd 0");
        exit(EXIT_FAILURE);
    }
    if (close(pipefd1) < 0) {
        perror("failed to close pipe fd 1");
        exit(EXIT_FAILURE);
    }

    // execute the program with the right parameters
    sprintf(buf, "%d", ppid);
    execlp("./ex2_upd.out", "./ex2_upd.out", buf, NULL);
    // if returned - it failed.
    perror("update exec failed");
    exit(EXIT_FAILURE);
}

/**
 * close child processes and wait for them
 */
void close_child_proc() {
    if (kill(ppid, SIGINT) < 0) {
        perror("kill");
        exit(EXIT_FAILURE);
    }

    if (kill(upid, SIGINT) < 0) {
        perror("kill");
        exit(EXIT_FAILURE);
    }

    // wait for child processes to finish
    if (waitpid(ppid, NULL, 0) < 0) {
        perror("waitpid");
        exit(EXIT_FAILURE);
    }

    if (waitpid(upid, NULL, 0) < 0) {
        perror("waitpid");
        exit(EXIT_FAILURE);
    }
}

/**
 * sigalarm handler
 * @param sig - the received signal
 */
void alarm_hand(int sig) {
    char *str = "BYE BYE";
    close_child_proc();
    write(STDOUT_FILENO, str, strlen(str));
    exit(EXIT_SUCCESS);
}

/**
 * installs the alarm handler
 */
void install_sigalarm_hand(void) {
    struct sigaction setup_action;
    sigset_t block_mask;

    memset(&setup_action, 0, sizeof setup_action);

    sigfillset(&block_mask);
    sigdelset(&block_mask, SIGINT);
    setup_action.sa_handler = alarm_hand;
    setup_action.sa_mask = block_mask;
    setup_action.sa_flags = 0;
    if (sigaction(SIGALRM, &setup_action, NULL) < 0) {
        perror("sigusr1 sigaction");
        exit(EXIT_FAILURE);
    }
}

/**
 * sigint handler
 * @param sig - the received signal
 */
void sigint_hand(int sig) {
    close_child_proc();
    exit(EXIT_SUCCESS);
}

/**
 * installs the sigint handler
 */
void install_sigint_hand(void) {
    struct sigaction setup_action;
    sigset_t block_mask;

    memset(&setup_action, 0, sizeof setup_action);

    sigfillset(&block_mask);
    setup_action.sa_handler = sigint_hand;
    setup_action.sa_mask = block_mask;
    setup_action.sa_flags = 0;
    if (sigaction(SIGINT, &setup_action, NULL) < 0) {
        perror("sigint sigaction");
        exit(EXIT_FAILURE);
    }
}