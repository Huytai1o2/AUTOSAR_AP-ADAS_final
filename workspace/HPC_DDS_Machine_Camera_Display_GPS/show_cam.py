import cv2
import numpy as np
import socket
import struct

window_name = 'DDS AI Stream - TCP'
cv2.namedWindow(window_name, cv2.WINDOW_NORMAL | cv2.WINDOW_KEEPRATIO)
cv2.setWindowProperty(window_name, cv2.WND_PROP_FULLSCREEN, cv2.WINDOW_FULLSCREEN)

HOST = '127.0.0.1'
PORT = 8888
TRAILER_SIZE = 13
TRAILER_FMT = '<Bqi'


def recv_exact(conn, n):
    """Receive exactly n bytes, or return None on EOF."""
    buf = b''
    while len(buf) < n:
        chunk = conn.recv(n - len(buf))
        if not chunk:
            return None
        buf += chunk
    return buf


def outline_text(img, text, org, font, font_scale, color, thickness):
    cv2.putText(
        img, text, org, font, font_scale, (0, 0, 0), thickness + 2, cv2.LINE_AA
    )
    cv2.putText(
        img, text, org, font, font_scale, color, thickness, cv2.LINE_AA
    )


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
            length_data = conn.recv(4)
            if not length_data or len(length_data) < 4:
                break

            frame_len = struct.unpack('<I', length_data)[0]
            payload = recv_exact(conn, frame_len)
            if payload is None:
                break
            if len(payload) < frame_len:
                break

            trailer = recv_exact(conn, TRAILER_SIZE)
            if trailer is None:
                break
            # Set in_range, node_id, dist_m to global variables
            # in_range = 0
            # node_id = 0
            # dist_m = 0
            in_range, node_id, dist_m = struct.unpack(TRAILER_FMT, trailer)
            # print(f"[SCREEN] in_range: {in_range}, node_id: {node_id}, dist_m: {dist_m}")
            nparr = np.frombuffer(payload, np.uint8)
            img = cv2.imdecode(nparr, cv2.IMREAD_COLOR)

            if img is not None:
                h, w = img.shape[:2]
                font = cv2.FONT_HERSHEY_SIMPLEX
                fs = max(0.45, min(w, h) / 700.0)
                thk = max(1, int(round(fs * 2)))
                margin = 20

                if dist_m < 100: #for debug only
                    warn = 'Warning! Traffic light close, please slow down!'
                    (_tw, _th), bl = cv2.getTextSize(warn, font, fs, thk)
                    # org y is the text baseline; keep bottom of text block above image bottom
                    y_warn = h - margin - bl
                    outline_text(
                        img, warn, (margin, y_warn), font, fs, (0, 255, 255), thk
                    )

                y_br = margin + 30
                for line in (
                    f'Traffic id: {node_id}',
                    f'Distance: {dist_m} meters',
                ):
                    (tw, th), _ = cv2.getTextSize(line, font, fs, thk)
                    x = w - margin - tw
                    outline_text(
                        img, line, (x, y_br), font, fs, (0, 255, 255), thk
                    )
                    y_br += th + 8

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
