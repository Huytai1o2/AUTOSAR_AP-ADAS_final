import cv2
import numpy as np
import socket
import struct
import threading
import sys
import os

# Pin this process to core 3, leaving cores 0,1,2 for ProviderSensor/YOLO
try:
    os.sched_setaffinity(0, {3})
except Exception:
    pass  # non-Linux or no permission — continue without pinning

# python show_cam.py            -> fullscreen display (default)
# python show_cam.py display:=0 -> headless, no window
SHOW_DISPLAY = not any(arg == 'display:=0' for arg in sys.argv[1:])

window_name = 'DDS AI Stream - TCP'
if SHOW_DISPLAY:
    cv2.namedWindow(window_name, cv2.WINDOW_NORMAL | cv2.WINDOW_KEEPRATIO)
    cv2.setWindowProperty(window_name, cv2.WND_PROP_FULLSCREEN, cv2.WINDOW_FULLSCREEN)

HOST = '127.0.0.1'
PORT = 8888
TRAILER_SIZE = 13
TRAILER_FMT = '<Bqi'

latest_frame = None
latest_meta = (0, 0, 0)
frame_lock = threading.Lock()
stop_event = threading.Event()


def recv_exact(conn, n):
    buf = b''
    while len(buf) < n:
        chunk = conn.recv(n - len(buf))
        if not chunk:
            return None
        buf += chunk
    return buf


def outline_text(img, text, org, font, font_scale, color, thickness):
    cv2.putText(img, text, org, font, font_scale, (0, 0, 0), thickness + 2, cv2.LINE_AA)
    cv2.putText(img, text, org, font, font_scale, color, thickness, cv2.LINE_AA)


def receiver_thread(conn):
    global latest_frame, latest_meta
    try:
        while not stop_event.is_set():
            length_data = conn.recv(4)
            if not length_data or len(length_data) < 4:
                break

            frame_len = struct.unpack('<I', length_data)[0]
            payload = recv_exact(conn, frame_len)
            if payload is None or len(payload) < frame_len:
                break

            trailer = recv_exact(conn, TRAILER_SIZE)
            if trailer is None:
                break

            in_range, node_id, dist_m = struct.unpack(TRAILER_FMT, trailer)
            nparr = np.frombuffer(payload, np.uint8)
            img = cv2.imdecode(nparr, cv2.IMREAD_COLOR)

            if img is not None:
                with frame_lock:
                    latest_frame = img
                    latest_meta = (in_range, node_id, dist_m)
    except Exception as e:
        print(f"C++ ngắt kết nối hoặc gặp lỗi: {e}")
    finally:
        conn.close()


server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
server_socket.bind((HOST, PORT))
server_socket.listen(1)
server_socket.settimeout(1.0)

print(f"Bật hiển thị TCP, đang nghe tại {HOST}:{PORT} ...")
if not SHOW_DISPLAY:
    print("[INFO] Headless mode (display:=0) — no window.")

try:
    while True:
        try:
            conn, addr = server_socket.accept()
        except socket.timeout:
            if SHOW_DISPLAY and cv2.waitKey(1) & 0xFF in [ord('q'), 27]:
                break
            continue

        print(f"Đã kết nối với C++ DDS Component tại {addr}")
        stop_event.clear()
        t = threading.Thread(target=receiver_thread, args=(conn,), daemon=True)
        t.start()

        while t.is_alive():
            if SHOW_DISPLAY:
                with frame_lock:
                    img = latest_frame
                    in_range, node_id, dist_m = latest_meta

                if img is not None:
                    display = img.copy()
                    h, w = display.shape[:2]
                    font = cv2.FONT_HERSHEY_SIMPLEX
                    fs = max(0.45, min(w, h) / 700.0)
                    thk = max(1, int(round(fs * 2)))
                    margin = 20

                    # if dist_m < 100:
                    if in_range:
                        warn = 'Warning! Traffic light close, please slow down!'
                        (_tw, _th), bl = cv2.getTextSize(warn, font, fs, thk)
                        y_warn = h - margin - bl
                        outline_text(display, warn, (margin, y_warn), font, fs, (0, 255, 255), thk)

                    y_br = margin + 30
                    for line in (f'Traffic id: {node_id}', f'Distance: {dist_m} meters'):
                        (tw, th), _ = cv2.getTextSize(line, font, fs, thk)
                        x = w - margin - tw
                        outline_text(display, line, (x, y_br), font, fs, (0, 255, 255), thk)
                        y_br += th + 8

                    cv2.imshow(window_name, display)

                key = cv2.waitKey(30) & 0xFF
                if key in [ord('q'), 27]:
                    stop_event.set()
                    conn.close()
                    break

        t.join(timeout=2.0)
        with frame_lock:
            latest_frame = None

except KeyboardInterrupt:
    pass
finally:
    stop_event.set()
    server_socket.close()
    if SHOW_DISPLAY:
        cv2.destroyAllWindows()
