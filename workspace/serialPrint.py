import serial

# Cấu hình cổng và baudrate
port = "/dev/ttyACM0"   # đổi nếu cần, ví dụ /dev/ttyUSB0
baud = 115200

try:
    # Mở cổng serial
    with serial.Serial(port, baud, timeout=1) as ser:
        print(f"Đang đọc dữ liệu từ {port} @ {baud} baud...\nNhấn Ctrl+C để thoát.\n")

        while True:
            line = ser.readline().decode(errors='ignore').strip()
            if line:
                print(line)

except serial.SerialException as e:
    print(f"Lỗi cổng serial: {e}")
except KeyboardInterrupt:
    print("\nĐã dừng chương trình.")
