#include "minesweeper.h"
#include <assert.h>
#include <stdio.h>

int main() {
    Arena arena;
    arena_init(&arena, 1024 * 1024); // 1MB should be enough

    Board board;
    board_init(&board, &arena, 10, 10, 10);

    // Test initial state
    assert(board.width == 10);
    assert(board.height == 10);
    assert(board.mine_count == 10);
    assert(board.revealed_count == 0);
    assert(board.game_over == 0);
    assert(board.won == 0);

    // Count mines
    int mine_count = 0;
    for (int y = 0; y < 10; y++) {
        for (int x = 0; x < 10; x++) {
            uint8_t cell = board_get_cell(&board, x, y);
            if ((cell & 0xF) == MINE_VALUE) mine_count++;
            assert((cell & REVEALED_FLAG) == 0); // All hidden
            assert((cell & FLAGGED_FLAG) == 0); // Not flagged
        }
    }
    assert(mine_count == 10);

    // Reveal a non-mine cell
    int x, y;
    for (y = 0; y < 10; y++) {
        for (x = 0; x < 10; x++) {
            if ((board_get_cell(&board, x, y) & 0xF) != MINE_VALUE) break;
        }
        if (x < 10) break;
    }
    assert(x < 10 && y < 10);
    int result = board_reveal_cell(&board, x, y);
    assert(result == 1);
    assert((board_get_cell(&board, x, y) & REVEALED_FLAG) != 0);
    assert(board.revealed_count > 0);

    // Flag a hidden cell
    int flag_x, flag_y;
    for (flag_y = 0; flag_y < 10; flag_y++) {
        for (flag_x = 0; flag_x < 10; flag_x++) {
            if ((board_get_cell(&board, flag_x, flag_y) & REVEALED_FLAG) == 0) break;
        }
        if (flag_x < 10) break;
    }
    assert(flag_x < 10 && flag_y < 10);
    result = board_flag_cell(&board, flag_x, flag_y);
    assert(result == 1);
    assert((board_get_cell(&board, flag_x, flag_y) & FLAGGED_FLAG) != 0);

    // Try to reveal a flagged cell
    result = board_reveal_cell(&board, flag_x, flag_y);
    assert(result == 0); // Should not reveal

    // Unflag the cell
    result = board_flag_cell(&board, flag_x, flag_y);
    assert(result == 1);
    assert((board_get_cell(&board, flag_x, flag_y) & FLAGGED_FLAG) == 0);

    // Reveal the unflagged cell
    result = board_reveal_cell(&board, flag_x, flag_y);
    assert(result == 1);
    assert((board_get_cell(&board, flag_x, flag_y) & REVEALED_FLAG) != 0);

    arena_free(&arena);
    printf("All tests passed.\n");
    return 0;
}