/**
 * Name:    David Samuelson
 * ID:      208788851
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>

#define BOARD_BUF_SIZE  100
typedef struct board {
    unsigned short board[4][4];
} board_t;


board_t brd;        /* the 2048 board */
pid_t print_pid;    /* the print pid */

void install_sigalarm_hand(void);
void install_sigint_hand(void);

void initialize_board(board_t *bptr);
void add_number(board_t *bptr);
void print_board(board_t *bptr);

void check_game(board_t *bptr);
void handle_key_pressed(char key, board_t *bptr);

/**
 * exits nicely by sending sigint to father proccess
 */
void exit_nicely(void) {
    alarm(0);
    system("stty cooked echo");
    kill(getppid(), SIGINT);
    exit(EXIT_SUCCESS);
}

/**
 * sends sigusr1 to the print proccess
 */
void send_sigusr1(void) {
    // send the sigusr1 signal to the print proccess
    if (kill(print_pid, SIGUSR1) < 0) {
        perror("kill");
        exit_nicely();
    }
}

/**
 * prints a given string to stdout
 * @param str - the given string
 */
void print(char *str) {
    if (write(STDOUT_FILENO, str, strlen(str)) < 0) {
        perror("write");
        exit_nicely();
    }
}

/**
 * generate random x
 * @return random number between 1 and 5 (both inclusive)
 */
unsigned int generate_x() {
    return ((rand() % 5) + 1);
}


int main(int argc, char **argv) {
    // seed the random generator
    srand(time(NULL));

    // if didn't receive the argument
    if (argc <= 1) {
        exit(EXIT_FAILURE);
    }

    /* attach the signal handlers */
    install_sigalarm_hand();
    install_sigint_hand();
    print_pid = atoi(argv[1]);

    system("stty cbreak -echo");

    // start the game
    initialize_board(&brd);
    print_board(&brd);
    while (1) {
        int ch;
        alarm(generate_x());

        ch = getchar();
        if (ch == -1) //if interrupted, continue
            continue;
        alarm(0);
        handle_key_pressed(ch, &brd);
        check_game(&brd);
        print_board(&brd);
    }
}

/**
 * initializes the board given board with zeroes then inserts two
 * numbers at random locations.
 * @param bptr - the given board
 */
void initialize_board(board_t *bptr) {
    int i, j;
    int init;
    if (!bptr)
        return;

    // initialize with zeroes
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            bptr->board[i][j] = 0;
        }
    }

    // add two random 2's
    for (init = 0; init < 2; ++init) {
        i = rand() % 4;
        j = rand() % 4;
        bptr->board[i][j] = 2;
    }
}

/**
 * adds 2 in random location on the board
 * @param bptr - the given board
 */
void add_number(board_t *bptr) {
    unsigned int empty_cells = 0;
    unsigned int index_list[4 * 4][2];
    unsigned int i, j;
    if (!bptr)
        return;

    // count the empty cells
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            if (bptr->board[i][j] == 0) {
                // save the index of the empty cell
                index_list[empty_cells][0] = i;
                index_list[empty_cells][1] = j;
                ++empty_cells;
            }
        }
    }

    // if found empty cells
    if (empty_cells) {
        // choose random empty cell and add there number
        int rand_idx = rand() % empty_cells;
        i = index_list[rand_idx][0];
        j = index_list[rand_idx][1];
        bptr->board[i][j] = 2;
    }
}

/**
 * checks whether the last board was on win state, lose state.
 * @return On win returns 1, On lose returns -1, otherwise returns 0
 */
int check_win(board_t *bptr) {
    int i, j;
    // check win first
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            // if found 2048 cell
            if (bptr->board[i][j] == 2048) {
                return 1;
            }
        }
    }

    // now check lose. if found empty cell return 0, else return lose
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            // if found empty cell
            if (bptr->board[i][j] == 0) {
                return 0;
            }
        }
    }

    // no empty cell found - return lose.
    return -1;
}

void check_game(board_t *bptr) {
    int res;
    if (!bptr)
        return;

    res = check_win(bptr);
    // if win
    if (res == 1) {
        print("win");
        send_sigusr1();
        exit_nicely();
    }
    // if lose
    if (res == -1) {
        print("lose");
        send_sigusr1();
        exit_nicely();
    }
}

