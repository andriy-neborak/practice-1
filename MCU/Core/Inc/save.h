#ifndef INC_SAVE_H_
#define INC_SAVE_H_

#include <stdint.h>
#include "game.h"

/* Константи адрес Flash-пам'яті */
#define FLASH_LEADERBOARD_ADDR 0x0800F800 // Сторінка для таблиці лідерів
#define FLASH_SAVE_ADDR        0x0800FC00 // Сторінка для ігрових слотів
#define SAVE_MAGIC_NUMBER      0xABBA1234
#define MAX_SAVE_SLOTS         3
#define MAX_LEADERS            5

/* Структури для збереження гри */
typedef struct {
    uint32_t magic;
    uint32_t score;
    char     playerName[16];
    uint8_t  board[BOARD_ROWS][BOARD_COLS];
} GameSaveData_t;

/* Структури для таблиці лідерів */
typedef struct {
    uint32_t score;
    char     playerName[16];
} LeaderRecord_t;

typedef struct {
    uint32_t magic;
    LeaderRecord_t leaders[MAX_LEADERS];
} Leaderboard_t;

/* Глобальні змінні */
extern char current_player_name[16];

/* Функції збереження/завантаження гри */
void Save_Game(uint8_t slot);
int  Load_Game(uint8_t slot);

/* Функції роботи з таблицею лідерів */
void Update_Leaderboard(uint32_t final_score, const char* name);
void Get_Leaderboard(Leaderboard_t* dest);

#endif /* INC_SAVE_H_ */
