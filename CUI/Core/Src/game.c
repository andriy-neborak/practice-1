#include "game.h"
#include <stdlib.h> // Для rand() та abs()
#include <string.h> // Для memset()

// --- ГЛОБАЛЬНІ ЗМІННІ (оголошені в game.h) ---
uint8_t board[BOARD_ROWS][BOARD_COLS];
uint32_t score = 0;

/* --- ПРОТОТИПИ ВНУТРІШНІХ ФУНКЦІЙ (static) --- */
// ЗВЕРНИ УВАГУ: Тепер ця функція приймає координати (row, col)
static uint8_t GetValidRandomColor(int r, int c);

static void Game_ApplyGravityInternal(void);
static int Game_CheckAndRemoveMatches(void);
static uint32_t GetScoreForCount(uint8_t count);
static void Game_RunGravityLoop(void);
static void Game_FillEmptyCells(void);

/* ==========================================
   ПУБЛІЧНІ ФУНКЦІЇ (Викликаються з main.c)
   ========================================== */

// Ініціалізація гри
void Game_Init(void) {
    score = 0;
    // Очищаємо все поле нулями
    memset(board, 0, sizeof(board));
    // Заповнюємо поле (тепер з перевіркою на 3 в ряд)
    Game_FillEmptyCells();
}

// Функція обміну кульок
uint8_t Game_Swap(uint8_t r1, uint8_t c1, uint8_t r2, uint8_t c2) {
    // 1. Перевірка меж масиву
    if (r1 >= BOARD_ROWS || c1 >= BOARD_COLS || r2 >= BOARD_ROWS || c2 >= BOARD_COLS) {
        return 0;
    }

    // 2. Перевірка: чи є клітинки сусідніми?
    // Сума різниць координат має дорівнювати 1 (або по вертикалі, або по горизонталі)
    int diff_r = abs((int)r1 - (int)r2);
    int diff_c = abs((int)c1 - (int)c2);

    if ((diff_r + diff_c) != 1) {
        return 0; // Не сусідні
    }

    // 3. Тимчасовий обмін
    uint8_t temp = board[r1][c1];
    board[r1][c1] = board[r2][c2];
    board[r2][c2] = temp;

    // 4. Перевірка: чи склалася лінія?
    if (Game_CheckAndRemoveMatches()) {
        // Якщо так - запускаємо "гравітацію" (падіння нових кульок)
        Game_RunGravityLoop();
        return 1; // Успішний хід
    } else {
        // 5. Якщо лінії немає - повертаємо все назад (скасування ходу)
        board[r2][c2] = board[r1][c1];
        board[r1][c1] = temp;
        return 0; // Хід не зараховано
    }
}

/* ==========================================
   ПРИВАТНІ ФУНКЦІЇ (Логіка)
   ========================================== */

// Головний цикл оновлення поля (видалення -> падіння -> заповнення -> знову видалення)
static void Game_RunGravityLoop(void) {
    int matches_found;
    do {
        Game_FillEmptyCells(); // Досипаємо кульки
        matches_found = Game_CheckAndRemoveMatches(); // Шукаємо нові лінії
    } while (matches_found); // Повторюємо, поки знаходяться лінії
}

// Заповнення порожніх клітинок (де 0)
static void Game_FillEmptyCells(void) {
    // Спочатку "струшуємо" всі кульки вниз (гравітація)
    Game_ApplyGravityInternal();

    // Тепер заповнюємо дірки зверху
    for (int i = 0; i < BOARD_ROWS; i++) {
        for (int j = 0; j < BOARD_COLS; j++) {
            if (board[i][j] == 0) {
                // ТУТ ГОЛОВНА ЗМІНА: передаємо координати i, j
                board[i][j] = GetValidRandomColor(i, j);
            }
        }
    }
}

