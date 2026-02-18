	/* USER CODE BEGIN Header */
	/**
	  ******************************************************************************
	  * @file           : main.c
	  * @brief          : Main program body (Protocol Handler)
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
	/* USER CODE END PD */

	/* Private variables ---------------------------------------------------------*/
	/* USER CODE BEGIN PV */
	uint8_t rx_byte;
	uint8_t rx_buffer[PACKET_SIZE];
	uint8_t rx_idx = 0;
	volatile uint8_t packet_received_flag = 0;
	uint32_t last_byte_tick = 0;
	Packet_t current_packet;
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
	  /* Infinite loop */
	  while (1)
	  {
	      if (packet_received_flag == 1)
	      {
	          // Копіюємо дані в структуру
	          current_packet.cmd    = rx_buffer[0];
	          current_packet.addr_h = rx_buffer[1];
	          current_packet.addr_l = rx_buffer[2];
	          current_packet.data_h = rx_buffer[3];
	          current_packet.data_l = rx_buffer[4];
	          current_packet.crc    = rx_buffer[5];

	          // Рахуємо CRC для перших 5 байт
	          uint8_t calc_crc = CRC8_Calc(rx_buffer, 5);

	          if (calc_crc == current_packet.crc)
	          {
	              // === ВАЛІДНИЙ ПАКЕТ ===
	              // Очищаємо буфер перед відповіддю, залишаючи лише CMD
	              uint8_t tx_buf[PACKET_SIZE] = {current_packet.cmd, 0, 0, 0, 0, 0};

	              switch (current_packet.cmd)
	              {
	                  case 0x10: // NEW GAME
	                      Game_Init();
	                      tx_buf[4] = 0xAA; // Статус OK
	                      break;

	                  case 0x11: // SWAP
	                      if (Game_Swap(current_packet.addr_h, current_packet.addr_l,
	                                    current_packet.data_h, current_packet.data_l)) {
	                          tx_buf[4] = 0xAA;
	                      } else {
	                          tx_buf[4] = 0xEE; // Невдалий хід
	                      }
	                      break;

	                  default:
	                      tx_buf[4] = 0xFF; // Невідома команда
	                      break;
	              }

	              tx_buf[5] = CRC8_Calc(tx_buf, 5);
	              HAL_UART_Transmit(&huart1, tx_buf, PACKET_SIZE, 100);
	          }
	          else
	          {
	              // === ПОМИЛКА CRC (ДІАГНОСТИКА) ===
	              // Byte 0: EE (Error)
	              // Byte 1: calc_crc (Те, що STM32 нарахував сам!)
	              // Byte 2: current_packet.crc (Те, що ти надіслав)
	              uint8_t err_diag[PACKET_SIZE] = {0xEE, calc_crc, current_packet.crc, 0xEE, 0xEE, 0xEE};
	              HAL_UART_Transmit(&huart1, err_diag, PACKET_SIZE, 100);
	          }
	          packet_received_flag = 0;
	      }
	  }
	  /* USER CODE END 3 */
	}

	/* ... (Встав сюди SystemClock_Config, UART callback та Error_Handler як і раніше) ... */
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
		rx_idx = 0;
		packet_received_flag = 0;
		HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
	  }
	}

	void Error_Handler(void) {
	  __disable_irq();
	  while (1) {}
	}
