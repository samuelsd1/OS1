/**
 * Name:    David Samuelson
 * ID:      208788851
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/wait.h>

#define STUD_NAME_LEN   (32)
#define PATH_SIZE       (512)

#define EXEC_NAME       "stud.out"
#define OUTPUT_NAME     "stud_output.txt"
#define COMP_PROG       "./comp.out"

#define DET_NAME        "results.csv"
#define DET_ENTRY_LEN   (128)

/* helpful data structures definitions*/
typedef enum search_description {
    FOUND,                      /* Successfully found the C file */
    NOT_FOUND,                  /* Could not find any C file */
    MULT_DIR,                   /* Found multiple directions */
    MAX_DEPTH_REACHED,          /* Max depth reached during search */
    SYSCALL_ERR                 /* There was error during a system call*/
} search_desc_t;

typedef struct search_result {
    search_desc_t description;  /* Description of the search result */
    int depth;                  /* Depth of the C file (if found) */
    char file_path[PATH_SIZE];  /* Path of the file */
} search_t;

typedef enum score_description {
    SYS_ERR = -1,               /* Error during system call */
    NONE,                       /* No description (no error) */

    NO_C_FILE,                  /* No C file found */
    MULTIPLE_DIRECTORIES,       /* Stopped the search because of multiple directories */
    WRONG_DIRECTORY,            /* File found in wrong directory */

    COMPILATION_ERROR,          /* Error with the compilation */
    TIMEOUT,                    /* Timeout of 5 seconds occurred */

    BAD_OUTPUT,                 /* Bad program output */
    SIMILAR_OUTPUT,             /* Similar program output */
    GREAT_JOB                   /* Good program output */
} score_desc_t;

typedef struct score_penalty {
    int penalty;                /* Score penalty */
    score_desc_t reason;        /* reason for the penalty */
} penalty_t;

typedef struct student_details {
    char name[STUD_NAME_LEN];    /* Student name */
    penalty_t search_penalty;    /* Search penalty */
    penalty_t ex_penalty;        /* Compilation and testing penalty */
} stud_det_t;

/* Function definitions*/
int handle_conf_file(int fd);

int handle_program(const char *f_dir, const char *input_dir, const char *out_dir);

/*  Handle each student   */
stud_det_t handle_student(const char *stud_name, const char *stud_dir, const char *input_dir, const char *out_dir);

search_t locate_c_file(const char *path, int depth, const int max_depth);

penalty_t from_search_result(search_t search);

/*  Test the student   */
penalty_t test(const char *path, const char *input_dir, const char *out_dir);

int compile(const char *path);

int run(const char *input_dir);

int compare_files(const char *out_dir);

int clear_files();

ssize_t write_student_details(int fd, stud_det_t details);

/*  Converters  */
search_t search_failed(search_desc_t description, int depth);

penalty_t from_search_result(search_t search);

char *score_desc_tostring(score_desc_t desc);

/**
 * Main method, no need to explain what it does :)
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char **argv) {
    int conf_fd;

    printf("Hey :)\n");
    if (argc <= 1) {
        printf("Not enough arguments!\n");
        exit(EXIT_FAILURE);
    }

    printf("> Opening the configuration file \'%s\'...\n", argv[1]);

    // open the configuration file
    conf_fd = open(argv[1], O_RDONLY, 0777);
    if (conf_fd < 0) {
        perror("Could not open the configuration file");
        exit(EXIT_FAILURE);
    }
    printf("> Successfully opened the configuration file.\n");
    handle_conf_file(conf_fd);

    printf("> Closing the configuration file...\n");
    if (close(conf_fd) < 0) {
        perror("Error while closing the configuration file");
        exit(EXIT_FAILURE);
    }
    printf("> Successfully closed the configuration file.\nBye :)\n");
    exit(EXIT_SUCCESS);
}

/**
 * Function reads from the configuration file and executes the program
 * @param fd - open file descriptor of the configuration file
 * @return -1 if failed, otherwise returns handle_program() return value.
 */
