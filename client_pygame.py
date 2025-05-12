import pygame
import socket
import threading

# Dimensiones del mapa lógico y de la ventana gráfica
WIDTH, HEIGHT = 40, 20
CELL_SIZE = 20
WINDOW_WIDTH = WIDTH * CELL_SIZE
WINDOW_HEIGHT = HEIGHT * CELL_SIZE

# Colores
BORDER_COLOR = (100, 100, 100)
FOOD_COLOR = (255, 255, 0)
BG_COLOR = (30, 30, 30)
PLAYER_COLORS = [
    (0, 200, 0), (0, 150, 255), (255, 100, 100),
    (200, 0, 200), (255, 165, 0), (180, 180, 0)
]

# Conexión al servidor
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(("127.0.0.1", 12345))

# Estado actual del juego
game_state = []

# Recibir datos del servidor
def recv_loop():
    global game_state
    buffer = ""
    while True:
        try:
            data = sock.recv(2048).decode()
            buffer += data
            while "UPDATE\n" in buffer and "END\n" in buffer:
                start = buffer.index("UPDATE\n") + len("UPDATE\n")
                end = buffer.index("END\n")
                content = buffer[start:end].strip().splitlines()
                game_state = content
                buffer = buffer[end + len("END\n"):]
        except:
            break

recv_thread = threading.Thread(target=recv_loop, daemon=True)
recv_thread.start()

# Inicializar pygame
pygame.init()
win = pygame.display.set_mode((WINDOW_WIDTH, WINDOW_HEIGHT))
pygame.display.set_caption("Agar.io 2D Cliente")
font = pygame.font.SysFont("consolas", 18)

clock = pygame.time.Clock()

def draw_border():
    for x in range(0, WINDOW_WIDTH, CELL_SIZE):
        pygame.draw.line(win, BORDER_COLOR, (x, 0), (x, WINDOW_HEIGHT))
    for y in range(0, WINDOW_HEIGHT, CELL_SIZE):
        pygame.draw.line(win, BORDER_COLOR, (0, y), (WINDOW_WIDTH, y))

def draw_game():
    win.fill(BG_COLOR)
    draw_border()

    for line in game_state:
        if line.startswith("FOOD"):
            _, x, y = line.split()
            px, py = int(x) * CELL_SIZE, int(y) * CELL_SIZE
            pygame.draw.circle(win, FOOD_COLOR, (px + CELL_SIZE//2, py + CELL_SIZE//2), CELL_SIZE//4)
        elif line.startswith("PLAYER"):
            _, pid, x, y, size = line.split()
            pid, x, y, size = int(pid), int(x), int(y), int(size)
            px, py = x * CELL_SIZE, y * CELL_SIZE
            color = PLAYER_COLORS[pid % len(PLAYER_COLORS)]
            radius = max(CELL_SIZE//2, min(CELL_SIZE + size * 2, CELL_SIZE * 2))
            pygame.draw.circle(win, color, (px + CELL_SIZE//2, py + CELL_SIZE//2), radius)
            label = "*" if size >= 10 else str(size)
            text = font.render(label, True, (0, 0, 0))
            win.blit(text, (px + CELL_SIZE//2 - text.get_width()//2, py + CELL_SIZE//2 - text.get_height()//2))

    pygame.display.flip()

# Main loop
running = True
while running:
    clock.tick(30)
    keys = pygame.key.get_pressed()

    if keys[pygame.K_UP]:
        sock.sendall(b"MOVE 0 -1")
    elif keys[pygame.K_DOWN]:
        sock.sendall(b"MOVE 0 1")
    elif keys[pygame.K_LEFT]:
        sock.sendall(b"MOVE -1 0")
    elif keys[pygame.K_RIGHT]:
        sock.sendall(b"MOVE 1 0")

    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False

    draw_game()

pygame.quit()
sock.close()