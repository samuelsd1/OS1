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
#include <fcntl.h>
#include <semaphore.h>
#include <pthread.h>


#define FNAME       "208788851.txt"

#define SHM_SIZE    (4096)
#define SHM_FLAGS   (IPC_CREAT | 0644)


void debug(char *str) {
    //printf("[DEBUG]:\t%s\n", str);
}

typedef struct {
    int id;
    char *addr;
} shm_det_t;

typedef struct {
    pthread_t pool[5];
} threadpool_t;

shm_det_t create_shm(char *name, char k);

void init_shm(shm_det_t det);

int close_shm(shm_det_t det);

int create_file();

int close_file();

int init_pool(threadpool_t *pool);

int destroy_pool(threadpool_t *pool, int flag);

int server_logic(char *shm);

/**
 * Global variables
 */
int fd;
pthread_mutex_t fmtx;

volatile int internal_count;
volatile char job;

int main() {
    int exit_code;
    shm_det_t det;

    // seed the random gen.
    srand(time(NULL));

    // create the file
    if (create_file() < 0) {
        exit(EXIT_FAILURE);
    }

    // create the shared memory
    det = create_shm(FNAME, 'K');
    if (det.id < 0) {
        close_file();
        exit(EXIT_FAILURE);
    }

    // initialize the shared memory
    init_shm(det);

    // server logic here...
    exit_code = server_logic(det.addr);
    exit_code = (exit_code < 0) ? EXIT_FAILURE : EXIT_SUCCESS;


    // close the shared memory
    if (close_shm(det) < 0) {
        exit_code = EXIT_FAILURE;
    }

    // close the file
    if (close_file() < 0) {
        exit_code = EXIT_FAILURE;
    }

    exit(exit_code);
}

/**
 * Creates shared memory with the given name and key
 * @param name - name of the shared memory
 * @param k - key of the shared memory
 * @return - returns the shm_det_t struct, on error ID field will be -1 and shm_addr will be null.
 *              on success, those fields would be properly initialized.
 */
shm_det_t create_shm(char *name, char k) {
    key_t key;
    shm_det_t det = {-1, NULL};

    // name the key as requested
    key = ftok(name, k);

    // create the segment
    det.id = shmget(key, SHM_SIZE, SHM_FLAGS);
    if (det.id < 0) {
        perror("shmget");
        return det;
    }

    // attach the segment to our data space.
    det.addr = shmat(det.id, NULL, 0);
    if (det.addr == (char *) -1) {
        perror("shmat");
        shmctl(det.id, IPC_RMID, NULL);
        perror("shmctl");

        det = (shm_det_t) {-1, NULL};
        return det;
    }

    return det;
}

/**
 * initializes the shared memory
 * @param det - details of the given shared memory
 */
void init_shm(shm_det_t det) {
    memset(det.addr, 0, SHM_SIZE);
}

/**
 * Closes the shared memory
 * @param det - details of the given shared memory
 * @return - on success returns 0, otherwise returns -1.
 */
int close_shm(shm_det_t det) {
    int err = 0;
    // detach the memory
    if (shmdt(det.addr) < 0) {
        perror("shmdt");
        err = -1;
    }

    // remove the memory
    if (shmctl(det.id, IPC_RMID, NULL) < 0) {
        perror("shmctl");
        err = -1;
    }
    return err;
}

/**
 * creates the output file and initializes the mutex
 * @return on error returns -1, positive values otherwise
 */
int create_file() {
    if (pthread_mutex_init(&fmtx, 0) < 0) {
        perror("pthread mutex init");
        return -1;
    }

    fd = open(FNAME, O_RDWR | O_TRUNC | O_CREAT, 0666);
    if (fd < 0) {
        perror("open");
        pthread_mutex_destroy(&fmtx);
        perror("mutex destroy");
    }
    return fd;
}

/**
 * Closes the file and destroyes the mutex
 * @return on success returns 0, -1 otherwise.
 */
int close_file() {
    int err = 0;
    if (close(fd) < 0) {
        err = -1;
        perror("close");
    }

    if (pthread_mutex_destroy(&fmtx) < 0) {
        err = -1;
        perror("pthread_mutex_destroy");
    }
    return err;
}

/**
 * This function manages all the server logic, without worrying about
 * creating the shared memory etc... just receives the shared memory
 * address and starts working with it.
 * @param shm - address of the shared memory
 * @return - 0 on success, -1 otherwise.
 */