int handle_conf_file(int fd) {
    char confFile[481];
    char *f_dir, *input_dir, *out_dir;
    ssize_t len;

    // initialize configuration file with zeroes
    memset(confFile, 0, sizeof(confFile));
    // read from the configuration file
    len = read(fd, confFile, 480);
    // handle errors
    if (len < 1) {
        perror("Could not read the configuration file");
        if (close(fd) < 0) {
            perror("Error while closing the configuration file");
        }
        return -1;
    }
    // set null terminator, so it can be treated as string.
    confFile[480] = 0;
    // split configuration file to tokens by the newline character.
    f_dir = strtok(confFile, "\n");
    input_dir = strtok(NULL, "\n");
    out_dir = strtok(NULL, "\n");

    if (f_dir == NULL || input_dir == NULL || out_dir == NULL) {
        printf("Incomplete configuration file!\n");
        return -1;
    }

    // finished acquiring folder dir, input dir and output dir.
    // now, need to handle those files and do the logic.
    return handle_program(f_dir, input_dir, out_dir);
}

/**
 * Opens the students folder, and handles all the students (tests them)
 * @param f_dir - students folder directory
 * @param input_dir - input file directory
 * @param out_dir - correct output file directory
 * @return -1 in case of system error, 0 otherwise
 */
int handle_program(const char *f_dir, const char *input_dir, const char *out_dir) {
    DIR *dir;           /* directory ptr */
    struct dirent *ep;  /* entry ptr */
    int error = 0;
    int fdet;

    printf("> Creating details file...\n");
    fdet = open(DET_NAME, O_WRONLY | O_TRUNC | O_CREAT, 0777);
    if (fdet < 0) {
        perror("Failed to open details file");
        return -1;
    }

    // open the student folders directory
    printf("> Opening the directory \'%s\'...\n", f_dir);
    dir = opendir(f_dir);

    // check for failure
    if (NULL == dir) {
        perror("Could not open the folders directory");
        if (close(fdet) < 0) {
            perror("Failed to close details file");
        }
        return -1;
    }

    // read the directory.
    printf("> Successfully opened the directory. reading it...\n\n");
    while ((ep = readdir(dir)) != NULL) {
        // ignore self and parent directories
        if (!strcmp(ep->d_name, ".") || !strcmp(ep->d_name, ".."))
            continue;
        // if is student directory
        if (DT_DIR == ep->d_type) {
            char path[PATH_SIZE];
            stud_det_t details;

            // create the students directory path and get his details.
            sprintf(path, "%s/%s", f_dir, ep->d_name);

            // handle the student, get details.
            printf(">> Handling '%s'...\n", ep->d_name);
            details = handle_student(ep->d_name, path, input_dir, out_dir);

            // finished handling the student. write his details to the csv file.
            printf("> Finished testing '%s'\n", ep->d_name);
            // check that no system call error occurred
            if (details.search_penalty.reason == SYS_ERR || details.ex_penalty.reason == SYS_ERR) {
                // system call error occurred, break outside of the while loop.
                printf("> Error during handling \'%s\'.\n", ep->d_name);
                error = -1;
                break;
            }

            // write the student details
            printf("> Writing '%s' details...\n", ep->d_name);
            if (write_student_details(fdet, details) < 0) {
                printf("> Failed to write students details.\n");
                error = -1;
                break;
            }
            printf("> Finished handling '%s'.\n\n", ep->d_name);
        }
    }
    // close the directory
    if (closedir(dir) < 0) {
        perror("Error during closing the folders directory");
        error = -1;
    }

    // close the student details file
    if (close(fdet) < 0) {
        perror("Error closing student details file");
        error = -1;
    }

    // clear the remaining files
    clear_files();
    return error;
}

/**
 * Handles a student
 * @param stud_name - name of the student
 * @param stud_dir - directory of the students folder
 * @param input_dir - input file for the students program
 * @param out_dir - correct output file to compare later
 * @return - student details struct containing details about the score of the student
 */
