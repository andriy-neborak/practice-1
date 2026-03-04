#ifndef INC_PROTOCOL_H_
#define INC_PROTOCOL_H_

/* =========================================================
 * Команди (CMD)
 * ========================================================= */
#define CMD_NEW_GAME        0x10
#define CMD_SWAP            0x11
#define CMD_FINISH          0x12
#define CMD_GET_SCORE       0x15
#define CMD_UPDATE_CELL     0x16
#define CMD_SET_NAME        0x20
#define CMD_SAVE            0x30
#define CMD_LOAD            0x31
#define CMD_GET_SLOT_NAME   0x32
#define CMD_GET_LEADERBOARD 0x40

/* =========================================================
 * Статуси відповіді (STATUS)
 * ========================================================= */
#define STATUS_OK           0xAA   /* Успіх                  */
#define STATUS_GAME_OVER    0xDD   /* Гра завершена          */
#define STATUS_ERROR        0xEE   /* Помилка / слот порожній */
#define STATUS_UNKNOWN      0xFF   /* Невідома команда       */

#endif /* INC_PROTOCOL_H_ */
