import socket
import threading
import time
import random

# Dirección del servidor
HOST = "127.0.0.1"
PORT = 12345

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect((HOST, PORT))

# Escuchar el estado del juego (opcional para lógica más avanzada)
def listen():
    while True:
        try:
            data = sock.recv(2048).decode()
            if not data:
                break
        except:
            break

listener = threading.Thread(target=listen, daemon=True)
listener.start()

# Movimiento automático
directions = [(0,-1), (0,1), (-1,0), (1,0)]

try:
    while True:
        dx, dy = random.choice(directions)
        msg = f"MOVE {dx} {dy}\n"
        sock.sendall(msg.encode())
        time.sleep(0.3)
except KeyboardInterrupt:
    pass
finally:
    sock.close()