#include "save.h"
#include "stm32f0xx_hal.h"
#include <string.h>

extern uint8_t  board[BOARD_ROWS][BOARD_COLS];
extern uint32_t score;

char current_player_name[16] = "Player1";

/* =========================================================
 * ВНУТРІШНІ УТИЛІТИ
 * ========================================================= */

/**
 * Стирає одну сторінку Flash і записує блок даних з адреси data.
 * size_in_bytes має бути кратним 4.
 */
static HAL_StatusTypeDef Flash_Write_Page(uint32_t address,
                                          const uint32_t *data,
                                          uint32_t size_in_bytes)
{
    HAL_StatusTypeDef status;
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t page_error = 0;

    HAL_FLASH_Unlock();

    erase.TypeErase   = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = address;
    erase.NbPages     = 1;

    status = HAL_FLASHEx_Erase(&erase, &page_error);
    if (status != HAL_OK) {
        HAL_FLASH_Lock();
        return status;
    }

    uint32_t words = size_in_bytes / 4;
    for (uint32_t i = 0; i < words; i++) {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                                   address + i * 4,
                                   data[i]);
        if (status != HAL_OK) break;
    }

    HAL_FLASH_Lock();
    return status;
}

/* =========================================================
 * WEAR LEVELING ДЛЯ ТАБЛИЦІ ЛІДЕРІВ
 *
 * Сторінка 0x0800F800 розбита на LB_SLOTS_PER_PAGE комірок.
 * Кожна комірка — одна Leaderboard_t (104 байти).
 * Записи йдуть послідовно: 0, 1, 2, … 8.
 * Активний запис — останній з валідним magic.
 * Коли всі комірки заповнені — стираємо сторінку і починаємо з 0.
 *
 * Ресурс: 10 000 стирань × 9 записів = ~90 000 оновлень лідерборду.
 * ========================================================= */

/**
 * Повертає індекс (0..LB_SLOTS_PER_PAGE-1) останнього валідного запису.
 * Повертає -1 якщо сторінка порожня (після стирання).
 */
static int LB_Find_Active_Slot(void)
{
    const Leaderboard_t *page = (const Leaderboard_t *)FLASH_LEADERBOARD_ADDR;
    int last_valid = -1;

    for (int i = 0; i < (int)LB_SLOTS_PER_PAGE; i++) {
        if (page[i].magic == SAVE_MAGIC_NUMBER) {
            last_valid = i;
        }
    }
    return last_valid;
}

/**
 * Записує новий знімок Leaderboard_t в наступну вільну комірку.
 * Якщо місця немає — стирає сторінку і пише в комірку 0.
 */
static void LB_Write_Next(const Leaderboard_t *lb)
{
    int active = LB_Find_Active_Slot();
    int next   = active + 1;   /* наступна вільна комірка */

    if (next >= (int)LB_SLOTS_PER_PAGE) {
        /*
         * Сторінка повна — стираємо і пишемо весь блок з початку.
         * Одне стирання = один цикл знос.
         */
        Flash_Write_Page(FLASH_LEADERBOARD_ADDR,
                         (const uint32_t *)lb,
                         sizeof(Leaderboard_t));
        return;
    }

    /*
     * Місце є — пишемо тільки в наступну комірку без стирання сторінки.
     * Стирання НЕ відбувається → знос сторінки НЕ збільшується.
     */
    uint32_t target_addr = FLASH_LEADERBOARD_ADDR
                           + (uint32_t)next * sizeof(Leaderboard_t);

    HAL_FLASH_Unlock();
    uint32_t words = sizeof(Leaderboard_t) / 4;
    const uint32_t *src = (const uint32_t *)lb;
    for (uint32_t i = 0; i < words; i++) {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                          target_addr + i * 4,
                          src[i]);
    }
    HAL_FLASH_Lock();
}

/* =========================================================
 * ПУБЛІЧНІ ФУНКЦІЇ — ТАБЛИЦЯ ЛІДЕРІВ
 * ========================================================= */

void Get_Leaderboard(Leaderboard_t *dest)
{
    int active = LB_Find_Active_Slot();

    if (active >= 0) {
        const Leaderboard_t *page = (const Leaderboard_t *)FLASH_LEADERBOARD_ADDR;
        memcpy(dest, &page[active], sizeof(Leaderboard_t));
    } else {
        /* Сторінка порожня — повертаємо пустий лідерборд */
        memset(dest, 0, sizeof(Leaderboard_t));
        dest->magic = SAVE_MAGIC_NUMBER;
    }
}

void Update_Leaderboard(uint32_t final_score, const char *name)
{
    Leaderboard_t lb;
    Get_Leaderboard(&lb);

    /* Шукаємо місце для вставки (сортування за спаданням) */
    int insert_idx = -1;
    for (int i = 0; i < MAX_LEADERS; i++) {
        if (final_score > lb.leaders[i].score) {
            insert_idx = i;
            break;
        }
    }

    if (insert_idx == -1) return; /* Рахунок не потрапляє в топ — нічого не пишемо */

    /* Зсув записів вниз */
    for (int i = MAX_LEADERS - 1; i > insert_idx; i--) {
        lb.leaders[i] = lb.leaders[i - 1];
    }

    /* Вставка нового рекорду */
    lb.leaders[insert_idx].score = final_score;
    memset(lb.leaders[insert_idx].playerName, 0, 16);
    strncpy(lb.leaders[insert_idx].playerName, name, 15);
    lb.magic = SAVE_MAGIC_NUMBER;

    /* Wear-leveling запис — стирання тільки коли сторінка повна */
    LB_Write_Next(&lb);
}

/* =========================================================
 * ПУБЛІЧНІ ФУНКЦІЇ — ІГРОВІ СЛОТИ
 * (логіка не змінена, залишена як є)
 * ========================================================= */

void Save_Game(uint8_t slot)
{
    if (slot >= MAX_SAVE_SLOTS) return;

    GameSaveData_t all_slots[MAX_SAVE_SLOTS];
    const GameSaveData_t *flash_ptr = (const GameSaveData_t *)FLASH_SAVE_ADDR;

    memcpy(all_slots, flash_ptr, sizeof(all_slots));

    all_slots[slot].magic = SAVE_MAGIC_NUMBER;
    all_slots[slot].score = score;
    memset(all_slots[slot].playerName, 0, 16);
    strncpy(all_slots[slot].playerName, current_player_name, 15);
    memcpy(all_slots[slot].board, board, sizeof(board));

    Flash_Write_Page(FLASH_SAVE_ADDR,
                     (const uint32_t *)all_slots,
                     sizeof(all_slots));
}

int Load_Game(uint8_t slot)
{
    if (slot >= MAX_SAVE_SLOTS) return 0;

    const GameSaveData_t *flash_ptr = (const GameSaveData_t *)FLASH_SAVE_ADDR;

    if (flash_ptr[slot].magic != SAVE_MAGIC_NUMBER) return 0;

    score = flash_ptr[slot].score;
    memset(current_player_name, 0, 16);
    strncpy(current_player_name, flash_ptr[slot].playerName, 15);
    memcpy(board, flash_ptr[slot].board, sizeof(board));
    return 1;
}
