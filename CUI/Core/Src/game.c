#include "game.h"
#include "usart.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h> // Потрібно для abs(), але можна і без неї

// --- ЗМІННІ ---
uint8_t board[BOARD_ROWS][BOARD_COLS];
char debug_view[BOARD_ROWS][BOARD_COLS];
uint32_t score = 0;

// --- ПРОТОТИПИ ЛОКАЛЬНИХ ФУНКЦІЙ ---
static void Game_UpdateDebugView(void);
static uint8_t GetValidRandomColor(uint8_t row, uint8_t col);
static void Game_ApplyGravity(void);
static int Game_CheckAndRemoveMatches(void);
static uint32_t GetScoreForCount(uint8_t count);
static void Game_RunGravityLoop(void);

// --- ІНІЦІАЛІЗАЦІЯ ---
void Game_Init(void) {
    score = 0;
    memset(board, 0, sizeof(board));

    // Заповнюємо поле (початковий стан)
    Game_FillEmptyCells();
    Game_UpdateDebugView();
}

// --- ГОЛОВНА ФУНКЦІЯ ОБМІНУ (SWAP) ---
/**
  * @brief Спроба поміняти дві кульки місцями.
  * @param r1, c1 - координати першої кульки
  * @param r2, c2 - координати другої кульки
  */
void Game_Swap(uint8_t r1, uint8_t c1, uint8_t r2, uint8_t c2) {

    // 1. Перевірка меж масиву (щоб не вилізти за межі пам'яті)
    if (r1 >= BOARD_ROWS || c1 >= BOARD_COLS || r2 >= BOARD_ROWS || c2 >= BOARD_COLS) {
        return; // Помилка координат
    }

    // 2. Перевірка на сусідство (тільки по вертикалі або горизонталі)
    // abs() повертає модуль числа. Сума різниць координат має бути 1.
    int diff_r = (r1 > r2) ? (r1 - r2) : (r2 - r1);
    int diff_c = (c1 > c2) ? (c1 - c2) : (c2 - c1);

    if ((diff_r + diff_c) != 1) {
        HAL_UART_Transmit(&huart1, (uint8_t*)"\r\nError: Not neighbors!\r\n", 25, 100);
        return; // Не сусіди
    }

    // 3. Виконуємо обмін (Swap)
    uint8_t temp = board[r1][c1];
    board[r1][c1] = board[r2][c2];
    board[r2][c2] = temp;

    // 4. Перевіряємо, чи цей хід щось знищив
    int match_found = Game_CheckAndRemoveMatches();

    if (match_found) {
        // УРА! Хід успішний. Запускаємо ланцюгову реакцію.
        Game_RunGravityLoop();

        HAL_UART_Transmit(&huart1, (uint8_t*)"\r\nMove Successful!\r\n", 20, 100);
    } else {
        // Хід нічого не дав -> МІНЯЄМО НАЗАД (Revert)
        temp = board[r1][c1];
        board[r1][c1] = board[r2][c2];
        board[r2][c2] = temp;

        HAL_UART_Transmit(&huart1, (uint8_t*)"\r\nInvalid Move (No Match)\r\n", 27, 100);
    }

    // 5. Показуємо результат
    Game_SendBoardUART();
}


// --- ДОПОМІЖНІ ФУНКЦІЇ ---

/**
  * @brief Цикл: Гравітація -> Досипання -> Знищення (поки є що нищити)
  */
static void Game_RunGravityLoop(void) {
    int matches_found;
    do {
        // Спочатку падаємо і досипаємо
        Game_FillEmptyCells();

        // Потім знову перевіряємо, чи не вибухнуло щось нове
        matches_found = Game_CheckAndRemoveMatches();

        // Якщо matches_found == 1, цикл повториться
    } while (matches_found);

    Game_UpdateDebugView();
}

void Game_FillEmptyCells(void) {
    Game_ApplyGravity();

    for (int j = 0; j < BOARD_COLS; j++) {
        for (int i = 0; i < BOARD_ROWS; i++) {
            if (board[i][j] == 0) {
                board[i][j] = GetValidRandomColor(i, j);
            } else {
                break;
            }
        }
    }
}

