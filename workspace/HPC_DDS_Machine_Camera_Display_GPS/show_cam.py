import cv2
import numpy as np
import socket
import struct

window_name = 'DDS AI Stream - TCP'
cv2.namedWindow(window_name, cv2.WINDOW_NORMAL | cv2.WINDOW_KEEPRATIO)
cv2.setWindowProperty(window_name, cv2.WND_PROP_FULLSCREEN, cv2.WINDOW_FULLSCREEN)

HOST = '127.0.0.1'
PORT = 8888

# Mở máy chủ TCP
server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
server_socket.bind((HOST, PORT))
server_socket.listen(1)

print(f"Bật hiển thị TCP, đang nghe tại {HOST}:{PORT} ...")

while True:
    conn, addr = server_socket.accept()
    print(f"Đã kết nối với C++ DDS Component tại {addr}")
    try:
        while True:
            # 1. Đọc kích thước khung hình trước (4 bytes số nguyên)
            length_data = conn.recv(4)
            if not length_data or len(length_data) < 4:
                break
            
            # Giải mã số nguyên (Little Endian)
            frame_len = struct.unpack('<I', length_data)[0]

            # 2. Đọc chính xác dung lượng của Khung hình JPEG đó (Vòng lặp)
            payload = b""
            while len(payload) < frame_len:
                chunk = conn.recv(min(frame_len - len(payload), 4096))
                if not chunk:
                    break
                payload += chunk
                
            if len(payload) < frame_len:
                break # Mất kết nối nửa chừng

            # 3. Giải nén OpenCV và hiện lên
            nparr = np.frombuffer(payload, np.uint8)
            img = cv2.imdecode(nparr, cv2.IMREAD_COLOR)

            if img is not None:
                cv2.imshow(window_name, img)
            
            if cv2.waitKey(1) & 0xFF in [ord('q'), 27]:
                conn.close()
                break
    except Exception as e:
        print(f"C++ ngắt kết nối hoặc gặp lỗi: {e}")
    finally:
        conn.close()
        
server_socket.close()
cv2.destroyAllWindows()