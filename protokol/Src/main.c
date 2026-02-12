/* 1. ПІДКЛЮЧЕННЯ БІБЛІОТЕК (Це виправить помилки uint8_t) */
#include <stdint.h>  /* Обов'язково для типів uint8_t, uint32_t */

/* Якщо ти працюєш в STM32CubeIDE, розкоментуй наступний рядок: */
/* #include "main.h" */

/* 2. НАЛАШТУВАННЯ ТА ГЛОБАЛЬНІ ЗМІННІ */
#define PACKET_SIZE 6
uint8_t rx_buffer[PACKET_SIZE];
uint8_t rx_index = 0;
uint32_t last_byte_time = 0;

/* Заглушки для функцій, щоб компілятор не "сварився" */
void WriteRegister(uint16_t addr, uint16_t data) {}
uint16_t ReadRegister(uint16_t addr) { return 0; }
uint32_t HAL_GetTick(void) { return 0; } /* Якщо немає HAL, це дозволить зібрати проект */

/* 3. ФУНКЦІЯ CRC-8 */
uint8_t CRC8(uint8_t *data, uint8_t len)
{
    uint8_t crc = 0x00;
    for(uint8_t i=0; i<len; i++)
    {
        crc ^= data[i];
        for(uint8_t j=0; j<8; j++)
        {
            if(crc & 0x80) crc = (crc << 1) ^ 0x07;
            else crc <<= 1;
        }
    }
    return crc;
}

/* 4. НАДСИЛАННЯ ВІДПОВІДІ (Тут я закоментував HAL_UART, щоб не було нових помилок) */
void SendResponse(uint8_t cmd, uint16_t addr, uint16_t data)
{
    uint8_t tx[PACKET_SIZE];
    tx[0] = cmd;
    tx[1] = (addr >> 8) & 0xFF;
    tx[2] = addr & 0xFF;
    tx[3] = (data >> 8) & 0xFF;
    tx[4] = data & 0xFF;
    tx[5] = CRC8(tx, 5);

    /* HAL_UART_Transmit(&huart1, tx, PACKET_SIZE, 100); */
}

/* 5. ОБРОБКА ПАКЕТА */
void ProcessPacket(uint8_t *packet)
{
    if(CRC8(packet, 5) != packet[5]) return;

    uint8_t cmd = packet[0];
    uint16_t addr = (packet[1] << 8) | packet[2];
    uint16_t data = (packet[3] << 8) | packet[4];

    if(cmd == 0x01) {
        WriteRegister(addr, data);
        SendResponse(cmd, addr, data);
    } else if(cmd == 0x02) {
        SendResponse(cmd, addr, ReadRegister(addr));
    }
}

/* 6. ПРИЙОМ БАЙТА */
void UART_Receive_Handler(uint8_t byte)
{
    uint32_t now = HAL_GetTick();
    if(rx_index > 0 && (now - last_byte_time > 10)) rx_index = 0;

    rx_buffer[rx_index++] = byte;
    last_byte_time = now;

    if(rx_index >= PACKET_SIZE)
    {
        ProcessPacket(rx_buffer);
        rx_index = 0;
    }
}

/* ГОЛОВНА ФУНКЦІЯ (Щоб проект мав точку входу) */
int main(void)
{
    while (1)
    {
        /* Основний цикл програми */
    }
}
