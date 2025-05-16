#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wasi.h>

#include <example.h>
#include <minesweeper.h>


void print_string(const char* str, uint32_t len) {
    ciovec_t iov = { (void *)str, len };
    size_t nwritten;
    fd_write(1, &iov, 1, &nwritten);
}

void log_message(const char *text) {
    size_t len = strlen(text);
    char newline = '\n';
    print_string(text, len);
    print_string(&newline, 1);
}

// Export the add function for JavaScript and Wasmtime
int add(int a, int b) {
    log_message("Adding two numbers");
    return a + b;
}

double mysin(double a) {
    log_message("Calculating sine");
    return sin(a);
}


void arena_init(Arena* arena, size_t size) {
    // TODO
    //arena->memory = malloc(size);
    arena->size = size;
    arena->offset = 0;
}

void* arena_alloc(Arena* arena, size_t size) {
    if (arena->offset + size > arena->size) {
        return NULL; // Out of memory
    }
    void* ptr = arena->memory + arena->offset;
    arena->offset += size;
    return ptr;
}

void arena_reset(Arena* arena) {
    arena->offset = 0;
}

void arena_free(Arena* arena) {
    // TODO
    //free(arena->memory);
    arena->memory = NULL;
    arena->size = 0;
    arena->offset = 0;
}

static int get_index(const Board* board, int x, int y) {
    return y * board->width + x;
}

static int is_valid(const Board* board, int x, int y) {
    return x >= 0 && x < board->width && y >= 0 && y < board->height;
}

void board_init(Board* board, Arena* arena, int width, int height, int mine_count) {
    board->width = width;
    board->height = height;
    board->mine_count = mine_count;
    board->revealed_count = 0;
    board->game_over = 0;
    board->won = 0;
    size_t cells_size = width * height * sizeof(uint8_t);
    board->cells = arena_alloc(arena, cells_size);
    if (board->cells == NULL) {
        exit(1); // Handle allocation failure
    }
    memset(board->cells, 0, cells_size);
    board_reset(board, mine_count);
}

void board_reset(Board* board, int mine_count) {
    board->mine_count = mine_count;
    board->revealed_count = 0;
    board->game_over = 0;
    board->won = 0;
    memset(board->cells, 0, board->width * board->height * sizeof(uint8_t));
    srand(time(NULL));
    for (int i = 0; i < mine_count; i++) {
        int x, y, idx;
        do {
            x = rand() % board->width;
            y = rand() % board->height;
            idx = get_index(board, x, y);
        } while ((board->cells[idx] & 0xF) == MINE_VALUE);
        board->cells[idx] = MINE_VALUE;
    }
    for (int y = 0; y < board->height; y++) {
        for (int x = 0; x < board->width; x++) {
            if ((board->cells[get_index(board, x, y)] & 0xF) == MINE_VALUE) continue;
            int count = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    int nx = x + dx, ny = y + dy;
                    if (is_valid(board, nx, ny) && (board->cells[get_index(board, nx, ny)] & 0xF) == MINE_VALUE) {
                        count++;
                    }
                }
            }
            board->cells[get_index(board, x, y)] |= count;
        }
    }
}

int board_reveal_cell(Board* board, int x, int y) {
    if (board->game_over || !is_valid(board, x, y)) return 0;
    int idx = get_index(board, x, y);
    uint8_t* cell = &board->cells[idx];
    if ((*cell & REVEALED_FLAG) || (*cell & FLAGGED_FLAG)) return 0;
    *cell |= REVEALED_FLAG;
    *cell &= ~FLAGGED_FLAG;
    board->revealed_count++;
    if ((*cell & 0xF) == MINE_VALUE) {
        board->game_over = 1;
        return -1; // Hit a mine
    }
    if ((*cell & 0xF) == 0) {
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue;
                board_reveal_cell(board, x + dx, y + dy);
            }
        }
    }
    if (board->revealed_count == board->width * board->height - board->mine_count) {
        board->game_over = 1;
        board->won = 1;
    }
    return 1;
}

int board_flag_cell(Board* board, int x, int y) {
    if (board->game_over || !is_valid(board, x, y)) return 0;
    int idx = get_index(board, x, y);
    uint8_t* cell = &board->cells[idx];
    if (*cell & REVEALED_FLAG) return 0;
    *cell ^= FLAGGED_FLAG;
    return 1;
}

int board_get_game_state(Board* board) {
    if (board->game_over) return board->won ? 2 : 1;
    return 0;
}

uint8_t board_get_cell(Board* board, int x, int y) {
    if (!is_valid(board, x, y)) return 0;
    return board->cells[get_index(board, x, y)];
}
