import serial
import time

def crc8(data):
    crc = 0
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) ^ 0x07) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
    return crc

ser = serial.serial_for_url('loop://', timeout=0.1)

command = 0x10
address1 = 0x01
address2 = 0x02
data1 = 0x05
data2 = 0x06

packet = bytes([command, address1, address2, data1, data2])

calculated_crc = crc8(packet)

print("Data bytes:     ", [f"0x{byte:02X}" for byte in packet])
print("Calculated CRC: ", f"0x{calculated_crc:02X}")

full_packet = packet + bytes([calculated_crc])

print("Full packet:    ", [f"0x{byte:02X}" for byte in full_packet])

ser.write(full_packet)

time.sleep(0.05)
received = ser.read(6)

print("\nReceived packet:", [f"0x{byte:02X}" for byte in received])

if len(received) == 6:
    received_data = received[:-1]
    received_crc = received[-1]

    recalculated_crc = crc8(received_data)
    print("CRC received:   ", f"0x{received_crc:02X}")
    print("CRC calculated: ", f"0x{recalculated_crc:02X}")

    if received_crc == recalculated_crc:
        print("CRC OK")
    else:
        print("CRC ERROR")
else:
    print("Timeout or incomplete packet")
