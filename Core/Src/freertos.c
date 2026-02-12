#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"
#include "usart.h"

osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void) {
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);
}

void StartDefaultTask(void *argument) {
  /* USER CODE BEGIN StartDefaultTask */
  uint8_t msg_ask[] = "\r\nPlease enter a letter: "; // Запит
  uint8_t msg_res[] = "You entered: ";               // Відповідь
  uint8_t rx_byte;                                   // Сюди запишемо твою букву

  for(;;) {
    // 1. Відправляємо запит у Tera Term
    HAL_UART_Transmit(&huart1, msg_ask, sizeof(msg_ask)-1, 100);

    // 2. ЧЕКАЄМО. Функція зупинить задачу, поки ти не введеш 1 символ
    // HAL_MAX_DELAY означає, що плата чекатиме вічно
    if (HAL_UART_Receive(&huart1, &rx_byte, 1, HAL_MAX_DELAY) == HAL_OK) {

        // 3. Виводимо результат
        HAL_UART_Transmit(&huart1, (uint8_t*)"\r\n", 2, 10); // Новий рядок
        HAL_UART_Transmit(&huart1, msg_res, sizeof(msg_res)-1, 100);
        HAL_UART_Transmit(&huart1, &rx_byte, 1, 100);
        HAL_UART_Transmit(&huart1, (uint8_t*)"\r\n", 2, 10); // Новий рядок

        // 4. Мигаємо зеленим світлодіодом PC9 для візуального контролю
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_9);
    }

    // Невелика пауза перед наступним запитом
    osDelay(500);
  }
  /* USER CODE END StartDefaultTask */
}
