# practice-1






# Проєкт: Four in a Row (STM32)

Навчальний проєкт реалізації логіки гри "4 в ряд" на мікроконтролері STM32 з використанням операційної системи реального часу.

## ⚙️ Технічні вимоги та інструменти

### Апаратне забезпечення
* **Target Board:** STM32F0Discovery
* **MCU:** STM32F051R8 (ARM Cortex-M0)
* **Flash:** 64 KB | **RAM:** 8 KB

### Програмне забезпечення
* **IDE:** STM32CubeIDE
* **Compiler:** GCC for ARM Embedded Processors (arm-none-eabi)
* **Debugger:** ST-LINK/V2 (Firmware V2J46S0)

### Використані бібліотеки
1.  **STM32F0 HAL Driver** — для роботи з периферією (GPIO, UART).
2.  **FreeRTOS** — операційна система реального часу для багатозадачності.
3.  **CMSIS-OS V2** — шар абстракції для RTOS.

## 🚀 Функціонал
На даному етапі реалізовано:
* [x] Налаштування тактування системи (RCC).
* [x] Інтеграція FreeRTOS.
* [x] Створення базової задачі (`StartDefaultTask`).
* [x] Керування світлодіодами (PC8, PC9) через RTOS.
* [x] Обробка натискання кнопки (PA0) для взаємодії з користувачем.

# 🎮 Four in a Row — PC Project Requirements
## 💻 System Requirements

### Operating System:
* [x] Windows 10 (64-bit)
* [x] Windows 11 (64-bit)

### Architecture:
* [x] x64 (recommended)

### Minimum Hardware Requirements:
* [x] CPU: Dual-core 1.8 GHz or higher
* [x] RAM: 4 GB (8 GB recommended)
* [x] Storage: ~100 MB free space

## 🛠 Development Environment
IDE:
* [x] Visual Studio Code 1.109.1
Python Version:
* [x] 3.14.3
Tkinter Version:
* [x] 8.6 (bundled with Python)
