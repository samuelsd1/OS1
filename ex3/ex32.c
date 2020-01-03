/**
 * Name:    David Samuelson
 * ID:      208788851
 */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <memory.h>

#define SHM_SIZE    (4096)

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

/**
 * Writes an error string to stderr
 * @param str - the string
 */
void write_err(char *str) {
    write(STDERR_FILENO, str, strlen(str));
}

#define IS_IN_BOARD(row, col)    ((row >=0) && (row<8) && (col>=0) && (col<8))

typedef struct pos {
    int row;
    int col;
} pos_t;

typedef enum color {
    EMPTY,
    BLACK,
    WHITE
} color_t;

typedef struct game {
    color_t board[8][8];
} game_t;


void init_board(game_t *gptr);
void print_board(game_t *gptr);

int can_place_at(game_t *gptr, pos_t curr, color_t p);
void place_stone(game_t *gptr, pos_t curr, color_t p);

pos_t input_valid_cell(game_t *gptr, color_t p);
int print_valid_cells(game_t *gptr, color_t p);

int get_game_status(game_t *gptr);

static int connected;
int send_pid();

void sigusr_hand(int sig) {
    connected = sig;
}

int game(char *shm);

int main() {
    int shm_id;
    char *shm;
    key_t key;

    int res;

    debug("attaching the signal handler...");
    // attach the signal handler
    if (signal(SIGUSR1, sigusr_hand) < 0) {
        perror("signal");
        exit(EXIT_FAILURE);
    }

    /**
     * Connect to server
     */
    debug("calling send pid...");
    if (send_pid() < 0) {
        write_err("Failed to send pid to server.\n");
        exit(EXIT_FAILURE);
    }

    debug("waiting for sigusr1 signal...");
    connected = 0;
    while (!connected) {
        pause();
    }
    debug("Received sigusr1 signal.");

    /**
     * Connect to the shared memory
     */
    debug("Connecting to the shared memory...");
    // name the key as requested
    key = ftok("ex31.c", 'k');
    shm_id = shmget(key, SHM_SIZE, 0666);
    if (shm_id < 0) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    debug("Success.  Attaching the shared memory...");
    shm = shmat(shm_id, NULL, 0);
    if (shm == (char *) -1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }
    debug("Successfully attached the shared memory.");
    debug("Starting the game...");
    res = game(shm);
    switch(res){
        case 1:
            printf("Winning player: Black\n");
            break;
        case 2:
            printf("Winning player: White\n");
            break;
        case 3:
            printf("No winning player\n");
            break;
        default:
            debug("Game over with unknown status");
            break;
    }

    /**
     * Detach the shared memory
     */
    debug("Detaching the shared memory...");
    if (shmdt(shm) < 0) {
        perror("shmdt");
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}

/**
 * Parses a move from the shared memory
 * @param shm - the shared memory
 * @return - position of the placed rock
 */
pos_t parse_move(char *shm){
    pos_t pos;
    pos.col = shm[1];
    pos.row = shm[2];
    return pos;
}

/**
 * Writes the move to the shared memory
 * @param shm - address of the shared memory
 * @param pos - the position of the rock.
 */
void write_move(char *shm, pos_t pos){
    shm[1] = (char)pos.col;
    shm[2] = (char)pos.row;
    shm[3] = '\0';
}

/**
 * Given my color, returns the opponent color
 * @param my_color - literally, my color.
 * @return - color of my opponent
 */
color_t get_opponent_color(color_t my_color){
    if(my_color == BLACK)
        return WHITE;
    return BLACK;
}

/**
 * Converts the color enum to char
 * @param color - the color enum
 * @return - char representing the color
 */
char color_to_char(color_t color){
    if(color == BLACK)
        return 'b';
    return 'w';
}

/**
 * Converts the game status to a char.
 * @param status - the game status
 * @return - char representing the game status
 */
char status_to_char(int status){
    switch (status){
        case 1:
            return 'b';
        case 2:
            return 'w';
        case 3:
            return 't';
        default:
            return 'r';
    }
}

/**
 * Given the shared memory address, this function handles the game logic.
 * previous parts of the program are responsible for
 * @param shm
 * @return
 */
int game(char *shm) {
    game_t game;
    color_t myColor;

    init_board(&game);

    /* determine my color */
    if (shm[0] == 0x00) {
        // then im first player.
        myColor = BLACK;
    } else {
        // im second player.
        myColor = WHITE;
    }

    // if im second player, wait for first player to play his move.
    if(myColor == WHITE){
        pos_t move;

        while(shm[0] != color_to_char(BLACK)){
            sleep(1);
        }
        move = parse_move(shm);
        place_stone(&game, move, BLACK);
        print_board(&game);
    }

    for(;;){
        pos_t pos;
        // if i cant place cell at any position
        if(print_valid_cells(&game, myColor) <= 0){
            int status = get_game_status(&game);

            // write i lost to shared memory, and exit
            shm[0] = 'o';
            shm[1] = status_to_char(status);
            shm[2] = '\0';

            return status;
        }

        // input a valid cell
        pos = input_valid_cell(&game, myColor);
        // place the stone
        place_stone(&game, pos, myColor);
        // update memory.
        write_move(shm, pos);
        shm[0] = color_to_char(myColor);

        // wait for opponent move
        while(shm[0] == color_to_char(myColor)){
            printf("Waiting for the other player to make a move\n");
            sleep(1);
        }

        // opponent moved. check what he wrote to the memory...
        // if game over, return the status of the game.
        if(shm[0] == 'o'){
            return get_game_status(&game);
        }

        // else, handle his movement:
        pos = parse_move(shm);
        place_stone(&game, pos, get_opponent_color(myColor));
        print_board(&game);
    }
}

/**
 * Function sends the PID to the server
 * @return 0 on success, -1 otherwise
 */
int send_pid() {
    int fd;
    char *my_fifo = "fifo_clientTOserver";
    char buf[64];

    // open the fifo
    debug("opening fifo...\n");
    fd = open(my_fifo, 0666);
    if (fd < 0) {
        perror("open fifo");
        return -1;
    }
    debug("fifo opened");

    // write to fifo
    sprintf(buf, "%d", getpid());
    debug("writing to fifo: ");
    debug(buf);
    if (write(fd, buf, strlen(buf)) < 0) {
        perror("write");
        close(fd);
        perror("close");
        return -1;
    }

    // close the fd
    debug("closing fd...");
    if (close(fd) < 0) {
        perror("close fifo");
    }
    return 0;
}

/**
 * checks the board and returns the game status
 * @param gptr - the game pointer
 * @return Game Status, one of the following:
 *          * Game keeps running:   0
 *          * First (B) player won: 1
 *          * Second(W) player won: 2
 *          * Tie:                  3
 */
int get_game_status(game_t *gptr){
    int available_black;
    int available_white;
    int i,j;


    available_black = print_valid_cells(gptr, BLACK);
    available_white = print_valid_cells(gptr, WHITE);

    // if both players can place stones - game is running.
    if(available_black && available_white){
        return 0;
    }

    // count the stones on the board.
    available_white = 0;
    available_black = 0;
    for(i=0;i<8;++i){
        for(j=0;j<8;++j){
            switch(gptr->board[i][j]){
                case BLACK:
                    available_black++;
                    break;
                case WHITE:
                    available_white++;
                    break;
                default:
                    continue;
            }
        }
    }

    // if we have a tie
    if(available_black == available_white){
        return 3;
    }

    // return the player with the most available stones
    return (available_black > available_white) ? 1 : 2;
}

/**
 * initializes the board
 * @param gptr - the game board
 */
void init_board(game_t *gptr) {
    int row, col;
    for (row = 0; row < 8; ++row) {
        for (col = 0; col < 8; ++col) {
            gptr->board[row][col] = EMPTY;
        }
    }

    gptr->board[3][3] = BLACK;
    gptr->board[4][4] = BLACK;

    gptr->board[3][4] = WHITE;
    gptr->board[4][3] = WHITE;
}

/**
 * Prints the board
 * @param gptr - game ptr
 */
void print_board(game_t *gptr) {
    int row, col;
    printf("The board is:\n");
    for (row = 0; row < 8; ++row) {
        for (col = 0; col < 8; ++col) {
            char c;
            switch (gptr->board[row][col]) {
                case BLACK:
                    c = '2';
                    break;
                case WHITE:
                    c = '1';
                    break;
                default:
                    c = '0';
                    break;
            }
            printf("%c ", c);
        }
        printf("\n");
    }
}

/**
 * Counts how many stones would be flipped in the given direction [dx,dy]
 * if we place stone in the position 'curr', when the color of the
 * current player is p.
 * @param gptr - pointer to the game
 * @param curr - position of the placed stone
 * @param p - the players color
 * @param dy - delta y (used to represent direction)
 * @param dx - delta x (used to represent direction)
 * @return the number of the stones that would be flipped.
 */
int count_flip(game_t *gptr, pos_t curr, color_t p, int dy, int dx) {
    int row, col;
    // Initialize a counter variable to 0
    int counter = 0;

    if (dx == 0 && dy == 0) {
        return 0;
    }

    // Scan outward from the x,y position looking for another piece of color p:
    row = curr.row + dy;
    col = curr.col + dx;
    for (; IS_IN_BOARD(row, col); row += dy, col += dx) {
        // If a blank square or the edge of the board is reached, clear the counter and stop scanning
        if (gptr->board[row][col] == EMPTY) {
            counter = 0;
            break;
        }

        // If a piece of color p is found, stop scanning
        if (gptr->board[row][col] == p) {
            break;
        }
        // Increment the counter
        ++counter;
    }

    // if the edge of the board is reached, clear the counter
    if (!IS_IN_BOARD(row, col)) {
        counter = 0;
    }

    // If the counter is not zero, the move is legal and the counter tells us how many pieces to flip in this direction
    return counter;
}

/**
 * Function returns if a stone with the color p can be placed
 * at the position 'curr'.
 * @param gptr - the game pointer
 * @param curr - the position to place the stone
 * @param p - the color of the stone to be placed
 * @return 1 if the stone with the color p can beplaced at the position 'curr'
 */
int can_place_at(game_t *gptr, pos_t curr, color_t p) {
    int flipped;
    int dx, dy;

    // To see if it is legal to place a piece of color p at position x,y, do:
    // If the x,y square is already occupied, the move is not legal
    if (gptr->board[curr.row][curr.col] != EMPTY) {
        return 0;
    }

    flipped = 0;
    // for each direction
    for (dy = -1; dy <= 1; ++dy) {
        for (dx = -1; dx <= 1; ++dx) {
            // count the flips.
            flipped += count_flip(gptr, curr, p, dy, dx);
        }
    }
    // If we've checked all the directions and haven't flipped any pieces, the move is not legal
    return flipped;
}

/**
 * flips the stones after stone with color p was placed at the position 'curr'
 * towards the direction [dx,dy].
 * @param gptr - the game pointer
 * @param curr - position of the placed stone
 * @param p - color of the placed stone
 * @param dy - delta y, used to represent direction
 * @param dx - delta x, used to represent direction
 */
void flip(game_t *gptr, pos_t curr, color_t p, int dy, int dx) {
    int row, col;

    // except no direction
    if (!dx && !dy)
        return;

    // if current position is no in the color p
    if (gptr->board[curr.row][curr.col] != p)
        return;

    // if there are no flipped stones in that direction, dont flip.
    if (count_flip(gptr, curr, p, dy, dx) <= 0) {
        return;
    }

    // Scan outward from the x,y position
    row = curr.row + dy;
    col = curr.col + dx;
    for (; IS_IN_BOARD(row, col); row += dy, col += dx) {
        // If a blank square or the edge of the board is reached, clear the counter and stop scanning
        if (gptr->board[row][col] == EMPTY) {
            break;
        }
        // If a piece of player color is found, stop scanning
        if (gptr->board[row][col] == p) {
            break;
        }

        // the color is opposite. flip it.
        gptr->board[row][col] = p;
    }
}

/**
 * places a stone in the given position (if it is legal position)
 * @param gptr - game pointer
 * @param curr - position to place the stone
 * @param p - color of the stone
 */
void place_stone(game_t *gptr, pos_t curr, color_t p) {
    int dx, dy;
    if (!can_place_at(gptr, curr, p)) {
        return;
    }

    // place the stone
    gptr->board[curr.row][curr.col] = p;

    // for each direction
    for (dy = -1; dy <= 1; ++dy) {
        for (dx = -1; dx <= 1; ++dx) {
            flip(gptr, curr, p, dy, dx);
        }
    }
}

/**
 * inputs a cell and makes sure its valid while
 * printing the requested messages.
 * @param gptr - the game pointer
 * @param p - color of the player
 * @return - the entered position , which guaranteed to be legal.
 */
pos_t input_valid_cell(game_t *gptr, color_t p) {
    pos_t c;
    printf("Please choose a square\n");
    do {
        scanf(" [%d,%d]", &c.col, &c.row);
        if (!IS_IN_BOARD(c.row, c.col)) {
            printf("No such square\nPlease choose another square\n");
            continue;
        }

        if (!can_place_at(gptr, c, p)) {
            printf("This square is invalid\nPlease choose another square\n");
            continue;
        }
        break;
    } while (1);

    return c;
}

/**
 * prints the valid cells to place a stone with the color p
 * @param gptr - the game pointer
 * @param p - color of the stone
 * @return - number of the valid cells thaht the stone p can be placed
 */
int print_valid_cells(game_t *gptr, color_t p) {
    int row, col;
    int count;
    char buf[32];
    debug("Valid cells (in format [x,y]");
    count = 0;
    for (row = 0; row < 8; ++row) {
        for (col = 0; col < 8; ++col) {
            pos_t pos;
            pos.row = row;
            pos.col = col;

            if (can_place_at(gptr, pos, p)) {
                ++count;
                sprintf(buf, "\t* [%d,%d]\n", col, row);
                debug(buf);
            }
        }
    }
    return count;
}