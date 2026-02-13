/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usart.h"
#include "gpio.h"
#include <string.h>
#include <stdlib.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
#define ROWS 8
#define COLS 8

// Масив 8x8. Кольори: 1,2,3,4,5,6. (0 - пусто)
uint8_t gameField[ROWS][COLS];

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

// Функція ініціалізації поля (щоб не було 4 в ряд)
void InitGameField(void) {
    // 1. Проходимо по всіх рядках (y) та стовпцях (x)
    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {

            uint8_t color;
            int isValid = 0; // Прапорець: 0 - колір поганий, 1 - хороший

            // 2. Цикл: підбираємо колір, поки не знайдемо підходящий
            while (isValid == 0) {
                color = (rand() % 6) + 1; // Випадкове число від 1 до 6
                isValid = 1; // Сподіваємось, що підійде

                // 3. Перевірка зліва (щоб не було 4 однакові в ряд)
                if (x >= 3) {
                    if (gameField[y][x-1] == color &&
                        gameField[y][x-2] == color &&
                        gameField[y][x-3] == color) {
                        isValid = 0;
                    }
                }

                // 4. Перевірка зверху (щоб не було 4 однакові в стовпчик)
                if (isValid == 1 && y >= 3) {
                    if (gameField[y-1][x] == color &&
                        gameField[y-2][x] == color &&
                        gameField[y-3][x] == color) {
                        isValid = 0;
                    }
                }
            }
            // 5. Записуємо хороший колір у клітинку
            gameField[y][x] = color;
        }
    }
}

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */

  // Перша генерація при старті (щоб масив не був пустим)
  srand(0);
  InitGameField();

  /* Infinite loop */
  for(;;)
  {
    // --- Логіка Кнопки (PA0) ---
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET)
    {
      // === ЯКЩО КНОПКА НАТИСНУТА ===

      // 1. Вимикаємо синій діод (індикація натискання)
      HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, GPIO_PIN_RESET);

      // 2. ГЕНЕРУЄМО НОВЕ ПОЛЕ! (Ось це головна зміна)
      InitGameField();

      // 3. Затримка 200 мс, щоб не генерувало занадто швидко
      osDelay(200);
    }
    else
    {
      // === ЯКЩО КНОПКА ВІДПУЩЕНА ===
      // Блимаємо синім (режим очікування)
      HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_8);
    }

    // --- Зелений діод (PC9) завжди блимає ---
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_9);

    // --- Затримка циклу ---
    osDelay(100);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */
