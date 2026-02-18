#ifndef INC_GAME_H_
#define INC_GAME_H_

#include <stdint.h>

// Розміри поля
#define BOARD_ROWS 8
#define BOARD_COLS 8

// Глобальні змінні
extern uint8_t board[BOARD_ROWS][BOARD_COLS];
extern uint32_t score;

// Лише ці дві функції мають бути доступні для main.c
void Game_Init(void);
uint8_t Game_Swap(uint8_t r1, uint8_t c1, uint8_t r2, uint8_t c2);

#endif /* INC_GAME_H_ */
