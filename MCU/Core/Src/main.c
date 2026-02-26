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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include "game.h"
#include "save.h"
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
#define PACKET_SIZE 6
#define TIMEOUT_MS  10
#define CMD_UPDATE_CELL 0x16
/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
uint8_t rx_byte;
uint8_t rx_buffer[PACKET_SIZE];
uint8_t rx_idx = 0;
volatile uint8_t packet_received_flag = 0;
uint32_t last_byte_tick = 0;
Packet_t current_packet;

extern char current_player_name[16];
uint8_t board_snapshot[BOARD_ROWS][BOARD_COLS];
uint32_t anim_speed_ms = 150;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USER CODE BEGIN 0 */
uint8_t CRC8_Calc(uint8_t *data, uint8_t len)
{
    uint8_t crc = 0x00;
    uint8_t i, j;
    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

void Send_Packet(uint8_t cmd, uint8_t r, uint8_t c, uint8_t data, uint8_t status)
{
    uint8_t tx_buf[PACKET_SIZE];
    tx_buf[0] = cmd;
    tx_buf[1] = r;
    tx_buf[2] = c;
    tx_buf[3] = data;
    tx_buf[4] = status;
    tx_buf[5] = CRC8_Calc(tx_buf, 5);
    HAL_UART_Transmit(&huart1, tx_buf, PACKET_SIZE, 100);
}

void Send_Board_Diff(void)
{
    for (uint8_t r = 0; r < BOARD_ROWS; r++) {
        for (uint8_t c = 0; c < BOARD_COLS; c++) {
            if (board[r][c] != board_snapshot[r][c]) {
                Send_Packet(CMD_UPDATE_CELL, r, c, board[r][c], 0xAA);
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

void Send_Full_Board(void)
{
    for (uint8_t r = 0; r < BOARD_ROWS; r++) {
        for (uint8_t c = 0; c < BOARD_COLS; c++) {
            Send_Packet(CMD_UPDATE_CELL, r, c, board[r][c], 0xAA);
            HAL_Delay(2);
        }
    }
}
/* USER CODE END 0 */

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
      if (packet_received_flag == 1)
      {
          current_packet.cmd    = rx_buffer[0];
          current_packet.addr_h = rx_buffer[1];
          current_packet.addr_l = rx_buffer[2];
          current_packet.data_h = rx_buffer[3];
          current_packet.data_l = rx_buffer[4];
          current_packet.crc    = rx_buffer[5];

          if (CRC8_Calc(rx_buffer, 5) == current_packet.crc)
          {
              switch (current_packet.cmd)
              {
                  case 0x10: // НОВА ГРА
                      Game_Init();
                      Send_Packet(0x10, 0, 0, 0, 0xAA);
                      Send_Full_Board();
                      break;

                  case 0x11: // ХІД (SWAP)
                  {
                      memcpy(board_snapshot, board, sizeof(board_snapshot));
                      uint8_t success = Game_Swap(current_packet.addr_h, current_packet.addr_l,
                                                  current_packet.data_h, current_packet.data_l);
                      if (success) {
                          Send_Packet(0x11, 0, 0, 0, 0xAA);
                          UI_Update_Step();
                          Game_RunGravityLoop();

                          // Перевірка на автоматичне завершення (немає ходів)
                          if (Game_HasPossibleMoves() == 0) {
                              Update_Leaderboard(score, current_player_name);
                              Send_Packet(0x11, 0, 0, 0, 0xDD); // Повідомлення Python про Game Over
                          }
                      } else {
                          Send_Packet(0x11, 0, 0, 0, 0xEE);
                      }
                  }
                  break;

                  case 0x12: // ПРИМУСОВЕ ЗАВЕРШЕННЯ (Кнопка "Finish")
                  {
                      Update_Leaderboard(score, current_player_name); // Запис у таблицю рекордів
                      Game_Init(); // Очищення поля
                      Send_Packet(0x12, 0, 0, 0, 0xAA); // Підтвердження
                      Send_Full_Board(); // Оновлення екрану у Python
                  }
                  break;

                  case 0x15: // ОТРИМАТИ SCORE
                  {
                      uint8_t tx_score[PACKET_SIZE] = {0x15, (uint8_t)((score>>24)&0xFF), (uint8_t)((score>>16)&0xFF), (uint8_t)((score>>8)&0xFF), (uint8_t)(score&0xFF), 0};
                      tx_score[5] = CRC8_Calc(tx_score, 5);
                      HAL_UART_Transmit(&huart1, tx_score, PACKET_SIZE, 100);
                  }
                  break;

                  case 0x20: // ПРИЙНЯТИ ІМ'Я
                  {
                      uint8_t chunk = current_packet.addr_h;
                      if (chunk < 6) {
                          int base = chunk * 3;
                          if (base < 16) current_player_name[base] = current_packet.addr_l;
                          if (base+1 < 16) current_player_name[base+1] = current_packet.data_h;
                          if (base+2 < 16) current_player_name[base+2] = current_packet.data_l;
                      }
                      if (chunk == 5) current_player_name[15] = '\0';
                      Send_Packet(0x20, chunk, 0, 0, 0xAA);
                  }
                  break;

                  case 0x30: // ЗБЕРЕГТИ СТАН ГРИ (Слот)
                      Save_Game(current_packet.addr_h);
                      Send_Packet(0x30, current_packet.addr_h, 0, 0, 0xAA);
                      break;

                  case 0x31: // ЗАВАНТАЖИТИ СТАН ГРИ
                      if (Load_Game(current_packet.addr_h)) {
                          Send_Packet(0x31, current_packet.addr_h, 0, 0, 0xAA);
                          HAL_Delay(10);
                          Send_Full_Board();
                      } else {
                          Send_Packet(0x31, current_packet.addr_h, 0, 0, 0xEE);
                      }
                      break;
               case 0x32: // ЗАПИТАТИ НІКНЕЙМ ЗІ СЛОТА (12 літер)
{
    uint8_t slot = current_packet.addr_h; // Отримуємо номер слота (0, 1, 2)
    
    if (slot < MAX_SAVE_SLOTS) {
        GameSaveData_t *flash_ptr = (GameSaveData_t *)FLASH_SAVE_ADDR;
        
        // Перевіряємо валідність збереження
        if (flash_ptr[slot].magic == SAVE_MAGIC_NUMBER) {
            
            // Пакет 1: символи 0, 1, 2 (Команда 0x33)
            Send_Packet(0x33, slot, flash_ptr[slot].playerName[0], 
                                    flash_ptr[slot].playerName[1], 
                                    flash_ptr[slot].playerName[2]);
            HAL_Delay(5);

            // Пакет 2: символи 3, 4, 5 (Команда 0x34)
            Send_Packet(0x34, slot, flash_ptr[slot].playerName[3], 
                                    flash_ptr[slot].playerName[4], 
                                    flash_ptr[slot].playerName[5]);
            HAL_Delay(5);

            // Пакет 3: символи 6, 7, 8 (Команда 0x35)
            Send_Packet(0x35, slot, flash_ptr[slot].playerName[6], 
                                    flash_ptr[slot].playerName[7], 
                                    flash_ptr[slot].playerName[8]);
            HAL_Delay(5);

            // Пакет 4: символи 9, 10, 11 (Команда 0x36)
            Send_Packet(0x36, slot, flash_ptr[slot].playerName[9], 
                                    flash_ptr[slot].playerName[10], 
                                    flash_ptr[slot].playerName[11]);
            HAL_Delay(5);
            
            // Фінальний статус: Успішно (0xAA)
            Send_Packet(0x32, slot, 0, 0, 0xAA); 
        } else {
            // Статус: Слот порожній (0xEE)
            Send_Packet(0x32, slot, 0, 0, 0xEE); 
        }
    }
}
break;

                 case 0x40: // ОТРИМАТИ ТАБЛИЦЮ ЛІДЕРІВ
{
    Leaderboard_t lb;
    Get_Leaderboard(&lb);

    for (uint8_t i = 0; i < MAX_LEADERS; i++) {
        // 1. Передача імені (розбиваємо 10 літер на декілька пакетів)
        // Пакет 1: символи 0, 1, 2
        Send_Packet(0x41, i, lb.leaders[i].playerName[0], lb.leaders[i].playerName[1], lb.leaders[i].playerName[2]);
        HAL_Delay(5);

        // Пакет 2: символи 3, 4, 5
        Send_Packet(0x43, i, lb.leaders[i].playerName[3], lb.leaders[i].playerName[4], lb.leaders[i].playerName[5]);
        HAL_Delay(5);

        // Пакет 3: символи 6, 7, 8
        Send_Packet(0x44, i, lb.leaders[i].playerName[6], lb.leaders[i].playerName[7], lb.leaders[i].playerName[8]);
        HAL_Delay(5);

        // Пакет 4: символ 9 (остання літера)
        Send_Packet(0x45, i, lb.leaders[i].playerName[9], 0x00, 0x00);
        HAL_Delay(5);

        // 2. Передача балів (score)
        uint8_t s_h = (uint8_t)((lb.leaders[i].score >> 8) & 0xFF);
        uint8_t s_l = (uint8_t)(lb.leaders[i].score & 0xFF);
        Send_Packet(0x42, i, 0, s_h, s_l);
        HAL_Delay(5);
    }
}
break;

                  default:
                      Send_Packet(current_packet.cmd, 0, 0, 0, 0xFF);
                      break;
              }
          }
          packet_received_flag = 0;
          rx_idx = 0;
      }
  }
}

/* --- SYSTEM CONFIG & INTERRUPTS --- */

void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL12;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1);
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK1;
  HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == USART1) {
    uint32_t current_tick = HAL_GetTick();
    if (rx_idx > 0 && (current_tick - last_byte_tick > TIMEOUT_MS)) rx_idx = 0;
    last_byte_tick = current_tick;
    if (packet_received_flag == 0) {
        rx_buffer[rx_idx++] = rx_byte;
        if (rx_idx >= PACKET_SIZE) {
            packet_received_flag = 1;
            rx_idx = 0;
        }
    }
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == USART1) {
    __HAL_UART_CLEAR_OREFLAG(huart);
    rx_idx = 0; packet_received_flag = 0;
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
  }
}

void Error_Handler(void)
{
  __disable_irq();
  while (1) {}
}