stud_det_t handle_student(const char *stud_name, const char *stud_dir, const char *input_dir, const char *out_dir) {
    stud_det_t stud_det;
    search_t search;
    // initialize the student details
    // initialize penalties using compound literals
    stud_det.search_penalty = (penalty_t) {0, NONE};
    stud_det.ex_penalty = (penalty_t) {0, NONE};
    strcpy(stud_det.name, stud_name);

    // locate the students C file
    printf(">> Searching for C file...\n");
    search = locate_c_file(stud_dir, 0, 10);    // maximum depth 10, start from 0
    stud_det.search_penalty = from_search_result(search);

    // if couldnt find the C file, there is no reason to continue.
    if (search.description != FOUND) {
        printf(">> Couldn't locate the C file.\n");
        return stud_det;
    }

    printf(">> Found C file at %s.\n", search.file_path);
    // test the student
    stud_det.ex_penalty = test(search.file_path, input_dir, out_dir);
    printf(">> Finished testing the student.\n");
    return stud_det;
}

/**
 * Locates the C file in the given path
 * @param path - path of the current directory
 * @param depth - depth of the current directory
 * @param max_depth - maximal depth
 * @return search result struct that contains search details and path(if found)
 */
search_t locate_c_file(const char *path, int depth, const int max_depth) {
    DIR *dir;
    struct dirent *ep;
    struct dirent folder, file;
    int folder_count = 0, found = 0;
    char next_path[PATH_SIZE];

    // stop condition - max depth reached
    if (depth == max_depth) {
        return search_failed(MAX_DEPTH_REACHED, max_depth);
    }

    // open the directory
    dir = opendir(path);
    // handle failure
    if (NULL == dir) {
        perror("Could not open student inner directory");
        return search_failed(SYSCALL_ERR, depth);
    }

    // read the directory
    while ((ep = readdir(dir)) != NULL) {
        // ignore self and parent directory
        if (!strcmp(ep->d_name, ".") || !strcmp(ep->d_name, ".."))
            continue;

        // if it is a directory
        if (DT_DIR == ep->d_type) {
            // found directory, save its entry and increase the folder counter
            ++folder_count;
            folder = *ep;
        } else if (DT_REG == ep->d_type) {
            // found file, check if it is C file.
            char *dot = strrchr(ep->d_name, '.');
            if (dot && !strcmp(dot, ".c")) {
                // it is c file, save its entry, and raise the found flag;
                found = 1;
                file = *ep;
            }
        }
    }
    // cleanup, close the directory
    if (closedir(dir) < 0) {
        perror("Failed to close directory");
        return search_failed(SYSCALL_ERR, depth);
    }

    // if found the C file
    if (found) {
        search_t result;
        result.depth = depth;
        result.description = FOUND;
        sprintf(result.file_path, "%s/%s", path, file.d_name);
        return result;
    }

    // no file found, and search cannot be continued
    if (folder_count > 1)
        return search_failed(MULT_DIR, depth);
    else if (folder_count <= 0)
        return search_failed(NOT_FOUND, depth);

    // folder count is 1, => folder entry is initialized
    sprintf(next_path, "%s/%s", path, folder.d_name);
    return locate_c_file(next_path, depth + 1, max_depth);
}

/**
 * Generates search result from given description and depth
 * @param description - description of the search result
 * @param depth - depth of the search
 * @return search_t struct.
 */
search_t search_failed(search_desc_t description, int depth) {
    search_t retval;
    retval.description = description;
    retval.depth = depth;
    sprintf(retval.file_path, "You found the secret string!");
    return retval;
}

/**
 * Calculates and generates penalty from search result
 * @param search  - the search result
 * @return penalty_t struct that represents the penalty of the search result
 */
