#include "save.h"
#include "stm32f0xx_hal.h" // Для роботи з Flash пам'яттю
#include <string.h>

// "Експортуємо" змінні з game.c, щоб save.c міг їх прочитати та записати
extern uint8_t board[BOARD_ROWS][BOARD_COLS];
extern uint32_t score;
extern char playerName[16];

void Save_Game(void) {
    GameSaveData_t dataToSave;

    // 1. Пакуємо поточний стан гри
    dataToSave.magic = SAVE_MAGIC_NUMBER;
    dataToSave.score = score;
    strncpy(dataToSave.playerName, playerName, 16);
    memcpy(dataToSave.board, board, sizeof(board));

    // 2. Робота з Flash
    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t PageError;
    EraseInitStruct.TypeErase   = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.PageAddress = FLASH_SAVE_ADDR;
    EraseInitStruct.NbPages     = 1;

    if (HAL_FLASHEx_Erase(&EraseInitStruct, &PageError) == HAL_OK) {
        uint32_t *pSource = (uint32_t *)&dataToSave;
        uint32_t *pDest   = (uint32_t *)FLASH_SAVE_ADDR;
        uint32_t wordsToWrite = sizeof(GameSaveData_t) / 4;

        for (uint32_t i = 0; i < wordsToWrite; i++) {
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)pDest, *pSource);
            pDest++;
            pSource++;
        }
    }

    HAL_FLASH_Lock();
}

int Load_Game(void) {
    GameSaveData_t *pSavedData = (GameSaveData_t *)FLASH_SAVE_ADDR;

    if (pSavedData->magic == SAVE_MAGIC_NUMBER) {
        // Відновлюємо дані у глобальні змінні гри
        score = pSavedData->score;
        strncpy(playerName, pSavedData->playerName, 16);
        memcpy(board, pSavedData->board, sizeof(board));
        return 1; // Успішно
    }
    return 0; // Збереження немає
}
