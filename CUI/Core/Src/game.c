#include "game.h"
#include <stdlib.h>
#include <string.h>

// Глобальні змінні
uint8_t board[BOARD_ROWS][BOARD_COLS];
uint32_t score = 0;

/* --- ПРОТОТИПИ --- */
static uint8_t GetValidRandomColor(int r, int c);
static void Game_ApplyGravityInternal(void);
static int Game_CheckAndRemoveMatches(void);
static uint32_t GetScoreForCount(uint8_t count);
static void Game_RunGravityLoop(void);
static void Game_FillEmptyCells(void);
static int Game_IsMatchPresent(void); // НОВИЙ ПРОТОТИП: для перевірки на Глухий кут

/* --- ПУБЛІЧНІ ФУНКЦІЇ --- */

void Game_Init(void) {
    score = 0;

    // Заповнюємо поле так, щоб одразу не було збігів
    for (int r = 0; r < BOARD_ROWS; r++) {
        for (int c = 0; c < BOARD_COLS; c++) {
            board[r][c] = GetValidRandomColor(r, c);
        }
    }
}

uint8_t Game_Swap(uint8_t r1, uint8_t c1, uint8_t r2, uint8_t c2) {
    if (r1 >= BOARD_ROWS || c1 >= BOARD_COLS || r2 >= BOARD_ROWS || c2 >= BOARD_COLS) return 0;

    int diff_r = abs((int)r1 - (int)r2);
    int diff_c = abs((int)c1 - (int)c2);
    if ((diff_r + diff_c) != 1) return 0;

    // Тимчасовий обмін
    uint8_t temp = board[r1][c1];
    board[r1][c1] = board[r2][c2];
    board[r2][c2] = temp;

    if (Game_CheckAndRemoveMatches()) {
        Game_RunGravityLoop();
        return 1;
    } else {
        // Скасування обміну
        board[r2][c2] = board[r1][c1];
        board[r1][c1] = temp;
        return 0;
    }
}

// НОВА ПУБЛІЧНА ФУНКЦІЯ: Перевіряє, чи є можливі ходи на полі
uint8_t Game_HasPossibleMoves(void) {
    for (int r = 0; r < BOARD_ROWS; r++) {
        for (int c = 0; c < BOARD_COLS; c++) {

            // 1. Пробуємо обмін з ПРАВИМ сусідом
            if (c < BOARD_COLS - 1) {
                uint8_t temp = board[r][c];
                board[r][c] = board[r][c+1];
                board[r][c+1] = temp;

                int has_match = Game_IsMatchPresent();

                // Повертаємо кубики на місце
                board[r][c+1] = board[r][c];
                board[r][c] = temp;

                if (has_match) return 1;
            }

            // 2. Пробуємо обмін з НИЖНІМ сусідом
            if (r < BOARD_ROWS - 1) {
                uint8_t temp = board[r][c];
                board[r][c] = board[r+1][c];
                board[r+1][c] = temp;

                int has_match = Game_IsMatchPresent();

                // Повертаємо кубики на місце
                board[r+1][c] = board[r][c];
                board[r][c] = temp;

                if (has_match) return 1;
            }
        }
    }
    return 0; // Глухий кут! Ходів немає
}

/* --- ПРИВАТНІ ФУНКЦІЇ --- */

static void Game_RunGravityLoop(void) {
    int matches_found;
    do {
        Game_FillEmptyCells();
        matches_found = Game_CheckAndRemoveMatches();
    } while (matches_found);
}

static void Game_FillEmptyCells(void) {
    Game_ApplyGravityInternal();

    for (int r = 0; r < BOARD_ROWS; r++) {
        for (int c = 0; c < BOARD_COLS; c++) {
            if (board[r][c] == 0) {
                board[r][c] = (rand() % 6) + 1;
            }
        }
    }
}

static void Game_ApplyGravityInternal(void) {
    for (int c = 0; c < BOARD_COLS; c++) {
        int write_row = BOARD_ROWS - 1;
        for (int read_row = BOARD_ROWS - 1; read_row >= 0; read_row--) {
            if (board[read_row][c] != 0) {
                board[write_row][c] = board[read_row][c];
                if (write_row != read_row) board[read_row][c] = 0;
                write_row--;
            }
        }
    }
}

static int Game_CheckAndRemoveMatches(void) {
    uint8_t marked[BOARD_ROWS][BOARD_COLS] = {0};
    int found_match = 0;

    // Перевірка "3 в ряд" (Горизонталь)
    for (int r = 0; r < BOARD_ROWS; r++) {
        for (int c = 0; c <= BOARD_COLS - 3; c++) {
            uint8_t color = board[r][c];
            if (color == 0) continue;
            if (board[r][c+1] == color && board[r][c+2] == color) {
                found_match = 1;
                marked[r][c] = marked[r][c+1] = marked[r][c+2] = 1;
            }
        }
    }

    // Перевірка "3 в ряд" (Вертикаль)
    for (int c = 0; c < BOARD_COLS; c++) {
        for (int r = 0; r <= BOARD_ROWS - 3; r++) {
            uint8_t color = board[r][c];
            if (color == 0) continue;
            if (board[r+1][c] == color && board[r+2][c] == color) {
                found_match = 1;
                marked[r][c] = marked[r+1][c] = marked[r+2][c] = 1;
            }
        }
    }

    if (found_match) {
        uint8_t count = 0;
        for (int r = 0; r < BOARD_ROWS; r++) {
            for (int c = 0; c < BOARD_COLS; c++) {
                if (marked[r][c]) {
                    board[r][c] = 0;
                    count++;
                }
            }
        }
        score += GetScoreForCount(count);
    }
    return found_match;
}

static uint32_t GetScoreForCount(uint8_t count) {
    if (count == 3) return 30;         // Базовий збіг
    if (count == 4) return 60;         // Бонус х2 за складність
    if (count >= 5) return 100;        // Супер-бонус
    return count * 10;                 // Резерв
}

static uint8_t GetValidRandomColor(int r, int c) {
    uint8_t color;
    int is_valid;

    do {
        color = (rand() % 6) + 1;
        is_valid = 1;

        if (c >= 2 && board[r][c-1] == color && board[r][c-2] == color) {
            is_valid = 0;
        }

        if (r >= 2 && board[r-1][c] == color && board[r-2][c] == color) {
            is_valid = 0;
        }

    } while (!is_valid);

    return color;
}

// НОВА ПРИВАТНА ФУНКЦІЯ: Тільки перевіряє наявність лінії, без видалення
static int Game_IsMatchPresent(void) {
    // Горизонталь
    for (int r = 0; r < BOARD_ROWS; r++) {
        for (int c = 0; c <= BOARD_COLS - 3; c++) {
            uint8_t color = board[r][c];
            if (color == 0) continue;
            if (board[r][c+1] == color && board[r][c+2] == color) {
                return 1;
            }
        }
    }
    // Вертикаль
    for (int c = 0; c < BOARD_COLS; c++) {
        for (int r = 0; r <= BOARD_ROWS - 3; r++) {
            uint8_t color = board[r][c];
            if (color == 0) continue;
            if (board[r+1][c] == color && board[r+2][c] == color) {
                return 1;
            }
        }
    }
    return 0;
}