penalty_t from_search_result(search_t search) {
    penalty_t p;
    switch (search.description) {
        case FOUND:
            // found the file. penalty is 10*depth.
            p.penalty = 10 * search.depth;
            p.reason = (search.depth > 0) ? WRONG_DIRECTORY : NONE;
            break;
        case NOT_FOUND:
            // could not find the c file. 100 points penalty
            p.penalty = 100;
            p.reason = NO_C_FILE;
            break;
        case MULT_DIR:
            // multiple directories, could not find the C file. 100 points penalty
            p.penalty = 100;
            p.reason = MULTIPLE_DIRECTORIES;
            break;
        case MAX_DEPTH_REACHED:
            // max depth reached without finding the C file. 100 points penalty
            p.penalty = 100;
            p.reason = NO_C_FILE;
            break;
        case SYSCALL_ERR:
            // system call error, notify about it.
            p.penalty = 0;
            p.reason = SYS_ERR;
            break;
        default:
            // I Covered all options, so if default case is activated, treat as syscall error.
            p.penalty = 0;
            p.reason = SYS_ERR;
            break;
    }
    return p;
}

/**
 * Given path to C file, test() returns the penalty for the program part.
 *  test() compiles, runs and compares output of the students program,
 *  and by that calculates the penalty.
 *
 * @param path      -   path of the C file
 * @param input_dir -   input file path
 * @param out_dir   -   output file path
 * @return penalty for the student program part.
 */
penalty_t test(const char *path, const char *input_dir, const char *out_dir) {
    int res;
    printf(">> Compiling C file at %s...\n", path);
    res = compile(path);
    if (res < 0) {
        printf(">> Compilation failed, reason: System error. retval: %d\n", res);
        return (penalty_t) {0, SYS_ERR};
    } else if (res != 0) {
        printf(">> Compilation failed, reason: compilation error.\n");
        return (penalty_t) {100, COMPILATION_ERROR};
    }
    printf(">> Compilation succeed. running the program...\n");
    // compilation succeed. run the program.
    res = run(input_dir);
    if (res < 0) {
        // syscall err
        printf(">> System error occurred while running the program.\n");
        return (penalty_t) {0, SYS_ERR};
    } else if (res == 0) {
        // timeout
        printf(">> Student program terminated due to timeout.\n");
        return (penalty_t) {100, TIMEOUT};
    }

    printf(">> Successfully ran the program. comparing output...\n");
    // success, compare results.
    res = compare_files(out_dir);
    switch (res) {
        case 1:
            printf(">> Program output matches expected output. Great job!\n");
            return (penalty_t) {0, GREAT_JOB};
        case 2:
            printf(">> Program output is similar to expected output.\n");
            return (penalty_t) {30, SIMILAR_OUTPUT};
        case 3:
            printf(">> Program output does not match expected output.\n");
            return (penalty_t) {100, BAD_OUTPUT};
        default:
            printf(">> Some error occurred while comparing the outputs.\n");
            printf(">> Debug: compare_files retval: %d\n", res);
            return (penalty_t) {0, SYS_ERR};
    }

}

/**
 * compiles the c file at the given path into EXEC_NAME inside current dir
 * @param path - path of C file.
 * @return on success, returns 0. on compile failure, returns > 1,
 *  on sys call error - returns -1
 */
int compile(const char *path) {
    pid_t pid;
    int status;
    pid = fork();
    if (pid > 0) {
        // parent
        pid_t child;
        printf(">> Compile: successfully forked, child pid: %d\n", pid);
        while ((child = wait(&status)) != pid) {
            printf(">> Compile: wait returned %d, expected %d\n", child, pid);
            if (child < 0) {
                perror("Compile: wait to child process failed");
                printf("Failed to wait to child process, terminating it...\n");
                if (kill(pid, SIGTERM) < 0) {
                    perror("Compile: failed to terminate child process");
                    printf("Failed to terminate child process, trying to kill...\n");
                    if (kill(pid, SIGKILL) < 0) {
                        perror("Compile: failed to kill child process");
                        printf("Failed to kill child process! Call for help and save yourself!\n");
                    }
                }
                return -1;
            }
        }

        // successfully waited.
        // check if exited normally
        if (!WIFEXITED(status)) {
            // gcc did not exit normally, return error.
            perror("gcc did not exit normally");
            return -1;
        }
        // exited normally, check exit code (supposed to be 0)
        return WEXITSTATUS(status);
    } else if (pid == 0) {
        // then its the child
        // compile
        execlp("gcc", "gcc", path, "-o", EXEC_NAME, NULL);
        perror("Compile: failed to execute gcc");
        exit(EXIT_FAILURE);
    } else {
        // error:
        perror("Failed to fork at compile");
        return -1;
    }
}

