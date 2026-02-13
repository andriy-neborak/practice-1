#include <stdint.h>
#include <stdio.h>

/* --- НАЛАШТУВАННЯ --- */
#define PACKET_SIZE 6
#define TIMEOUT_MS  10
#define REG_COUNT   16

/* --- ЗМІННІ --- */
uint8_t rx_buffer[PACKET_SIZE];
uint8_t rx_index = 0;
uint32_t last_byte_time = 0;

// Емуляція регістрів пристрою
uint16_t device_registers[REG_COUNT] = {0};

// Останній сформований пакет (для перегляду)
uint8_t last_tx_packet[PACKET_SIZE] = {0};

// Емуляція часу для таймауту
uint32_t mock_tick = 0;
uint32_t HAL_GetTick(void) { return mock_tick; }

/* --- 1. CRC-8 алгоритм (поліном 0x07) --- */
uint8_t CRC8(uint8_t *data, uint8_t len) {
    uint8_t crc = 0x00;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) crc = (crc << 1) ^ 0x07;
            else crc <<= 1;
        }
    }
    return crc;
}

/* --- 2. Відправка відповіді (симетричний протокол) --- */
void SendResponse(uint8_t cmd, uint16_t addr, uint16_t data) {
    last_tx_packet[0] = cmd;
    last_tx_packet[1] = (uint8_t)(addr >> 8);
    last_tx_packet[2] = (uint8_t)(addr & 0xFF);
    last_tx_packet[3] = (uint8_t)(data >> 8);
    last_tx_packet[4] = (uint8_t)(data & 0xFF);
    last_tx_packet[5] = CRC8(last_tx_packet, 5);

    printf("Відправлено відповідь: ");
    for (int i = 0; i < PACKET_SIZE; i++)
        printf("%02X ", last_tx_packet[i]);
    printf("\n");
}

/* --- 3. Обробка пакета --- */
void ProcessPacket(uint8_t *packet) {
    if (CRC8(packet, 5) != packet[5]) {
        printf("Помилка CRC! Пакет відкинуто\n");
        return;
    }

    uint8_t cmd = packet[0];
    uint16_t addr = (packet[1] << 8) | packet[2];
    uint16_t data = (packet[3] << 8) | packet[4];

    printf("Обробка пакета: CMD=0x%02X, ADDR=0x%04X, DATA=0x%04X\n",
           cmd, addr, data);

    if (cmd == 0x01) { // запис
        if (addr < REG_COUNT) device_registers[addr] = data;
        SendResponse(cmd, addr, data);
    } else if (cmd == 0x02) { // читання
        uint16_t read_val = (addr < REG_COUNT) ? device_registers[addr] : 0;
        SendResponse(cmd, addr, read_val);
    }
}

/* --- 4. Прийом байта з таймаутом --- */
void UART_Receive_Handler(uint8_t byte) {
    uint32_t now = HAL_GetTick();

    if (rx_index > 0 && (now - last_byte_time > TIMEOUT_MS)) {
        printf("Timeout! Скидаємо старий пакет\n");
        rx_index = 0;
    }

    last_byte_time = now;

    if (rx_index < PACKET_SIZE) {
        rx_buffer[rx_index++] = byte;
        printf("Прийнято байт: 0x%02X, rx_index=%d\n", byte, rx_index);
    }

    if (rx_index == PACKET_SIZE) {
        printf("Повний пакет прийнято: ");
        for (int i = 0; i < PACKET_SIZE; i++)
            printf("%02X ", rx_buffer[i]);
        printf("\n");

        ProcessPacket(rx_buffer);
        rx_index = 0;
    }
}

/* --- 5. ТЕСТОВИЙ СЦЕНАРІЙ --- */
int main(void) {
    printf("=== Тест №1: запис 0xABCD у регістр 3 ===\n");
    UART_Receive_Handler(0x01); // CMD
    UART_Receive_Handler(0x00); // ADDR_H
    UART_Receive_Handler(0x03); // ADDR_L
    UART_Receive_Handler(0xAB); // DATA_H
    UART_Receive_Handler(0xCD); // DATA_L
    UART_Receive_Handler(0x3F); // CRC (приклад)

    printf("\n=== Тест №2: перевірка таймауту ===\n");
    UART_Receive_Handler(0x01);
    UART_Receive_Handler(0x00);
    mock_tick += 15; // затримка >10ms
    UART_Receive_Handler(0x03);

    while (1) {
        // Для додаткових тестів або інтерактивного введення
    }
}
