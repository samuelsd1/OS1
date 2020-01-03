/**
 * Name:    David Samuelson
 * ID:      208788851
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>

#define BOARD_BUF_SIZE  100

typedef struct board {
    unsigned short board[4][4];
} board_t;

void install_sigusr_hand(void);

void read_board(board_t *bptr);
void print_board(const board_t *bptr);

/**
 * prints a string to stdout
 * @param str - the string to print
 */
void print(char *str) {
    if (write(STDOUT_FILENO, str, strlen(str)) < 0) {
        perror("Failed to write");
        exit(EXIT_FAILURE);
    }
}

/**
 * clears the screen
 */
void clear_screen() {
    print("\033[4A");
}


int main() {
    // install the sigusr handler
    install_sigusr_hand();

    while (1) {
        board_t b;
        pause();
        // sigusr func returned, read board, clear screen and print the board.
        read_board(&b);
        clear_screen();
        print_board(&b);
    }
}

/**
 * Counts the commas in the received string
 * @param str - the string to cound the commas in
 * @return the number of commas in the received string.
 */
int count_commas(char *str) {
    long len, i;
    int count = 0;
    if (str) {
        for (i = 0, len = strlen(str); i < len; ++i) {
            if (str[i] == ',')
                ++count;
        }
    }
    return count;
}

/**
 * returns whether the given char is number
 * @param c - the char
 * @return whether the given char is number
 */
int is_num(char c) {
    return (c >= '0' && c <= '9');
}

/**
 * converts the given string to ushort
 * @param num - the given string
 * @return on failure return -1, otherwise return the conversion of the number
 */
short convert_to_ushort(char *num) {
    long len, i;
    unsigned short converted = 0;
    if (!num)
        return -1;

    len = strlen(num);
    if (len > 4 || len == 0)
        return -1;

    for (i = 0; i < len; ++i) {
        int n;
        if (!is_num(num[i]))
            return -1;
        n = num[i] - '0';
        converted += n;
        if (i < len - 1)
            converted *= 10;
    }
    return converted;
}

/**
 * function reads 2048-board representing string from stdin, and initializes
 * the received board.
 * @param bptr - the 2048 board to initialize from the inputted string
 */
void read_board(board_t *bptr) {
    int res;
    char input[BOARD_BUF_SIZE];
    char buf[16];
    char *token;
    int count;

    if (!bptr)
        return;

    res = (int) read(STDIN_FILENO, input, BOARD_BUF_SIZE);
    if (res < 0) {
        perror("read");
        exit(EXIT_FAILURE);
    }

    // count commas - if there are less or more than 15
    if (count_commas(input) != 15) {
        // then check if string contains "win" or "lose"
        if (strstr(input, "win") != NULL) {
            // input contains the word "win" => need to close
            print("Congratulations!\n");
        }
        if (strstr(input, "lose") != NULL) {
            //input contains the work "lose" => need to close
            print("Game Over!\n");
        }
        exit(EXIT_SUCCESS);
    }

    count = 0;
    token = strtok(input, ",");
    while (token != NULL) {
        // convert the read token to ushort
        short num = convert_to_ushort(token);
        // if failed to convert - return failure
        if (num < 0)
            return;

        // fill the bptr at the right place
        bptr->board[count / 4][count % 4] = (unsigned short) num;

        // keep reading from stdin. if we reading the last num - read until \n
        token = (++count < 15)
                ? strtok(NULL, ",")
                : strtok(NULL, "\n");
    }
}

/**
 * prints the given board to stdout
 * @param bptr - the given board
 */
void print_board(const board_t *bptr) {
    int i, j;
    char buf[6];
    if (!bptr)
        return;

    for (i = 0; i < 4; ++i) {
        print("|");

        for (j = 0; j < 4; ++j) {
            unsigned short num = bptr->board[i][j];
            if (num > 0) {
                sprintf(buf, "%.4d|", num);
            } else {
                sprintf(buf, "    |");
            }
            print(buf);
        }
        print("\n");
    }
}

/**
 * empty func, handles sigusr
 * @param sig
 */
void sigusr_hand(int sig) {
    // empty func
}

/**
 * installs sigusr handler
 */
void install_sigusr_hand(void) {
    struct sigaction setup_action;
    sigset_t block_mask;

    memset(&setup_action, 0, sizeof setup_action);

    sigfillset(&block_mask);
    sigdelset(&block_mask, SIGINT);
    setup_action.sa_handler = sigusr_hand;
    setup_action.sa_mask = block_mask;
    setup_action.sa_flags = 0;
    if (sigaction(SIGUSR1, &setup_action, NULL) < 0) {
        perror("sigusr sigaction");
        exit(EXIT_FAILURE);
    }
}