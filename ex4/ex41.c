/**
 * Name:    David Samuelson
 * ID:      208788851
 */

#include <stdio.h>
#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdlib.h>

#define SHM_SIZE    (4096)
int client_logic(char *shm);

int main(int argc, char **argv){
    int shm_id;
    char *shm;
    int err = 0;

    // connect to the shared memory
    key_t key = ftok("208788851.txt", 'K');

    shm_id = shmget(key, SHM_SIZE, 0666);
    if (shm_id < 0) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    // attach the shared memory
    shm = shmat(shm_id, NULL, 0);
    if (shm == (char *) -1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    // client logic here
    err = client_logic(shm);

    // detach the shared memory
    if (shmdt(shm) < 0) {
        perror("shmdt");
        exit(EXIT_FAILURE);
    }
    if(err){
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}

/**
 * executes a job
 * @param shm - pointer to the shared memory
 * @param queue - job queue semaphore
 * @param job_start - job start semaphore
 * @param job_end - job end semaphore
 * @return - the job that was inputted by the user
 */
int job(char *shm, sem_t *queue, sem_t *job_start, sem_t *job_end){
    char c;
    printf("Please enter request code\n");
    scanf(" %c", &c);
    // filter out the legal jobs
    switch(c){
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
        case 'g':
        case 'h':
            break;
        default:
            return c;
    }
    // post that i want to write to the shared memory
    sem_post(queue);
    // wait until server accepts to handle me
    sem_wait(job_start);
    // write the job to the shared memory
    *shm = c;
    // tell the server that i finished writing
    sem_post(job_end);
    // return the inputted job
    return c;
}

/**
 * client logic, create the semaphores, then execute jobs until 'i'
 * @param shm - pointer to the shared memory
 * @return - 0 on success, -1 otherwise
 */
int client_logic(char *shm){
    int c;
    int err = 0;
    sem_t *queue, *job_start, *job_end;

    // open the semaphores
    queue = sem_open("queue", 0);
    if (SEM_FAILED == queue) {
        perror("sem open queue");
        return -1;
    }
    job_start = sem_open("job_start", 0);
    if (SEM_FAILED == job_start) {
        perror("sem open job_start");
        sem_destroy(queue);
        perror("sem destroy queue");
        return -1;
    }
    job_end = sem_open("job_end", 0);
    if (SEM_FAILED == job_end) {
        perror("sem open job_end");
        sem_destroy(job_start);
        perror("sem destroy job_start");
        sem_destroy(queue);
        perror("sem destroy queue");
        return -1;
    }

    do{
        c = job(shm, queue, job_start, job_end);
    }while(c != 'i');

    // destroy the semaphores
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
    return err;
}