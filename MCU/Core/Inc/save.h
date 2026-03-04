#ifndef INC_SAVE_H_
#define INC_SAVE_H_

#include <stdint.h>
#include "game.h"

/* =========================================================
 * Адреси Flash-сторінок (STM32F051R8, сторінка = 1024 байти)
 * ========================================================= */
#define FLASH_LEADERBOARD_ADDR  0x0800F800U   /* Сторінка 62 — таблиця лідерів */
#define FLASH_SAVE_ADDR         0x0800FC00U   /* Сторінка 63 — ігрові слоти    */

/* =========================================================
 * Магічні числа та константи
 * ========================================================= */
#define SAVE_MAGIC_NUMBER       0xABBA1234U
#define SLOT_EMPTY_MAGIC        0xFFFFFFFFU   /* Flash після стирання           */

#define MAX_SAVE_SLOTS          3
#define MAX_LEADERS             5

/* =========================================================
 * Wear Leveling для таблиці лідерів
 *
 * sizeof(Leaderboard_t) = 4 (magic) + 5*(4+16) = 104 байти
 * Записів на сторінку  = 1024 / 104 = 9
 * Ресурс: 10 000 * 9 = ~90 000 циклів замість 10 000
 * ========================================================= */
#define LB_SLOTS_PER_PAGE  (FLASH_PAGE_SIZE / sizeof(Leaderboard_t))

/* =========================================================
 * Структури
 * ========================================================= */
typedef struct {
    uint32_t magic;
    uint32_t score;
    char     playerName[16];
    uint8_t  board[BOARD_ROWS][BOARD_COLS];
} GameSaveData_t;

typedef struct {
    uint32_t score;
    char     playerName[16];
} LeaderRecord_t;

typedef struct {
    uint32_t       magic;
    LeaderRecord_t leaders[MAX_LEADERS];
} Leaderboard_t;

/* =========================================================
 * Глобальні змінні
 * ========================================================= */
extern char current_player_name[16];

/* =========================================================
 * API
 * ========================================================= */
void Save_Game(uint8_t slot);
int  Load_Game(uint8_t slot);

void Update_Leaderboard(uint32_t final_score, const char* name);
void Get_Leaderboard(Leaderboard_t* dest);

#endif /* INC_SAVE_H_ */
