import socket
import threading
import time
import random

SERVER_IP = "127.0.0.1"
SERVER_PORT = 12345
NUM_BOTS = 5
MOVE_INTERVAL = 0.2

directions = [(0,-1), (0,1), (-1,0), (1,0)]

all_sizes = {}  # Mapa global id -> size actual

class BotClient:
    def __init__(self, id):
        self.id = id
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((SERVER_IP, SERVER_PORT))
        self.running = True
        self.size = 1
        self.thread = threading.Thread(target=self.listen, daemon=True)
        self.thread.start()

    def listen(self):
        buffer = ""
        while self.running:
            try:
                data = self.sock.recv(2048).decode()
                if not data:
                    break
                buffer += data
                while "UPDATE\n" in buffer and "END\n" in buffer:
                    start = buffer.index("UPDATE\n") + len("UPDATE\n")
                    end = buffer.index("END\n")
                    lines = buffer[start:end].strip().splitlines()
                    self.handle_game_state(lines)
                    buffer = buffer[end + len("END\n"):]
            except:
                break

    def handle_game_state(self, lines):
        global all_sizes
        current_sizes = {}

        for line in lines:
            if line.startswith("PLAYER"):
                _, pid, x, y, size = line.split()
                pid, size = int(pid), int(size)
                current_sizes[pid] = size

        # Comprobar cambios de tamaño
        for pid in current_sizes:
            old = all_sizes.get(pid, current_sizes[pid])
            new = current_sizes[pid]
            if new > old:
                diff = new - old
                if pid == self.id:
                    print(f"[Bot {self.id}] Creció de {old} → {new} (+{diff})")
                    if diff >= 2:
                        print(f"  😈 [Bot {self.id}] Probablemente devoró a otro jugador")
                    else:
                        print(f"  🍎 [Bot {self.id}] Probablemente comió comida")
        all_sizes = current_sizes

    def move_loop(self):
        while self.running:
            dx, dy = random.choice(directions)
            move_cmd = f"MOVE {dx} {dy}"
            print(f"[Bot {self.id}] Mueve a (dx={dx}, dy={dy})")
            try:
                self.sock.sendall(move_cmd.encode())
            except:
                self.running = False
            time.sleep(MOVE_INTERVAL)

    def start(self):
        move_thread = threading.Thread(target=self.move_loop, daemon=True)
        move_thread.start()

bots = []

# Iniciar múltiples bots
for i in range(NUM_BOTS):
    bot = BotClient(i)
    bot.start()
    bots.append(bot)
    time.sleep(0.1)

print("Bots activos. Logs en tiempo real... Ctrl+C para detener.")

try:
    while True:
        time.sleep(1)
except KeyboardInterrupt:
    print("Deteniendo bots...")
    for bot in bots:
        bot.running = False
        bot.sock.close()