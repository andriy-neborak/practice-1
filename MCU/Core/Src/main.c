/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body (Protocol Handler with Leaderboard & Finish)
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usart.h"
#include "gpio.h"

/* USER CODE BEGIN Includes */
#include <string.h>
#include "game.h"
#include "save.h"
#include "protocol.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
    uint8_t cmd;
    uint8_t addr_h;
    uint8_t addr_l;
    uint8_t data_h;
    uint8_t data_l;
    uint8_t crc;
} Packet_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define PACKET_SIZE  6
#define TIMEOUT_MS   10
/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* --- UART прийом --- */
static uint8_t rx_byte;                   /* буфер ISR для одного байту        */
static uint8_t rx_buffer[PACKET_SIZE];    /* буфер накопичення в ISR            */
static uint8_t rx_idx = 0;

/*
 * ВИПРАВЛЕННЯ #1 (Race condition):
 * Готовий пакет копіюється в окремий буфер packet_buf ВСЕРЕДИНІ ISR,
 * після чого піднімається прапор. Main loop читає тільки packet_buf —
 * ISR більше не може затерти дані під час обробки.
 */
static volatile uint8_t packet_received_flag = 0;
static uint8_t packet_buf[PACKET_SIZE];   /* захищена копія для main loop       */
static uint32_t last_byte_tick = 0;

static Packet_t current_packet;

extern char current_player_name[16];
static uint8_t board_snapshot[BOARD_ROWS][BOARD_COLS];
static uint32_t anim_speed_ms = 150;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USER CODE BEGIN 0 */

/* =========================================================
 * CRC
 * ========================================================= */
static uint8_t CRC8_Calc(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0x00;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : (crc << 1);
        }
    }
    return crc;
}

/* =========================================================
 * Відправка пакетів
 * ========================================================= */
static void Send_Packet(uint8_t cmd, uint8_t b1, uint8_t b2,
                        uint8_t b3, uint8_t b4)
{
    uint8_t tx[PACKET_SIZE];
    tx[0] = cmd;
    tx[1] = b1;
    tx[2] = b2;
    tx[3] = b3;
    tx[4] = b4;
    tx[5] = CRC8_Calc(tx, 5);
    HAL_UART_Transmit(&huart1, tx, PACKET_SIZE, 100);
}

/* =========================================================
 * Оновлення UI
 * ========================================================= */
static void Send_Board_Diff(void)
{
    for (uint8_t r = 0; r < BOARD_ROWS; r++) {
        for (uint8_t c = 0; c < BOARD_COLS; c++) {
            if (board[r][c] != board_snapshot[r][c]) {
                Send_Packet(CMD_UPDATE_CELL, r, c, board[r][c], STATUS_OK);
                HAL_Delay(2);
            }
        }
    }
}

void UI_Update_Step(void)
{
    Send_Board_Diff();
    memcpy(board_snapshot, board, sizeof(board_snapshot));
    if (anim_speed_ms > 0) HAL_Delay(anim_speed_ms);
}

static void Send_Full_Board(void)
{
    for (uint8_t r = 0; r < BOARD_ROWS; r++) {
        for (uint8_t c = 0; c < BOARD_COLS; c++) {
            Send_Packet(CMD_UPDATE_CELL, r, c, board[r][c], STATUS_OK);
            HAL_Delay(2);
        }
    }
}

/* USER CODE END 0 */

