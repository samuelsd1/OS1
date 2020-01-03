
#include <stdio.h>
#include <stdint.h>
#include <memory.h>

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

int main() {
    int i;
    game_t game;
    pos_t pos;

    init_board(&game);

    for (i = 0; i < 44; ++i) {
        color_t color;
        print_board(&game);

        printf("Turn of player ");
        if (i % 2 == 0) {
            color = BLACK;
            printf("BLACK\n");
        } else {
            color = WHITE;
            printf("WHITE\n");
        }

        print_valid_cells(&game, color);
        pos = input_valid_cell(&game, color);
        place_stone(&game, pos, color);
    }

    printf("Game over\n");
    return 0;
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

    /*
    gptr->board[2][2] = BLACK;
    gptr->board[2][3] = BLACK;
    gptr->board[2][4] = BLACK;
    gptr->board[2][5] = BLACK;
    gptr->board[2][6] = BLACK;
    gptr->board[3][2] = BLACK;
    gptr->board[3][3] = BLACK;
    gptr->board[4][2] = BLACK;
    gptr->board[4][4] = BLACK;

    gptr->board[3][4] = WHITE;
    gptr->board[4][3] = WHITE;
    gptr->board[5][2] = WHITE;
     */
}

/**
 * Prints the board
 * @param gptr - game ptr
 */
void print_board(game_t *gptr) {
    int row, col;
    printf("The board is:\n");
    printf("|---|---|---|---|---|---|---|---|\n");
    for (row = 0; row < 8; ++row) {
        printf("| ");
        for (col = 0; col < 8; ++col) {
            char c;
            switch (gptr->board[row][col]) {
                case BLACK:
                    c = 'B';
                    break;
                case WHITE:
                    c = 'W';
                    break;
                default:
                    c = ' ';
                    break;
            }
            printf("%c | ", c);
        }
        printf("\n|---|---|---|---|---|---|---|---|\n");
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
int flip(game_t *gptr, pos_t curr, color_t p, int dy, int dx) {
    int row, col;
    int counter = 0;
    // except no direction
    if (!dx && !dy)
        return 0;

    // if current position is no in the color p
    if (gptr->board[curr.row][curr.col] != p)
        return 0;

    // if there are no flipped stones in that direction, dont flip.
    if (count_flip(gptr, curr, p, dy, dx) <= 0) {
        return 0;
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
        ++counter;
    }
    return counter;
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
            int flipped = count_flip(gptr, curr, p, dy, dx);
            if(flipped > 0){
                flip(gptr, curr, p, dy, dx);
            }
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

        printf("Inputted: row: %d, col: %d\n", c.row, c.col);
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
    printf("Valid cells (in format [x,y]):\n");
    count = 0;
    for (row = 0; row < 8; ++row) {
        for (col = 0; col < 8; ++col) {
            pos_t pos;
            pos.row = row;
            pos.col = col;

            if (can_place_at(gptr, pos, p)) {
                printf("\t* [%d,%d]\n", col, row);
                ++count;
            }
        }
    }
    return count;
}