static void Game_ApplyGravity(void) {
    for (int j = 0; j < BOARD_COLS; j++) {
        int write_row = BOARD_ROWS - 1;
        for (int read_row = BOARD_ROWS - 1; read_row >= 0; read_row--) {
            if (board[read_row][j] != 0) {
                board[write_row][j] = board[read_row][j];
                write_row--;
            }
        }
        while (write_row >= 0) {
            board[write_row][j] = 0;
            write_row--;
        }
    }
}

static int Game_CheckAndRemoveMatches(void) {
    uint8_t marked[BOARD_ROWS][BOARD_COLS];
    memset(marked, 0, sizeof(marked));
    int found_match = 0;

    // Горизонталь
    for (int i = 0; i < BOARD_ROWS; i++) {
        for (int j = 0; j <= BOARD_COLS - 4; j++) {
            uint8_t color = board[i][j];
            if (color == 0) continue;
            int count = 1;
            while ((j + count < BOARD_COLS) && (board[i][j + count] == color)) count++;
            if (count >= 4) {
                found_match = 1;
                score += GetScoreForCount(count);
                for (int k = 0; k < count; k++) marked[i][j + k] = 1;
            }
        }
    }

    // Вертикаль
    for (int j = 0; j < BOARD_COLS; j++) {
        for (int i = 0; i <= BOARD_ROWS - 4; i++) {
            uint8_t color = board[i][j];
            if (color == 0) continue;
            int count = 1;
            while ((i + count < BOARD_ROWS) && (board[i + count][j] == color)) count++;
            if (count >= 4) {
                found_match = 1;
                score += GetScoreForCount(count);
                for (int k = 0; k < count; k++) marked[i + k][j] = 1;
            }
        }
    }

    // Видалення
    if (found_match) {
        for (int i = 0; i < BOARD_ROWS; i++) {
            for (int j = 0; j < BOARD_COLS; j++) {
                if (marked[i][j]) board[i][j] = 0;
            }
        }
    }
    return found_match;
}

static uint32_t GetScoreForCount(uint8_t count) {
    if (count == 4) return 100;
    if (count == 5) return 250;
    if (count == 6) return 500;
    if (count == 7) return 1000;
    if (count >= 8) return 5000;
    return 0;
}

static uint8_t GetValidRandomColor(uint8_t row, uint8_t col) {
    return (rand() % 6) + 1;
}

static void Game_UpdateDebugView(void) {
    for (int i = 0; i < BOARD_ROWS; i++) {
        for (int j = 0; j < BOARD_COLS; j++) {
            debug_view[i][j] = board[i][j];
        }
    }
}

// Функція для сумісності (якщо ти ще використовуєш старий виклик)
void Game_ProcessStep(uint8_t column) {
    // Тут можна зробити логіку вибору координат курсором,
    // але поки що ця функція просто приклад.
    // Тобі треба викликати Game_Swap(r1, c1, r2, c2)
}

void Game_SendBoardUART(void) {
    char line_buffer[64];
    sprintf(line_buffer, "\r\n--- SCORE: %lu ---\r\n", score);
    HAL_UART_Transmit(&huart1, (uint8_t*)line_buffer, strlen(line_buffer), 100);

    // Додаємо номери стовпчиків зверху для зручності
    HAL_UART_Transmit(&huart1, (uint8_t*)"   0 1 2 3 4 5 6 7\r\n", 20, 100);

    for (int i = 0; i < BOARD_ROWS; i++) {
        sprintf(line_buffer, "%d: ", i); // Номер рядка зліва
        HAL_UART_Transmit(&huart1, (uint8_t*)line_buffer, strlen(line_buffer), 100);

        for (int j = 0; j < BOARD_COLS; j++) {
            uint8_t val = board[i][j];
            char symbol = (val == 0) ? '.' : (val + '0');
            line_buffer[j*2] = symbol;
            line_buffer[j*2 + 1] = ' ';
        }
        line_buffer[BOARD_COLS*2] = '\r';
        line_buffer[BOARD_COLS*2 + 1] = '\n';
        line_buffer[BOARD_COLS*2 + 2] = '\0';
        HAL_UART_Transmit(&huart1, (uint8_t*)line_buffer, strlen(line_buffer), 100);
    }
}
