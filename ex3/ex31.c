/**
 * Name:    David Samuelson
 * ID:      208788851
 */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#define SHM_SIZE    (4096)
#define SHM_FLAGS   (IPC_CREAT | 0644)

#define MOD_DEBUG

#undef  MOD_DEBUG

void debug(char *str) {
#ifdef MOD_DEBUG
    char *dbg = "[DEBUG]:\t";
    write(STDOUT_FILENO, dbg, strlen(dbg));
    write(STDOUT_FILENO, str, strlen(str));
    write(STDOUT_FILENO, "\n", sizeof(char));
#endif
}

void write_err(char *str) {
    write(STDERR_FILENO, str, strlen(str));
}

int register_players(pid_t *b_pid, pid_t *w_pid);

void del_exit(int shm_id) {
    shmctl(shm_id, IPC_RMID, NULL);
    perror("shmctl");
    exit(EXIT_FAILURE);
}

void dt_del_exit(int shm_id, char *shm_addr) {
    shmdt(shm_addr);
    perror("shmdt");
    del_exit(shm_id);
}

int main() {
    int exit_status;
    int shm_id;
    char *shm_addr;
    key_t key;

    pid_t b_pid;
    pid_t w_pid;


    /**
     * Create the shared memory
     */
    // name the key as requested
    key = ftok("ex31.c", 'k');

    debug("getting the shared memory...");
    // create the segment
    shm_id = shmget(key, SHM_SIZE, SHM_FLAGS);
    if (shm_id < 0) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    debug("attaching shared memory...");
    // attach the segment to our data space.
    shm_addr = shmat(shm_id, NULL, 0);
    if (shm_addr == (char *) -1) {
        perror("shmat");
        del_exit(shm_id);
    }

    /**
     * Initialize the shared memory
     */
    memset(shm_addr, 0, SHM_SIZE);

    /**
     * Wait for players to connect and register them
     */
    debug("calling register players...");
    if (register_players(&b_pid, &w_pid) < 0) {
        write_err("Failed to register players.\n");
        dt_del_exit(shm_id, shm_addr);
    }

    debug("Finished registering players.");

    /**
     * Send start signal first player
     */
    debug("sending signal to first player...");
    if (kill(b_pid, SIGUSR1) < 0) {
        perror("kill bpid");
        dt_del_exit(shm_id, shm_addr);
    }

    /**
     * Wait for first player move
     */
    debug("waiting for first player to move...");
    while (shm_addr[0] == 0x00) {
        sleep(1);
    }

    /**
     * Send start signal to the second player
     */
    debug("sending signal to second player...");
    if (kill(w_pid, SIGUSR1) < 0) {
        perror("kill wpid");
        dt_del_exit(shm_id, shm_addr);
    }

    debug("Finished sending signals to both players. ");
    debug("Game started, waiting until its finished.");

    /**
     * Busy wait until game has finished
     */

    while (shm_addr[0] != 'o') {
        sleep(1);
    }

    /**
     * Print the corresponding message
     */
    printf("GAME OVER\n");
    switch (shm_addr[1]) {
        case 'b':
            printf("Winning player: Black\n");
            break;
        case 'w':
            printf("Winning player: White\n");
            break;
        case 't':
        default:
            // tie
            printf("No winning player\n");
            break;
    }

    /**
     * Delete the shared memory and finish
     */
    exit_status = EXIT_SUCCESS;
    // detach the memory
    debug("Detaching the shared memory..");
    if (shmdt(shm_addr) < 0) {
        perror("shmdt");
        exit_status = EXIT_FAILURE;
    }

    // remove the memory
    debug("Removing the shared memory");
    if (shmctl(shm_id, IPC_RMID, NULL) < 0) {
        perror("shmctl");
        exit_status = EXIT_FAILURE;
    }
    exit(exit_status);
}

pid_t read_from_fifo(char *fifo) {
    int fd;
    char buf[64];
    pid_t pid;

    // do while to treat those cases when the fifo reads nothing.
    do {
        debug("opening fifo...");
        fd = open(fifo, O_RDONLY);
        if (fd < 0) {
            perror("open fifo");
            return -1;
        }

        memset(buf, 0, sizeof(buf));

        debug("Reading from fifo...");
        if (read(fd, buf, sizeof(buf)) < 0) {
            perror("read from fifo");
            close(fd);
            perror("close");
            return -1;
        }
        debug("Read From FIFO:");
        debug(buf);
        debug("closing fifo...");
        if (close(fd) < 0) {
            perror("close");
            return -1;
        }
        debug("closed fifo.");
        debug("");
    } while (strlen(buf) == 0);
    sscanf(buf, "%d", &pid);
    return pid;
}

/**
 * Function creates a fifo pipe with the name 'fifo_clientTOserver' as defined,
 *  waits for the clients to write their pids, initializes the given pid_t ptrs,
 *  then closes and deletes the fifo pipe.
 * @param b_pid - ptr to the black player pid
 * @param w_pid - ptr to the white player pid
 * @return on success returns 0, -1 otherwise.
 */
int register_players(pid_t *b_pid, pid_t *w_pid) {
    char *my_fifo = "fifo_clientTOserver";
    pid_t p;


    /* delete the fifo if it was there before */
    debug("unlinking previous fifo (if exists)...");
    unlink(my_fifo);

    /* Create the fifo */
    debug("Creating the fifo...");
    if (mkfifo(my_fifo, 0666) < 0) {
        perror("mkfifo");
        return -1;
    }


    /* Read from the fifo and register players */

    debug("Reading first players pid...");
    /* Read the pid from the first player */
    p = read_from_fifo(my_fifo);
    if (p < 0) {
        write_err("Failed to read from fifo...");
        unlink(my_fifo);
        perror("unlink fifo");
        return -1;
    }
    *b_pid = p;
    debug("Finished reading the first pid.");




    /* Read the pid from the second player */
    p = read_from_fifo(my_fifo);
    if (p < 0) {
        write_err("Failed to read from fifo...");
        unlink(my_fifo);
        perror("unlink fifo");
        return -1;
    }
    *w_pid = p;

    debug("Finished reading second player pid.");

    /* Delete the fifo */
    debug("Deleting the fifo...");
    if (unlink(my_fifo) < 0) {
        perror("unlink fifo");
        return -1;
    }
    debug("finished deleting fifo, returning from register_players...");
    return 0;
}