int server_logic(char *shm) {
    threadpool_t pool;
    sem_t *queue, *job_start, *job_end, *tp_job_queue, *tp_job_acquire;
    int err = 0;
    int isRunning = 1;

    queue = sem_open("queue", O_CREAT, 0644, 0);
    if (SEM_FAILED == queue) {
        perror("sem open queue");
        return -1;
    }
    job_start = sem_open("job_start", O_CREAT, 0644, 0);
    if (SEM_FAILED == job_start) {
        perror("sem open job_start");
        sem_destroy(queue);
        perror("sem destroy queue");
        return -1;
    }
    job_end = sem_open("job_end", O_CREAT, 0644, 0);
    if (SEM_FAILED == job_end) {
        perror("sem open job_end");
        sem_destroy(job_start);
        perror("sem destroy job_start");
        sem_destroy(queue);
        perror("sem destroy queue");
        return -1;
    }

    tp_job_queue = sem_open("tp_job_queue", O_CREAT, 0644, 0);
    if (SEM_FAILED == tp_job_queue) {
        perror("sem open tp queue");
        sem_destroy(job_end);
        perror("sem destroy job_end");
        sem_destroy(job_start);
        perror("sem destroy job_start");
        sem_destroy(queue);
        perror("sem destroy queue");
        return -1;
    }

    tp_job_acquire = sem_open("tp_job_end", O_CREAT, 0644, 0);
    if (SEM_FAILED == tp_job_acquire) {
        perror("sem open tp job acquire");
        sem_destroy(tp_job_queue);
        perror("sem destroy tp job");
        sem_destroy(job_end);
        perror("sem destroy job_end");
        sem_destroy(job_start);
        perror("sem destroy job_start");
        sem_destroy(queue);
        perror("sem destroy queue");
        return -1;
    }

    // init the internal count
    internal_count = 0;
    if (init_pool(&pool) >= 0) {
        while (isRunning) {
            // wait for a job
            debug("Waiting for job request");
            sem_wait(queue);
            // give permission to a job
            debug("Giving job permission");
            sem_post(job_start);
            debug("Waiting for job to be written to shm");
            sem_wait(job_end);
            job = *shm;
            debug("job was written to shm:");
            // if the job is G or H , we want to finish the program.
            // we handle those jobs differently than other jobs.
            if (job == 'g' || job == 'h') {
                char buf[128];
                isRunning = 0;
                if (job == 'g') {
                    debug("received g. destroying pool...");
                    err = destroy_pool(&pool, 0);
                    if (err) {
                        debug("failed at destroying pool");
                    } else {
                        debug("successfully destroyed pool.");
                    }
                } else {
                    // if its H, we pass the job to the threads, then wait until they finish
                    int i;
                    debug("received h. passing the job to all threads...");
                    // pass the job to all the threads.
                    for (i = 0; i < 5; ++i) {
                        // pass the job to one of the threads
                        sem_post(tp_job_queue);
                        // wait until one of the threads acquires the job
                        sem_wait(tp_job_acquire);
                    }
                    debug("finished passing job to all threads. destroying the pool using join...");
                    // destroy the thread pool gracefully, using pthread join.
                    err = destroy_pool(&pool, 1);
                    if (err) {
                        debug("failed to destroy the pool using join");
                    } else {
                        debug("successfully destroyed the pool using join");
                    }
                }

                // write to the file, this time without worrying about
                // the file mutex, because all threads are dead by now.
                debug("writing to file...");
                sprintf(buf, "thread identifier is %lu and internal_count is %d\n", pthread_self(), internal_count);

                if (write(fd, buf, strlen(buf)) < 0) {
                    err = -1;
                    perror("main thread write to file");
                }
            } else {
                debug("normal job. passing the job to one of the threads.");
                // pass the job to one of the threads
                sem_post(tp_job_queue);
                debug("waiting for one of the threads to acquire the job");
                // wait until one of the threads acquires the job
                sem_wait(tp_job_acquire);
                debug("job acquired. moving to next job...");
            }
        } // while isRunning
    } else {
        err = -1;
    }

    debug("finished with the job loop. destroying semaphores...");
    if (sem_destroy(queue) < 0) {
        err = -1;
        perror("sem destroy job end");
    }
    if (sem_destroy(job_start) < 0) {
        err = -1;
        perror("sem destroy job start");
    }
    if (sem_destroy(job_end) < 0) {
        err = -1;
        perror("sem destroy job end");
    }
    if (sem_destroy(tp_job_queue) < 0) {
        err = -1;
        perror("sem destroy tp job queue");
    }
    if (sem_destroy(tp_job_acquire) < 0) {
        err = -1;
        perror("sem destroy tp job queue");
    }
    debug("finished destroying semaphores.");
    return err;
}

