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
#include <string.h> // Для роботи з пам'яттю
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
// Структура пакету (6 байт) для зручності доступу
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
#define TIMEOUT_MS  10  // Жорсткий таймаут між байтами
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* --- ЗМІННІ ДЛЯ ПРОТОКОЛУ --- */
uint8_t rx_byte;                // Тимчасовий байт (приймач)
uint8_t rx_buffer[PACKET_SIZE]; // Буфер для збору пакету
uint8_t rx_idx = 0;             // Поточний індекс

volatile uint8_t packet_received_flag = 0; // Прапор: 1 = є повний пакет
uint32_t last_byte_tick = 0;    // Час отримання останнього байта

Packet_t current_packet;        // Структура для розбору даних
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * @brief  Розрахунок CRC-8 з поліномом 0x07
 * @param  data: вказівник на масив даних
 * @param  len: довжина даних для розрахунку (зазвичай 5 байт)
 * @retval calculated_crc
 */
uint8_t CRC8_Calc(uint8_t *data, uint8_t len)
{
    uint8_t crc = 0x00; // Початкове значення (Init value)
    uint8_t i, j;

    for (i = 0; i < len; i++) {
        crc ^= data[i]; // XOR байта даних з поточним CRC
        for (j = 0; j < 8; j++) {
            if (crc & 0x80) { // Якщо старший біт = 1
                crc = (crc << 1) ^ 0x07; // Зсув вліво і XOR з поліномом 0x07
            } else {
                crc <<= 1; // Просто зсув вліво
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

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
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
  // Очищаємо сміття в регістрі
  __HAL_UART_FLUSH_DRREGISTER(&huart1);

  // Запускаємо прийом першого байта
  HAL_UART_Receive_IT(&huart1, &rx_byte, 1);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* --- 1. ПЕРЕВІРКА: ЧИ ПРИЙШОВ ПОВНИЙ ПАКЕТ? --- */
    if (packet_received_flag == 1)
    {
        // Копіюємо дані в структуру для зручності
        current_packet.cmd    = rx_buffer[0];
        current_packet.addr_h = rx_buffer[1];
        current_packet.addr_l = rx_buffer[2];
        current_packet.data_h = rx_buffer[3];
        current_packet.data_l = rx_buffer[4];
        current_packet.crc    = rx_buffer[5];

        /* --- 2. РАХУЄМО CRC (НОВИЙ МЕТОД) --- */
        // Передаємо перші 5 байт масиву (cmd, addr_h, addr_l, data_h, data_l)
        uint8_t calc_crc = CRC8_Calc(rx_buffer, 5);

        /* --- 3. ВАЛІДАЦІЯ --- */
        if (calc_crc == current_packet.crc)
        {
            // === ПАКЕТ ВАЛІДНИЙ ===

            // Тут твоя логіка гри.
            // Для прикладу: Просто підтверджуємо отримання (ACK) - повертаємо ті ж дані

            // Якщо треба змінити дані (наприклад, статус OK), розкоментуй:
            // rx_buffer[3] = 0x00;
            // rx_buffer[4] = 0xFF;

            // ПЕРЕРАХОВУЄМО CRC для відповіді (для перших 5 байтів)
            rx_buffer[5] = CRC8_Calc(rx_buffer, 5);

            // Відправляємо відповідь (ACK)
            HAL_UART_Transmit(&huart1, rx_buffer, PACKET_SIZE, 100);
        }
        else
                {
                    // === ПОМИЛКА CRC ===
                    // Відправляємо діагностичний пакет:
                    // [EE] [Порахована_CRC] [Отримана_CRC] [EE] [EE] [EE]
                    uint8_t err_packet[PACKET_SIZE] = {0xEE, calc_crc, current_packet.crc, 0xEE, 0xEE, 0xEE};
                    HAL_UART_Transmit(&huart1, err_packet, PACKET_SIZE, 100);
                }

        /* --- 4. ЗАВЕРШЕННЯ ОБРОБКИ --- */
        // Скидаємо прапор, дозволяємо приймати нові пакети в ISR
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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
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

  /** Initializes the CPU, AHB and APB buses clocks
  */
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

    /* А. ПЕРЕВІРКА ТАЙМАУТУ (10 мс) */
    // Якщо прийом йде (індекс > 0), але пауза затягнулася (> 10мс)
    if (rx_idx > 0 && (current_tick - last_byte_tick > TIMEOUT_MS))
    {
        rx_idx = 0; // Скидаємо "протухлий" пакет
    }

    // Оновлюємо час останнього байта
    last_byte_tick = current_tick;

    /* Б. БУФЕРИЗАЦІЯ */
    // Пишемо в буфер ТІЛЬКИ якщо Main Loop вже обробив попередній пакет (flag == 0)
    if (packet_received_flag == 0)
    {
        rx_buffer[rx_idx++] = rx_byte; // Записуємо байт

        // Якщо зібрали 6 байт
        if (rx_idx >= PACKET_SIZE)
        {
            packet_received_flag = 1; // Сигнал для Main Loop: "Є дані!"
            rx_idx = 0;               // Скидаємо індекс для наступного разу
        }
    }
    else
    {
        // Якщо flag == 1, значить Main ще думає.
        // Ми ігноруємо вхідні байти (або можна повертати помилку),
        // щоб не зіпсувати буфер, який зараз читає Main.
    }

    /* В. Слухаємо далі */
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    // Скидання при апаратних помилках
    __HAL_UART_CLEAR_OREFLAG(huart);
    rx_idx = 0;
    packet_received_flag = 0; // Примусове розблокування
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
/**
  * @brief  Reports the name of the source file and the source line number
  * where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
