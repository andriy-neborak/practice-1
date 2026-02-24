#ifndef INC_SAVE_H_
#define INC_SAVE_H_

#include <stdint.h>
#include "game.h" // Підключаємо, щоб знати розміри поля BOARD_ROWS і BOARD_COLS

// --- Константи для Flash-пам'яті ---
#define FLASH_SAVE_ADDR     0x0800FC00
#define SAVE_MAGIC_NUMBER   0xABBA1234

// --- Структура нашого збереження ---
typedef struct {
    uint32_t magic;               // Маркер
    uint32_t score;               // Рахунок
    char     playerName[16];      // Ім'я гравця
    uint8_t  board[BOARD_ROWS][BOARD_COLS]; // Ігрове поле
} GameSaveData_t;

// --- Прототипи функцій ---
void Save_Game(void);
int  Load_Game(void);

#endif /* INC_SAVE_H_ */