/* =========================================================
 * MAIN
 * ========================================================= */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();

    /* USER CODE BEGIN 2 */
    __HAL_UART_FLUSH_DRREGISTER(&huart1);
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
    Game_Init();
    /* USER CODE END 2 */

    while (1)
    {
        if (packet_received_flag == 0) continue;

        /*
         * ВИПРАВЛЕННЯ #1:
         * Читаємо з packet_buf — захищеної копії, а не з rx_buffer,
         * який ISR може змінити в будь-який момент.
         */
        current_packet.cmd    = packet_buf[0];
        current_packet.addr_h = packet_buf[1];
        current_packet.addr_l = packet_buf[2];
        current_packet.data_h = packet_buf[3];
        current_packet.data_l = packet_buf[4];
        current_packet.crc    = packet_buf[5];

        if (CRC8_Calc(packet_buf, 5) != current_packet.crc) {
            /* CRC не збігається — ігноруємо пакет */
            packet_received_flag = 0;
            continue;
        }

        switch (current_packet.cmd)
        {
            /* --------------------------------------------------
             * 0x10 — НОВА ГРА
             * -------------------------------------------------- */
            case CMD_NEW_GAME:
                Game_Init();
                memcpy(board_snapshot, board, sizeof(board_snapshot));
                Send_Packet(CMD_NEW_GAME, 0, 0, 0, STATUS_OK);
                Send_Full_Board();
                break;

            /* --------------------------------------------------
             * 0x11 — ХІД (SWAP)
             * -------------------------------------------------- */
            case CMD_SWAP:
            {
                memcpy(board_snapshot, board, sizeof(board_snapshot));

                uint8_t ok = Game_Swap(current_packet.addr_h,
                                       current_packet.addr_l,
                                       current_packet.data_h,
                                       current_packet.data_l);
                if (ok) {
                    Send_Packet(CMD_SWAP, 0, 0, 0, STATUS_OK);
                    UI_Update_Step();
                    Game_RunGravityLoop();

                    if (Game_HasPossibleMoves() == 0) {
                        /*
                         * Немає ходів — гра завершена автоматично.
                         * Записуємо лідерборд і повідомляємо Python.
                         */
                        Update_Leaderboard(score, current_player_name);
                        Send_Packet(CMD_SWAP, 0, 0, 0, STATUS_GAME_OVER);
                    }
                } else {
                    Send_Packet(CMD_SWAP, 0, 0, 0, STATUS_ERROR);
                }
            }
            break;

            /* --------------------------------------------------
             * 0x12 — ПРИМУСОВЕ ЗАВЕРШЕННЯ (кнопка "Finish")
             * -------------------------------------------------- */
            case CMD_FINISH:
            	uint8_t slot = current_packet.addr_h;
            	Save_Game(slot);
            	Update_Leaderboard(score, current_player_name);
                Game_Init();
                memcpy(board_snapshot, board, sizeof(board_snapshot));
                /*
                 * ВИПРАВЛЕННЯ #2:
                 * Надсилаємо STATUS_GAME_OVER замість STATUS_OK,
                 * щоб Python розумів що гра завершена (як і в 0x11).
                 */
                Send_Packet(CMD_FINISH, 0, 0, 0, STATUS_GAME_OVER);
                Send_Full_Board();
                break;

            /* --------------------------------------------------
             * 0x15 — ОТРИМАТИ SCORE
             * -------------------------------------------------- */
            case CMD_GET_SCORE:
            {
                uint8_t tx[PACKET_SIZE];
                tx[0] = CMD_GET_SCORE;
                tx[1] = (uint8_t)((score >> 24) & 0xFF);
                tx[2] = (uint8_t)((score >> 16) & 0xFF);
                tx[3] = (uint8_t)((score >>  8) & 0xFF);
                tx[4] = (uint8_t)( score        & 0xFF);
                tx[5] = CRC8_Calc(tx, 5);
                HAL_UART_Transmit(&huart1, tx, PACKET_SIZE, 100);
            }
            break;

            /* --------------------------------------------------
             * 0x20 — ПРИЙНЯТИ ІМ'Я (по 3 символи на пакет)
             * -------------------------------------------------- */
            case CMD_SET_NAME:
            {
                uint8_t chunk = current_packet.addr_h;
                if (chunk < 6) {
                    int base = chunk * 3;
                    /*
                     * ВИПРАВЛЕННЯ #3:
                     * Приймаємо тільки друковані ASCII символи (0x20–0x7E).
                     * Решта замінюється на '?' — захист від сміттєвих байтів.
                     */
                    uint8_t bytes[3] = {
                        current_packet.addr_l,
                        current_packet.data_h,
                        current_packet.data_l
                    };
                    for (int k = 0; k < 3; k++) {
                        if (base + k < 15) {
                            uint8_t ch = bytes[k];
                            current_player_name[base + k] =
                                (ch >= 0x20 && ch <= 0x7E) ? (char)ch : '?';
                        }
                    }
                }
                if (chunk == 5) current_player_name[15] = '\0';
                Send_Packet(CMD_SET_NAME, chunk, 0, 0, STATUS_OK);
            }
            break;

            /* --------------------------------------------------
             * 0x30 — ЗБЕРЕГТИ СТАН ГРИ
             * -------------------------------------------------- */
            case CMD_SAVE:
                Save_Game(current_packet.addr_h);
                Send_Packet(CMD_SAVE, current_packet.addr_h, 0, 0, STATUS_OK);
                break;

            /* --------------------------------------------------
             * 0x31 — ЗАВАНТАЖИТИ СТАН ГРИ
             * -------------------------------------------------- */
            case CMD_LOAD:
                if (Load_Game(current_packet.addr_h)) {
                    memcpy(board_snapshot, board, sizeof(board_snapshot));
                    Send_Packet(CMD_LOAD, current_packet.addr_h, 0, 0, STATUS_OK);
                    HAL_Delay(10);
                    Send_Full_Board();
                } else {
                    Send_Packet(CMD_LOAD, current_packet.addr_h, 0, 0, STATUS_ERROR);
                }
                break;

            /* --------------------------------------------------
             * 0x32 — ЗАПИТАТИ НІКНЕЙМ ЗІ СЛОТА
             * -------------------------------------------------- */
            case CMD_GET_SLOT_NAME:
            {
                uint8_t slot = current_packet.addr_h;
                if (slot < MAX_SAVE_SLOTS) {
                    const GameSaveData_t *fp = (const GameSaveData_t *)FLASH_SAVE_ADDR;
                    if (fp[slot].magic == SAVE_MAGIC_NUMBER) {
                        Send_Packet(0x33, slot, fp[slot].playerName[0],
                                                fp[slot].playerName[1],
                                                fp[slot].playerName[2]);
                        HAL_Delay(5);
                        Send_Packet(0x34, slot, fp[slot].playerName[3],
                                                fp[slot].playerName[4],
                                                fp[slot].playerName[5]);
                        HAL_Delay(5);
                        Send_Packet(0x35, slot, fp[slot].playerName[6],
                                                fp[slot].playerName[7],
                                                fp[slot].playerName[8]);
                        HAL_Delay(5);
                        Send_Packet(0x36, slot, fp[slot].playerName[9],
                                                fp[slot].playerName[10],
                                                fp[slot].playerName[11]);
                        HAL_Delay(5);
                        Send_Packet(CMD_GET_SLOT_NAME, slot, 0, 0, STATUS_OK);
                    } else {
                        Send_Packet(CMD_GET_SLOT_NAME, slot, 0, 0, STATUS_ERROR);
                    }
                }
            }
            break;

            /* --------------------------------------------------
             * 0x40 — ОТРИМАТИ ТАБЛИЦЮ ЛІДЕРІВ
             * -------------------------------------------------- */
            case CMD_GET_LEADERBOARD:
            {
                Leaderboard_t lb;
                Get_Leaderboard(&lb);

                for (uint8_t i = 0; i < MAX_LEADERS; i++) {
                    Send_Packet(0x41, i, lb.leaders[i].playerName[0],
                                        lb.leaders[i].playerName[1],
                                        lb.leaders[i].playerName[2]);
                    HAL_Delay(5);
                    Send_Packet(0x43, i, lb.leaders[i].playerName[3],
                                        lb.leaders[i].playerName[4],
                                        lb.leaders[i].playerName[5]);
                    HAL_Delay(5);
                    Send_Packet(0x44, i, lb.leaders[i].playerName[6],
                                        lb.leaders[i].playerName[7],
                                        lb.leaders[i].playerName[8]);
                    HAL_Delay(5);
                    Send_Packet(0x45, i, lb.leaders[i].playerName[9],
                                        0x00, 0x00);
                    HAL_Delay(5);

                    uint8_t s_h = (uint8_t)((lb.leaders[i].score >> 8) & 0xFF);
                    uint8_t s_l = (uint8_t)( lb.leaders[i].score       & 0xFF);
                    Send_Packet(0x42, i, 0, s_h, s_l);
                    HAL_Delay(5);
                }
            }
            break;

            /* --------------------------------------------------
             * Невідома команда
             * -------------------------------------------------- */
            default:
                Send_Packet(current_packet.cmd, 0, 0, 0, STATUS_UNKNOWN);
                break;
        }

        packet_received_flag = 0;

    } /* while(1) */
}

