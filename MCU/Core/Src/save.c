#include "save.h"
#include "stm32f0xx_hal.h"
#include <string.h>

extern uint8_t board[BOARD_ROWS][BOARD_COLS];
extern uint32_t score;

char current_player_name[16] = "Player1";

/* --- Внутрішня функція для запису даних у Flash --- */
static void Flash_Write_Page(uint32_t address, uint32_t *data, uint32_t size_in_bytes) {
    HAL_FLASH_Unlock();

    // Очищення сторінки
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t PageError;
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.PageAddress = address;
    EraseInitStruct.NbPages = 1;

    if (HAL_FLASHEx_Erase(&EraseInitStruct, &PageError) == HAL_OK) {
        // Запис по 4 байти (Word)
        uint32_t words_to_write = size_in_bytes / 4;
        for (uint32_t i = 0; i < words_to_write; i++) {
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address + (i * 4), data[i]);
        }
    }

    HAL_FLASH_Lock();
}

/* --- Логіка збереження гри (Слоти) --- */

void Save_Game(uint8_t slot) {
    if (slot >= MAX_SAVE_SLOTS) return;

    GameSaveData_t all_slots[MAX_SAVE_SLOTS];
    GameSaveData_t *flash_ptr = (GameSaveData_t *)FLASH_SAVE_ADDR;

    // 1. Читаємо поточні дані з Flash
    memcpy(all_slots, flash_ptr, sizeof(all_slots));

    // 2. Оновлюємо конкретний слот
    all_slots[slot].magic = SAVE_MAGIC_NUMBER;
    all_slots[slot].score = score;
    memset(all_slots[slot].playerName, 0, 16);
    strncpy(all_slots[slot].playerName, current_player_name, 15);
    memcpy(all_slots[slot].board, board, sizeof(board));

    // 3. Записуємо оновлений масив назад
    Flash_Write_Page(FLASH_SAVE_ADDR, (uint32_t *)all_slots, sizeof(all_slots));
}

int Load_Game(uint8_t slot) {
    if (slot >= MAX_SAVE_SLOTS) return 0;

    GameSaveData_t *flashData = (GameSaveData_t *)FLASH_SAVE_ADDR;

    if (flashData[slot].magic == SAVE_MAGIC_NUMBER) {
        score = flashData[slot].score;
        memset(current_player_name, 0, 16);
        strncpy(current_player_name, flashData[slot].playerName, 15);
        memcpy(board, flashData[slot].board, sizeof(board));
        return 1;
    }
    return 0;
}

/* --- Логіка таблиці лідерів --- */

void Get_Leaderboard(Leaderboard_t* dest) {
    Leaderboard_t *flash_leaders = (Leaderboard_t *)FLASH_LEADERBOARD_ADDR;

    if (flash_leaders->magic == SAVE_MAGIC_NUMBER) {
        memcpy(dest, flash_leaders, sizeof(Leaderboard_t));
    } else {
        // Якщо даних немає, створюємо пусту структуру
        memset(dest, 0, sizeof(Leaderboard_t));
        dest->magic = SAVE_MAGIC_NUMBER;
    }
}

void Update_Leaderboard(uint32_t final_score, const char* name) {
    Leaderboard_t current_ld;
    Get_Leaderboard(&current_ld);

    int insert_idx = -1;

    // Пошук місця для вставки (сортування за спаданням)
    for (int i = 0; i < MAX_LEADERS; i++) {
        if (final_score > current_ld.leaders[i].score) {
            insert_idx = i;
            break;
        }
    }

    if (insert_idx != -1) {
        // Зсув результатів вниз
        for (int i = MAX_LEADERS - 1; i > insert_idx; i--) {
            current_ld.leaders[i] = current_ld.leaders[i - 1];
        }

        // Вставка нового рекорду
        current_ld.leaders[insert_idx].score = final_score;
        memset(current_ld.leaders[insert_idx].playerName, 0, 16);
        strncpy(current_ld.leaders[insert_idx].playerName, name, 15);

        // Запис у Flash
        Flash_Write_Page(FLASH_LEADERBOARD_ADDR, (uint32_t *)&current_ld, sizeof(Leaderboard_t));
    }
}
