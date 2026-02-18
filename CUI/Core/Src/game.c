#include "game.h"
#include <stdlib.h>
#include <string.h>

// Глобальні змінні, оголошені в game.h
uint8_t board[BOARD_ROWS][BOARD_COLS];
uint32_t score = 0;

/* --- ПРОТОТИПИ ВНУТРІШНІХ ФУНКЦІЙ (Всі static) --- */
static uint8_t GetValidRandomColor(void);
static void Game_ApplyGravityInternal(void);
static int Game_CheckAndRemoveMatches(void);
static uint32_t GetScoreForCount(uint8_t count);
static void Game_RunGravityLoop(void);
static void Game_FillEmptyCells(void); // Тепер вона static і не конфліктує з .h

/* --- ПУБЛІЧНІ ФУНКЦІЇ --- */

void Game_Init(void) {
    score = 0;
    memset(board, 0, sizeof(board));
    Game_FillEmptyCells(); // Виклик внутрішньої функції
}

uint8_t Game_Swap(uint8_t r1, uint8_t c1, uint8_t r2, uint8_t c2) {
    if (r1 >= BOARD_ROWS || c1 >= BOARD_COLS || r2 >= BOARD_ROWS || c2 >= BOARD_COLS) {
        return 0;
    }

    // Перевірка на сусідство (сума різниць координат має бути 1)
    int diff_r = abs((int)r1 - (int)r2);
    int diff_c = abs((int)c1 - (int)c2);

    if ((diff_r + diff_c) != 1) {
        return 0;
    }

    // Тимчасовий обмін
    uint8_t temp = board[r1][c1];
    board[r1][c1] = board[r2][c2];
    board[r2][c2] = temp;

    if (Game_CheckAndRemoveMatches()) {
        Game_RunGravityLoop(); // Запуск ланцюгової реакції
        return 1; // Успіх
    } else {
        // Скасування обміну, якщо лінія не зібралася
        board[r2][c2] = board[r1][c1];
        board[r1][c1] = temp;
        return 0;
    }
}

/* --- ПРИВАТНІ ФУНКЦІЇ (ЛОГІКА) --- */

static void Game_RunGravityLoop(void) {
    int matches_found;
    do {
        Game_FillEmptyCells();
        matches_found = Game_CheckAndRemoveMatches();
    } while (matches_found);
}

static void Game_FillEmptyCells(void) {
    Game_ApplyGravityInternal();

    for (int j = 0; j < BOARD_COLS; j++) {
        for (int i = 0; i < BOARD_ROWS; i++) {
            if (board[i][j] == 0) {
                board[i][j] = GetValidRandomColor();
            }
        }
    }
}

static void Game_ApplyGravityInternal(void) {
    for (int j = 0; j < BOARD_COLS; j++) {
        int write_row = BOARD_ROWS - 1;
        for (int read_row = BOARD_ROWS - 1; read_row >= 0; read_row--) {
            if (board[read_row][j] != 0) {
                board[write_row][j] = board[read_row][j];
                if (write_row != read_row) board[read_row][j] = 0;
                write_row--;
            }
        }
    }
}

static int Game_CheckAndRemoveMatches(void) {
    uint8_t marked[BOARD_ROWS][BOARD_COLS];
    memset(marked, 0, sizeof(marked));
    int found_match = 0;

    // Горизонталь (4 в ряд)
    for (int i = 0; i < BOARD_ROWS; i++) {
        for (int j = 0; j <= BOARD_COLS - 4; j++) {
            uint8_t color = board[i][j];
            if (color == 0) continue;
            if (board[i][j+1] == color && board[i][j+2] == color && board[i][j+3] == color) {
                found_match = 1;
                marked[i][j] = marked[i][j+1] = marked[i][j+2] = marked[i][j+3] = 1;
            }
        }
    }

    // Вертикаль (4 в ряд)
    for (int j = 0; j < BOARD_COLS; j++) {
        for (int i = 0; i <= BOARD_ROWS - 4; i++) {
            uint8_t color = board[i][j];
            if (color == 0) continue;
            if (board[i+1][j] == color && board[i+2][j] == color && board[i+3][j] == color) {
                found_match = 1;
                marked[i][j] = marked[i+1][j] = marked[i+2][j] = marked[i+3][j] = 1;
            }
        }
    }

    if (found_match) {
        uint8_t count = 0;
        for (int i = 0; i < BOARD_ROWS; i++) {
            for (int j = 0; j < BOARD_COLS; j++) {
                if (marked[i][j]) {
                    board[i][j] = 0;
                    count++;
                }
            }
        }
        score += GetScoreForCount(count);
    }
    return found_match;
}

static uint32_t GetScoreForCount(uint8_t count) {
    return (uint32_t)count * 10;
}

static uint8_t GetValidRandomColor(void) {
    return (rand() % 6) + 1;
}
