/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body (Protocol Handler with Diff Update)
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

// Коди команд для відповіді
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

// Буфер для збереження старого стану поля (Snapshot)
uint8_t board_snapshot[BOARD_ROWS][BOARD_COLS];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USER CODE BEGIN 0 */
// Функція розрахунку CRC (твоя оригінальна)
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

// Універсальна функція відправки пакету
// cmd: команда, r/c: координати, data: колір/дані, status: статус (0xAA/0xEE)
void Send_Packet(uint8_t cmd, uint8_t r, uint8_t c, uint8_t data, uint8_t status)
{
    uint8_t tx_buf[PACKET_SIZE];
    tx_buf[0] = cmd;
    tx_buf[1] = r;      // Row / Addr_H
    tx_buf[2] = c;      // Col / Addr_L
    tx_buf[3] = data;   // Color / Data_H
    tx_buf[4] = status; // Status / Data_L
    tx_buf[5] = CRC8_Calc(tx_buf, 5); // CRC

    HAL_UART_Transmit(&huart1, tx_buf, PACKET_SIZE, 100);
}

// Функція відправки різниці між старим і новим станом
void Send_Board_Diff(void)
{
    for (uint8_t r = 0; r < BOARD_ROWS; r++) {
        for (uint8_t c = 0; c < BOARD_COLS; c++) {
            // Порівнюємо поточне поле зі збереженим знімком
            if (board[r][c] != board_snapshot[r][c]) {
                // Якщо клітинка змінилась - відправляємо оновлення
                Send_Packet(CMD_UPDATE_CELL, r, c, board[r][c], 0xAA);

                // Маленька затримка, щоб приймач встиг обробити потік даних
                HAL_Delay(2);
            }
        }
    }
}

// Функція для примусової відправки всього поля (наприклад, при старті гри)
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

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART1_UART_Init();

  /* USER CODE BEGIN 2 */
  __HAL_UART_FLUSH_DRREGISTER(&huart1);
  HAL_UART_Receive_IT(&huart1, &rx_byte, 1);

  // Ініціалізуємо гру при старті
  Game_Init();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      if (packet_received_flag == 1)
      {
          // 1. Розбір пакету
          current_packet.cmd    = rx_buffer[0];
          current_packet.addr_h = rx_buffer[1];
          current_packet.addr_l = rx_buffer[2];
          current_packet.data_h = rx_buffer[3];
          current_packet.data_l = rx_buffer[4];
          current_packet.crc    = rx_buffer[5];

          // 2. Перевірка CRC
          uint8_t calc_crc = CRC8_Calc(rx_buffer, 5);

          if (calc_crc == current_packet.crc)
          {
              // === ВАЛІДНИЙ ПАКЕТ ===
              switch (current_packet.cmd)
              {
                  case 0x10: // NEW GAME
                      Game_Init();
                      Send_Packet(0x10, 0, 0, 0, 0xAA); // Підтвердження старту
                      Send_Full_Board();                // Відправляємо все нове поле
                      break;

                  case 0x11: // SWAP
                  {
                      // 1. Робимо знімок поля ДО змін (для відправки різниці)
                      memcpy(board_snapshot, board, sizeof(board));

                      // 2. Пробуємо зробити хід
                      uint8_t success = Game_Swap(current_packet.addr_h, current_packet.addr_l,
                                                  current_packet.data_h, current_packet.data_l);

                      if (success) {
                          // Хід був за правилами, кубики згоріли і впали нові.

                          // 3. ПЕРЕВІРКА НА ГЛУХИЙ КУТ (Deadlock)
                          if (Game_HasPossibleMoves() == 0) {
                              // Ходів більше немає! Відправляємо статус 0xDD (Deadlock)
                              Send_Packet(0x11, 0, 0, 0, 0xDD);
                          } else {
                              // Все добре, гра продовжується. Відправляємо 0xAA (Успіх)
                              Send_Packet(0x11, 0, 0, 0, 0xAA);
                          }

                          // 4. Відправляємо Фронтенду оновлені клітинки
                          Send_Board_Diff();

                      } else {
                          // Хід неможливий (не сусідні кубики, або немає лінії 3-в-ряд)
                          Send_Packet(0x11, 0, 0, 0, 0xEE); // Помилка
                      }
                  }
                  break;

                  case 0x14: // GET CELL (Запит конкретної клітинки)
                  {
                      uint8_t r = current_packet.addr_h;
                      uint8_t c = current_packet.addr_l;

                      if (r < BOARD_ROWS && c < BOARD_COLS) {
                          Send_Packet(0x14, r, c, board[r][c], 0xAA);
                      } else {
                          Send_Packet(0x14, r, c, 0, 0xEE);
                      }
                  }
                  break;

                  case 0x15: // === GET SCORE ===
                  {
                      uint8_t tx_score[PACKET_SIZE];
                      tx_score[0] = 0x15; // Команда відповіді

                      // Рахунок - це uint32_t (4 байти). Розбиваємо його на байти для передачі
                      tx_score[1] = (uint8_t)((score >> 24) & 0xFF); // Найстарший байт
                      tx_score[2] = (uint8_t)((score >> 16) & 0xFF);
                      tx_score[3] = (uint8_t)((score >> 8)  & 0xFF);
                      tx_score[4] = (uint8_t)(score & 0xFF);         // Наймолодший байт

                      // Рахуємо CRC для цього пакета
                      tx_score[5] = CRC8_Calc(tx_score, 5);

                      // ВІДПРАВЛЯЄМО ПАКЕТ
                      HAL_UART_Transmit(&huart1, tx_score, PACKET_SIZE, 100);
                  }
                  break;

                  default: // Невідома команда
                      Send_Packet(current_packet.cmd, 0, 0, 0, 0xFF);
                      break;
              }
          }
          else
          {
              // === ПОМИЛКА CRC ===
              // Відправляємо діагностичний пакет
              Send_Packet(0xEE, calc_crc, current_packet.crc, 0xEE, 0xEE);
          }

          // Скидання прапора для прийому наступного пакету
          packet_received_flag = 0;
          rx_idx = 0; // Скидаємо індекс буфера на початок
      }
  }
  /* USER CODE END 3 */
}

/* ... (SystemClock_Config залишається без змін) ... */
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
  RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1);
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK1;
  HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);
}

// Callback для прийому по перериванню
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == USART1) {
	uint32_t current_tick = HAL_GetTick();
	// Тайм-аут: якщо байт прийшов надто пізно, скидаємо пакет
	if (rx_idx > 0 && (current_tick - last_byte_tick > TIMEOUT_MS)) rx_idx = 0;
	last_byte_tick = current_tick;

	if (packet_received_flag == 0) {
		rx_buffer[rx_idx++] = rx_byte;
		if (rx_idx >= PACKET_SIZE) {
			packet_received_flag = 1;
			rx_idx = 0;
		}
	}
	// Знову вмикаємо прийом переривання
	HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == USART1) {
	__HAL_UART_CLEAR_OREFLAG(huart);
	rx_idx = 0;
	packet_received_flag = 0;
	HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
  }
}

void Error_Handler(void) {
  __disable_irq();
  while (1) {}
}