/**
 * prints the given board to stdout in row format,
 * then signals the print pid
 * @param bptr - the given board
 */
void print_board(board_t *bptr) {
    int i, j;
    char buf[BOARD_BUF_SIZE];

    if (!bptr)
        return;

    memset(buf, 0, sizeof(buf));
    sprintf(buf, "%d", bptr->board[0][0]); //add first number
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            char num[6];
            // if its first number continue because we already printed it
            if (i == 0 && j == 0)
                continue;

            // add the next numbers with comma before
            sprintf(num, ",%d", bptr->board[i][j]);
            strcat(buf, num);
        }
    }

    // if failed to write
    if (write(STDOUT_FILENO, buf, strlen(buf)) < 0) {
        perror("upd write");
        exit_nicely();
    }

    send_sigusr1();
}

/**
 * moves left the given board by the 2048 logic
 * @param bptr - the given board
 */
void move_left(board_t *bptr) {
    int i, j;
    int current_cell;

    if (!bptr)
        return;

    for (i = 0; i < 4; ++i) {
        // logic for each row
        for (current_cell = 0; current_cell < 4; ++current_cell) {
            unsigned short num = 0;
            // seek the first nonzero number from the left.
            for (j = current_cell; j < 4; ++j) {
                if ((num = bptr->board[i][j]) != 0)
                    break;
            }

            if (num == 0)
                continue;

            // move the first num to the left
            bptr->board[i][j] = 0;
            bptr->board[i][current_cell] = num;

            // seek for number equal to num.
            for (j = current_cell + 1; j < 4; ++j) {
                if (bptr->board[i][j] == 0)
                    continue;
                if (bptr->board[i][j] != num)
                    break;

                // bptr[i][j] = num
                bptr->board[i][j] = 0;
                bptr->board[i][current_cell] *= 2;
                break;
            }
        }
    }
}

/**
 * rotates the given board left
 * @param bptr - the given board
 */
void rotate_left(board_t *bptr) {
    int i, j;
    board_t copy;
    if (!bptr)
        return;
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            copy.board[3 - j][i] = bptr->board[i][j];
        }
    }
    *bptr = copy;
}

/**
 * moves the board up by the 2048 logic
 * @param bptr - the given board
 */
void move_up(board_t *bptr) {
    // rotate 1 counter-clockwise
    rotate_left(bptr);
    // perform the move
    move_left(bptr);
    // rotate back
    rotate_left(bptr);
    rotate_left(bptr);
    rotate_left(bptr);
}

/**
 * moves the board down by the 2048 logic
 * @param bptr - the given board
 */
void move_down(board_t *bptr) {
    // rotate clockwise once
    rotate_left(bptr);
    rotate_left(bptr);
    rotate_left(bptr);
    // perform the move
    move_left(bptr);
    // rotate back
    rotate_left(bptr);
}

/**
 * moves the board right by the 2048 logic
 * @param bptr - the given board
 */
void move_right(board_t *bptr) {
    // rotate counter clockwise twice
    rotate_left(bptr);
    rotate_left(bptr);
    // perform the move
    move_left(bptr);
    // rotate back
    rotate_left(bptr);
    rotate_left(bptr);
}

/**
 * handles the key press by the user.
 * @param key - the pressed key
 * @param bptr - the board
 */
void handle_key_pressed(char key, board_t *bptr) {
    if (!bptr)
        return;

    switch (key) {
        case 'W':
        case 'w':
            move_up(bptr);
            break;
        case 'A':
        case 'a':
            move_left(bptr);
            break;
        case 'X':
        case 'x':
            move_down(bptr);
            break;
        case 'D':
        case 'd':
            move_right(bptr);
            break;
        case 'S':
        case 's':
            initialize_board(bptr);
            break;
        default:
            break;
    }
}

/**
 * alarm handler, does what defined in the targil
 * @param sig - the received signal
 */
void alarm_hand(int sig) {
    add_number(&brd);
    check_game(&brd);
    print_board(&brd);
    alarm(generate_x());
}

/**
 * installs theh alarm handler, blocking all signals but sigint
 */
void install_sigalarm_hand() {
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
 * handles sigint by exiting nicely
 * @param sig  - the received signal
 */
void sigint_hand(int sig) {
    exit_nicely();
}

/**
 * installs sigint handler
 */
void install_sigint_hand() {
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