#ifndef MINESWEEPER_H
#define MINESWEEPER_H

#include <stdint.h>
#include <stddef.h>

#define MINE_VALUE 9
#define REVEALED_FLAG (1 << 4)
#define FLAGGED_FLAG (1 << 5)

typedef struct {
    char* memory;
    size_t size;
    size_t offset;
} Arena;

void arena_init(Arena* arena, size_t size);
void* arena_alloc(Arena* arena, size_t size);
void arena_reset(Arena* arena);
void arena_free(Arena* arena);

typedef struct {
    int width;
    int height;
    uint8_t* cells;
    int mine_count;
    int revealed_count;
    int game_over;
    int won;
} Board;

void board_init(Board* board, Arena* arena, int width, int height, int mine_count);
void board_reset(Board* board, int mine_count);
int board_reveal_cell(Board* board, int x, int y);
int board_flag_cell(Board* board, int x, int y);
int board_get_game_state(Board* board);
uint8_t board_get_cell(Board* board, int x, int y);

#endif // MINESWEEPER_H