// Реалізація гравітації (зсув чисел вниз)
static void Game_ApplyGravityInternal(void) {
    for (int j = 0; j < BOARD_COLS; j++) {
        int write_row = BOARD_ROWS - 1; // Куди записувати (починаємо знизу)

        // Йдемо знизу вверх
        for (int read_row = BOARD_ROWS - 1; read_row >= 0; read_row--) {
            if (board[read_row][j] != 0) {
                // Якщо знайшли кульку - переміщаємо її вниз
                board[write_row][j] = board[read_row][j];

                // Якщо ми перемістили кульку, старе місце зануляємо
                if (write_row != read_row) {
                    board[read_row][j] = 0;
                }
                write_row--; // Піднімаємо вказівник запису
            }
        }
        // Решту зверху заповнювати нулями не треба, вони й так стануть нулями
    }
}

// Пошук ліній з 4-х кульок
static int Game_CheckAndRemoveMatches(void) {
    uint8_t marked[BOARD_ROWS][BOARD_COLS]; // Масив для позначок "що видаляти"
    memset(marked, 0, sizeof(marked));
    int found_match = 0;

    // 1. Перевірка Горизонталі (шукаємо 4 підряд)
    for (int i = 0; i < BOARD_ROWS; i++) {
        for (int j = 0; j <= BOARD_COLS - 4; j++) {
            uint8_t color = board[i][j];
            if (color == 0) continue; // Пусті не рахуємо

            if (board[i][j+1] == color &&
                board[i][j+2] == color &&
                board[i][j+3] == color) {

                found_match = 1;
                // Позначаємо всі 4 кульки на видалення
                marked[i][j] = 1;
                marked[i][j+1] = 1;
                marked[i][j+2] = 1;
                marked[i][j+3] = 1;
            }
        }
    }

    // 2. Перевірка Вертикалі (шукаємо 4 підряд)
    for (int j = 0; j < BOARD_COLS; j++) {
        for (int i = 0; i <= BOARD_ROWS - 4; i++) {
            uint8_t color = board[i][j];
            if (color == 0) continue;

            if (board[i+1][j] == color &&
                board[i+2][j] == color &&
                board[i+3][j] == color) {

                found_match = 1;
                marked[i][j] = 1;
                marked[i+1][j] = 1;
                marked[i+2][j] = 1;
                marked[i+3][j] = 1;
            }
        }
    }

    // 3. Видалення позначених кульок і нарахування балів
    if (found_match) {
        uint8_t count = 0;
        for (int i = 0; i < BOARD_ROWS; i++) {
            for (int j = 0; j < BOARD_COLS; j++) {
                if (marked[i][j]) {
                    board[i][j] = 0; // Видаляємо (робимо 0)
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

// --- НОВА ФУНКЦІЯ ГЕНЕРАЦІЇ ---
// Генерує колір, який НЕ створює лінію з 3-х однакових
static uint8_t GetValidRandomColor(int r, int c) {
    uint8_t color;
    int isValid = 0;

    // Крутимо цикл, поки не знайдемо "безпечний" колір
    while (isValid == 0) {
        color = (rand() % 6) + 1; // Випадкове число 1..6
        isValid = 1; // Припускаємо, що колір хороший

        // Перевірка ЗЛІВА: чи є вже дві такі самі кульки зліва?
        // Перевіряємо тільки якщо ми в 3-му стовпці або далі (c >= 2)
        if (c >= 2) {
            if (board[r][c-1] == color && board[r][c-2] == color) {
                isValid = 0; // Поганий колір (буде 3 в ряд), пробуємо інший
            }
        }

        // Перевірка ЗВЕРХУ: чи є вже дві такі самі кульки зверху?
        // Перевіряємо тільки якщо ми в 3-му рядку або нижче (r >= 2)
        if (isValid == 1 && r >= 2) {
            if (board[r-1][c] == color && board[r-2][c] == color) {
                isValid = 0; // Поганий колір (буде 3 в стовпчик)
            }
        }
    }
    return color;
}
