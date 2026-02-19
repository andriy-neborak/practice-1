import serial
import time
import sys

SERIAL_PORT = 'COM15'
BAUD_RATE = 38400
TOTAL_TIMEOUT = 2.0       
INTER_BYTE_TIMEOUT = 0.15 

def crc8_0x07(data: bytes) -> int:
    crc = 0x00
    poly = 0x07
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = (crc << 1) ^ poly
            else:
                crc <<= 1
            crc &= 0xFF
    return crc

def main():
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=TOTAL_TIMEOUT)
        print(f"[Init] Connected to {SERIAL_PORT}")
    except Exception as e:
        print(f"[Error] {e}")
        return

    try:
        payload = bytes([0xAA, 0x01, 0x10, 0x20, 0x30])

        pc_crc = crc8_0x07(payload)
        full_packet = payload + bytes([pc_crc])

        print(f"\n--- ВІДПРАВКА (PC) ---")
        print(f"Дані: {payload.hex().upper()}")
        print(f"CRC (PC): {hex(pc_crc).upper()}")
        
        ser.write(full_packet)

        print(f"\n--- ПРИЙОМ (STM32) ---")
        rx_data = ser.read(6)

        if len(rx_data) == 6:
            rx_payload = rx_data[:5]
            rx_crc_from_mcu = rx_data[5]

            print(f"Отримані дані: {rx_payload.hex().upper()}")

            print(f"CRC від STM32: {hex(rx_crc_from_mcu).upper()}") 

            if rx_crc_from_mcu == pc_crc:
                print("\n[RESULT] ✅ Суми збігаються! STM32 порахував правильно.")
            else:
                print(f"\n[RESULT] ❌ Помилка! PC={hex(pc_crc)}, STM32={hex(rx_crc_from_mcu)}")
        else:
            print(f"[Error] Неповна відповідь. Отримано {len(rx_data)} байтів.")

    except KeyboardInterrupt:
        pass
    finally:
        ser.close()

if __name__ == "__main__":
    main()