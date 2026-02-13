#include "main.h"
#include "usart.h"
#include "gpio.h"

/* --- ГЛОБАЛЬНІ ЗМІННІ --- */
extern UART_HandleTypeDef huart1;
uint8_t rx_byte;          // Тимчасовий байт для прийому
uint8_t echo_buf[6];      // Буфер для пакету з 6 байт
uint8_t rx_idx = 0;       // Поточний індекс у буфері

/* Прототипи системних функцій */
void SystemClock_Config(void);

/**
  * @brief Головна функція
  */
int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  __HAL_UART_FLUSH_DRREGISTER(&huart1); // Очищаємо "сміття" з порту перед грою

  /* СТАРТ: Вмикаємо прийом першого байта */
  HAL_UART_Receive_IT(&huart1, &rx_byte, 1);

  while (1)
  {
    /* Весь обмін відбувається в перериваннях автоматично */
  }
}

/**
  * @brief Колбек отримання байта
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    echo_buf[rx_idx++] = rx_byte; // Записуємо отриманий байт у масив

    if (rx_idx >= 6) // Якщо отримали рівно 6 байт
    {
      /* Відправляємо цей же пакет назад */
      HAL_UART_Transmit(&huart1, echo_buf, 6, 10);
      rx_idx = 0; // Скидаємо індекс для нового пакету
    }

    /* ВАЖЛИВО: Перезапускаємо прослуховування наступного байта */
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
  }
}

/**
  * @brief Обробка помилок (Overrun Error)
  */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    /* Очищаємо прапорець переповнення, який міг заблокувати UART */
    __HAL_UART_CLEAR_OREFLAG(huart);
    /* Перезапускаємо прийом після збою */
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
  }
}

/* --- СИСТЕМНІ НАЛАШТУВАННЯ (щоб не було помилок компиляції) --- */

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL12;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1);
}

void Error_Handler(void)
{
  __disable_irq();
  while (1) {}
}
