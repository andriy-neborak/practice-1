/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "game.h"    // [ЗМІНА]: Підключаємо логіку гри, яку ми винесли в окремий файл
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

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

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

/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();

  /* USER CODE BEGIN 2 */
  /* --- СТАРТ --- */

  // [ЗМІНА]: Очищуємо ігрове поле та готуємо змінні гри перед стартом циклу
  Game_Init();

  __HAL_UART_FLUSH_DRREGISTER(&huart1);
  HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* --- 1. ПЕРЕВІРКА: ЧИ ПРИЙШОВ ПОВНИЙ ПАКЕТ? --- */
    if (packet_received_flag == 1)
    {
        current_packet.cmd    = rx_buffer[0];
        current_packet.addr_h = rx_buffer[1];
        current_packet.addr_l = rx_buffer[2];
        current_packet.data_h = rx_buffer[3];
        current_packet.data_l = rx_buffer[4];
        current_packet.crc    = rx_buffer[5];

        /* --- 2. РАХУЄМО CRC --- */
        uint16_t sum = current_packet.cmd +
                       current_packet.addr_h + current_packet.addr_l +
                       current_packet.data_h + current_packet.data_l;

        uint8_t calc_crc = sum & 0xFF;

        /* --- 3. ВАЛІДАЦІЯ ТА ОБРОБКА --- */
        if (calc_crc == current_packet.crc)
        {
            // === ПАКЕТ ВАЛІДНИЙ ===

            // [ЗМІНА]: Логіка гри.
            // Якщо команда (cmd) дорівнює 0x01, вважаємо це ходом гравця.
            // Номер колонки беремо з байта data_l.
            if (current_packet.cmd == 0x01)
            {
                Game_ProcessStep(current_packet.data_l);
            }

            // Відправляємо стандартне підтвердження (ACK)
            HAL_UART_Transmit(&huart1, rx_buffer, PACKET_SIZE, 100);

            // [ЗМІНА]: Після кожного ходу відправляємо візуальне поле в UART
            // Це допоможе тобі бачити гру в терміналі (наприклад, TeraTerm або Putty)
            Game_SendBoardUART();
        }
        else
        {
            // === ПОМИЛКА CRC ===
            uint8_t err_packet[PACKET_SIZE] = {0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE};
            HAL_UART_Transmit(&huart1, err_packet, PACKET_SIZE, 100);
        }

        /* --- 4. ЗАВЕРШЕННЯ ОБРОБКИ --- */
        packet_received_flag = 0;
    }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
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
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
/* --- ОБРОБНИКИ ПЕРЕРИВАНЬ UART --- */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    uint32_t current_tick = HAL_GetTick();

    if (rx_idx > 0 && (current_tick - last_byte_tick > TIMEOUT_MS))
    {
        rx_idx = 0;
    }

    last_byte_tick = current_tick;

    if (packet_received_flag == 0)
    {
        rx_buffer[rx_idx++] = rx_byte;

        if (rx_idx >= PACKET_SIZE)
        {
            packet_received_flag = 1;
            rx_idx = 0;
        }
    }

    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    __HAL_UART_CLEAR_OREFLAG(huart);
    rx_idx = 0;
    packet_received_flag = 0;
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
  }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif /* USE_FULL_ASSERT */