/******************************************************************************
 *               THREAD POOL, DO NOT GO BELOW, VERY SCARY CODE                *
 *****************************************************************************/

/**
 * handles jobs a to e, as described in the exercise
 * @param inc - the increment to the internal counter
 */
void handle_ae(int inc) {
    int x = (rand() % 90) + 10;
    struct timespec spec;
    spec.tv_sec = 0;
    spec.tv_nsec = x;

    nanosleep(&spec, NULL);
    internal_count += inc;
}

/**
 * Handles the job f (writes thread id and internal count to the file).
 */
void handle_f() {
    char buf[128];
    size_t len;
    ssize_t err;
    sprintf(buf, "thread identifier is %lu and internal_count is %d\n", pthread_self(), internal_count);
    len = strlen(buf);
    debug("thread locking mutex");
    pthread_mutex_lock(&fmtx);
    err = write(fd, buf, len);
    pthread_mutex_unlock(&fmtx);
    debug("mutex unlocked.");
    if (err < 0) {
        perror("write err");
    }
}

/**
 * thread function for all the worker threads in the threadpool
 * @param param - unused
 * @return - NULL
 */
void *thread_func(void *param) {
    sem_t *tp_job_queue, *tp_job_acquire;
    // get the semaphores which sync the work queue
    tp_job_queue = sem_open("tp_job_queue", 0);
    if (SEM_FAILED == tp_job_queue) {
        perror("sem open tp queue");
        pthread_exit(NULL);
    }
    tp_job_acquire = sem_open("tp_job_end", 0);
    if (SEM_FAILED == tp_job_acquire) {
        perror("sem open tp job acquire");
        sem_destroy(tp_job_queue);
        perror("sem destroy tp job acquire");
        pthread_exit(NULL);
    }

    while (1) {
        char myjob;
        sem_wait(tp_job_queue);
        myjob = job;
        sem_post(tp_job_acquire);
        switch (myjob) {
            case 'a':
                handle_ae(1);
                break;
            case 'b':
                handle_ae(2);
                break;
            case 'c':
                handle_ae(3);
                break;
            case 'd':
                handle_ae(4);
                break;
            case 'e':
                handle_ae(5);
                break;
            case 'f':
                handle_f();
                break;
            case 'h':
                // h is basiclly f then close (for each thread, of course there is the responsibility
                // of the main thread to make all the threads get the 'h' job).
                handle_f();
                if (sem_destroy(tp_job_queue) < 0) {
                    perror("thread func sem destroy tp job queue");
                }
                if (sem_destroy(tp_job_acquire) < 0) {
                    perror("thread func sem destroy tp job queue");
                }
                pthread_exit(NULL);
            default:
                break;
        }
    }
}

/**
 * initializes the threadpool
 * @param pool - the threadpool struct
 * @return - 0 on success, -1 otherwise
 */
int init_pool(threadpool_t *pool) {
    int i;
    int err = 0;
    // create 5 threads
    for (i = 0; i < 5; ++i) {
        if (pthread_create(&pool->pool[i], NULL, thread_func, NULL) < 0) {
            perror("pthread_create");
            err = 1;
            break;
        }
    }
    // if there was error during creatin of the threads, cancel all the created threads.
    if (err) {
        int j;
        for (j = 0; j < i; ++j) {
            if (pthread_cancel(pool->pool[j]) < 0) {
                perror("pthread cancel");
            }
        }
        return -1;
    }
    return 0;
}

/**
 * destroys the threadpool, according to the given flag
 * @param pool - the threadpool struct
 * @param flag - if flag is 0, then the threads are immidiately killed, without waiting for them to finish.
 *      otherwise, it first waits for the threads to finish, then returns.
 * @return 0 on success, -1 otherwise.
 */
int destroy_pool(threadpool_t *pool, int flag) {
    // if flag == 1, then we join
    // else , we cancel.
    int i;
    int err = 0;
    for (i = 0; i < 5; ++i) {
        if (flag) {
            if (pthread_join(pool->pool[i], NULL) < 0) {
                err = -1;
            }
        } else {
            if (pthread_cancel(pool->pool[i]) < 0) {
                err = -1;
            }
        }
    }
    return err;
}