/* =========================================================
 * SYSTEM CLOCK
 * ========================================================= */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLMUL          = RCC_PLL_MUL12;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType    = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                   | RCC_CLOCKTYPE_PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1);

    PeriphClkInit.PeriphClockSelection  = RCC_PERIPHCLK_USART1;
    PeriphClkInit.Usart1ClockSelection  = RCC_USART1CLKSOURCE_PCLK1;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);
}

/* =========================================================
 * UART CALLBACKS
 * ========================================================= */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1) return;

    uint32_t now = HAL_GetTick();

    /* Таймаут між байтами — скидаємо буфер */
    if (rx_idx > 0 && (now - last_byte_tick > TIMEOUT_MS)) {
        rx_idx = 0;
    }
    last_byte_tick = now;

    /*
     * ВИПРАВЛЕННЯ #1:
     * Приймаємо байт тільки якщо main loop вже обробив попередній пакет.
     * Коли пакет зібраний — копіюємо в packet_buf і піднімаємо прапор.
     */
    if (packet_received_flag == 0) {
        rx_buffer[rx_idx++] = rx_byte;
        if (rx_idx >= PACKET_SIZE) {
            memcpy(packet_buf, rx_buffer, PACKET_SIZE); /* атомарна копія */
            packet_received_flag = 1;
            rx_idx = 0;
        }
    }

    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1) return;
    __HAL_UART_CLEAR_OREFLAG(huart);
    rx_idx = 0;
    packet_received_flag = 0;
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}
