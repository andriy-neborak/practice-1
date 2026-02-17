#ifndef __GAME_H
#define __GAME_H

#include "main.h"

// Розміри поля
#define BOARD_ROWS 8
#define BOARD_COLS 8

// Прототипи функцій (вони мають точно збігатися з тими, що у game.c)
void Game_Init(void);
void Game_FillEmptyCells(void);
void Game_ProcessStep(uint8_t column);
void Game_SendBoardUART(void);

#endif