/**
 * Runs the students file, defined in EXEC_NAME,
 * saves output in output file defined to be OUTPUT_NAME.
 * @param input_dir
 * @return -1 syscall err, 0 timeout, 1 success
 */
int run(const char *input_dir) {
    pid_t pid;
    int input_fd, output_fd;
    printf(">> -----RUN-----\n");
    // open input and output file, for io redirection
    printf(">> Opening input file %s...\n", input_dir);
    input_fd = open(input_dir, O_RDONLY, 0777);
    if (input_fd < 0) {
        printf(">> Failed to open input file.\n");
        perror("Run: Could not open input file");
        return -1;
    }

    // open output file
    printf(">> Opening output file %s...\n", OUTPUT_NAME);
    output_fd = open(OUTPUT_NAME, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (output_fd < 0) {
        printf(">> Failed to open output file.\n");
        perror("Run: Could not open output file");
        // close the input file
        if (close(input_fd) < 0) {
            perror("Failed to close input file");
        }
        return -1;
    }

    // fork
    printf(">> Forking...\n");
    pid = fork();
    if (pid < 0) {
        // fork failed
        perror("Run: fork failed");
        // close the open file descriptors
        if (close(input_fd) < 0) {
            perror("Failed to close input fd");
        }
        if (close(output_fd) < 0) {
            perror("Failed to close output fd");
        }
        return -1;
    }

    if (pid > 0) {
        // its parent process
        int status;
        int retval;
        int time = 0;

        printf(">> Run: successfully forked, child pid: %d\n", pid);
        retval = 0;
        // wait 5 secs , then handle timeout.
        do {
            // check if child process finished
            if (waitpid(pid, &status, WNOHANG) < 0) {
                // wait failed
                perror("Run: waitpid to child process failed.");
                if (kill(pid, SIGKILL) < 0) {
                    perror("Run: kill child process failed.");
                }
                retval = -1;
                break;
            }

            if (WIFEXITED(status)) {
                break;
            }

            // sleep
            sleep(1);
        } while (++time < 5);

        // if no error occurred during wait
        if (retval == 0) {
            // then either timeout happened or child process exited.
            if (time < 5) {
                // no timeout, child exited
                printf(">> Child finished.\n");
                retval = 1;
            } else {
                // timeout
                retval = 0;
                printf(">> Timeout detected. terminating child with pid %d...\n", pid);
                if (kill(pid, SIGTERM) < 0) {
                    perror("Run: failed to terminate child process");
                    printf(">> Run: Failed to terminate child process, trying to kill...\n");
                    if (kill(pid, SIGKILL) < 0) {
                        perror("Run: failed to kill child process");
                        printf(">>> Run: Failed to kill child process! Call for help and save yourself!\n");
                        retval = -1;
                    }
                }
                // confirm the kill, use wait to get return status and remove the process from
                // kernels process table
                if (retval != -1) {
                    pid_t killed;
                    printf(">> Successfully terminated child process %d, waiting for it to finish...\n", pid);
                    while ((killed = wait(&status)) != pid) {
                        printf(">> Run: wait returned %d, expected %d\n", killed, pid);
                        if (killed < 0) {
                            perror("Run: failed to wait to child process after kill");
                            retval = -1;
                            break;
                        }
                    }
                }
            }
        }

        // close the open file descriptors
        if (close(input_fd) < 0) {
            perror("Failed to close input fd");
            retval = -1;
        }
        if (close(output_fd) < 0) {
            perror("Failed to close output fd");
            retval = -1;
        }
        return retval;
    } else {
        // pid == 0, its child process
        // prepare the argument
        char file[sizeof(EXEC_NAME) + 2];
        sprintf(file, "./%s", EXEC_NAME);

        // redirect stdout and stdin
        if (dup2(output_fd, STDOUT_FILENO) < 0) {
            perror("Run: Failed to redirect stdout to output fd");
            exit(EXIT_FAILURE);
        }
        if (dup2(input_fd, STDIN_FILENO) < 0) {
            perror("Run: Failed to redirect stdin to input file");
            exit(EXIT_FAILURE);
        }

        // execute the run
        execlp(file, file, NULL);
        perror("Run: exec failed");
        exit(EXIT_FAILURE);
    }
}

/**
 * Compares the students output file with the given correct output using compare
 * program defined at COMP_PROG, when students output file defined at OUTPUT_NAME
 * @param out_dir - directory of the correct output file
 * @return on error returns -1, otherwise returns the result of the compare program
 */
int compare_files(const char *out_dir) {
    pid_t pid;
    int devnull;
    printf(">> -----Compare Results-----\n");

    // open devnull and input file, for io redirection
    printf(">> opening devnull...\n");
    devnull = open("/dev/null", O_WRONLY, 0777);
    if (devnull < 0) {
        printf(">> Failed to open devnull. aborting...\n");
        perror("Failed to open devnull");
        return -1;
    }

    printf(">> Forking...\n");
    pid = fork();
    if (pid < 0) {
        perror("CmpRes: fork failed");
        // close the open file descriptors
        if (close(devnull)) {
            perror("Failed to close devnull");
        }
        return -1;
    }

    // fork succeed
    if (pid > 0) {
        // its parent process
        int status;
        int retval = 0;
        pid_t child;
        printf(">> Compare_files: successfully forked, child pid: %d\n", pid);
        // wait for the child process
        printf(">> Waiting for comp to finish...\n");
        while ((child = wait(&status)) != pid) {
            printf(">> Comp: wait returned %d, expected %d\n", child, pid);
            if (child < 0) {
                // wait failed
                perror("CmpRes: wait to child process failed");

                // kill child proccess
                if (kill(pid, SIGKILL) < 0) {
                    perror("CmpRes: kill child process failed");
                }

                // cleanup, close devnull
                if (close(devnull) < 0) {
                    perror("CmpRes: failed to close devnull");
                }
                return -1;
            }
        }

        printf(">> Finished waiting, comp has finished.\n");
        if (!WIFEXITED(status)) {
            // comp failed.
            printf(">> comp crashed. cleaning up...\n");
            // cleanup, close devnull
            if (close(devnull) < 0) {
                perror("CmpRes: failed to close devnull");
            }
            return -1;
        }

        // comp exited normally, get exit status
        retval = WEXITSTATUS(status);
        printf(">> comp exited normally, exit status: %d\n", retval);

        // close the open file descriptors
        if (close(devnull) < 0) {
            perror("Failed to close devnull");
            retval = -1;
        }
        return retval;
    } else {
        // pid == 0, its child process
        char file[sizeof(OUTPUT_NAME) + 2];
        sprintf(file, "./%s", OUTPUT_NAME);

        // redirect stdout and stderr.
        if (dup2(devnull, STDOUT_FILENO) < 0) {
            perror("CmpRes: failed to redirect stdout to devnull");
        }
        /*
        if (dup2(devnull, STDERR_FILENO) < 0) {
            perror("CmpRes: failed to redirect stderr to devnull");
            exit(EXIT_FAILURE);
        }*/

        // execute the compare program
        printf(">> executing: %s %s %s...\n", COMP_PROG, out_dir, OUTPUT_NAME);
        execlp(COMP_PROG, COMP_PROG, out_dir, OUTPUT_NAME, NULL);
        perror("CmpRes: exec failed");
        exit(-1);
    }
}

/**
 * Clears any excessive files that the program might have created
 * @return on error returns -1, 0 otherwise
 */
int clear_files() {
    // if output file exists, delete it
    if (0 == access(OUTPUT_NAME, 0)) {
        // file exists
        if (unlink(OUTPUT_NAME) < 0) {
            perror("Clear: failed to delete output file");
            return -1;
        }
    }

    // if
    if (0 == access(EXEC_NAME, 0)) {
        // file exists
        if (unlink(EXEC_NAME) < 0) {
            perror("Clear: failed to delete exec file");
            return -1;
        }
    }
    return 0;
}

/**
 * Writes details of the student to the details file
 * @param fd - open file descriptor of the details file
 * @param details - the student details
 * @return on error returns -1, otherwise returns result of the write() function.
 */
ssize_t write_student_details(int fd, stud_det_t details) {
    char entry[DET_ENTRY_LEN];
    int calculated_score = 100 - details.search_penalty.penalty - details.ex_penalty.penalty;
    if (calculated_score < 0)
        calculated_score = 0;

    printf("==================================================\n");
    printf("  Student Details:\n");
    printf("--------------------------------------------------\n");
    printf("  (*) Student name: %s\n", details.name);
    printf("  (*) Search penalty: %d. reason: %s\n", details.search_penalty.penalty,
           score_desc_tostring(details.search_penalty.reason));
    printf("  (*) Exercise penalty: %d. reason: %s\n", details.ex_penalty.penalty,
           score_desc_tostring(details.ex_penalty.reason));
    printf("  (*) Final Score: %d\n", calculated_score);
    printf("==================================================\n");

    sprintf(entry, "%s,%d,", details.name, calculated_score);
    // sys err, dont write anything.
    if (details.search_penalty.reason == SYS_ERR || details.ex_penalty.reason == SYS_ERR) {
        return -1;
    }

    // if the search penalty reason leads to that the program could not be runned, write and exit.
    if (details.search_penalty.reason == NO_C_FILE || details.search_penalty.reason == MULTIPLE_DIRECTORIES) {
        strcat(entry, score_desc_tostring(details.search_penalty.reason));
        strcat(entry, "\n");
        return write(fd, entry, strlen(entry));
    }
    // continue if search penalty reason is wrong dir or none. otherwise, error.
    // print it and return.
    switch (details.search_penalty.reason) {
        case WRONG_DIRECTORY:
        case NONE:
            break;
        default:
            printf(">> Invalid search penalty %s\n", score_desc_tostring(details.search_penalty.reason));
            return -1;
    }

    // if search penalty reason is wrong directory, add it to entry.
    if (details.search_penalty.reason == WRONG_DIRECTORY) {
        strcat(entry, score_desc_tostring(WRONG_DIRECTORY));
        strcat(entry, ",");
    }

    // filter invalid ex penalty reasons:
    switch (details.ex_penalty.reason) {
        case COMPILATION_ERROR:
        case TIMEOUT:
        case BAD_OUTPUT:
        case SIMILAR_OUTPUT:
        case GREAT_JOB:
            break;
        default:
            printf(">> Invalid ex penalty: %s\n", score_desc_tostring(details.ex_penalty.reason));
            return -1;
    }

    // add ex penalty description to the entry
    strcat(entry, score_desc_tostring(details.ex_penalty.reason));
    strcat(entry, "\n");

    // write the entry
    return write(fd, entry, strlen(entry));
}

/**
 * generates string representation of the score description
 * @param desc - the score description
 * @return string representation of the score description
 */
char *score_desc_tostring(score_desc_t desc) {
    switch (desc) {
        case SYS_ERR:                   /* Error during system call */
            return "System call error";
        case NONE:                      /* No description (no error) */
            return "No error";
        case NO_C_FILE:                 /* No C file found */
            return "NO_C_FILE";
        case MULTIPLE_DIRECTORIES:      /* Stopped the search because of multiple directories */
            return "MULTIPLE_DIRECTORIES";
        case WRONG_DIRECTORY:           /* File found in wrong directory */
            return "WRONG_DIRECTORY";
        case COMPILATION_ERROR:         /* Error with the compilation */
            return "COMPILATION_ERROR";
        case TIMEOUT:                   /* Timeout of 5 seconds occurred */
            return "TIMEOUT";
        case BAD_OUTPUT:                /* Bad program output */
            return "BAD_OUTPUT";
        case SIMILAR_OUTPUT:            /* Similar program output */
            return "SIMILAR_OUTPUT";
        case GREAT_JOB:                 /* Good program output */
            return "GREAT_JOB";
        default:
            return "Undefined error.";
    }
}