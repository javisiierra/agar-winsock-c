import socket
import threading
import pygame
import time

WIDTH, HEIGHT = 800, 600
GRID_W, GRID_H = 40, 20
CELL_SIZE = 30
SCALE_X = WIDTH // GRID_W
SCALE_Y = HEIGHT // GRID_H

SERVER_IP = "127.0.0.1"
SERVER_PORT = 12345

players = {}
foods = []

def parse_update(data):
    global players, foods
    players.clear()
    foods.clear()

    lines = data.strip().split("\n")
    for line in lines:
        if line.startswith("PLAYER"):
            _, pid, x, y, size = line.split()
            players[int(pid)] = (int(x), int(y), int(size))
        elif line.startswith("FOOD"):
            _, x, y = line.split()
            foods.append((int(x), int(y)))

def receive_updates(sock):
    buffer = ""
    while True:
        try:
            data = sock.recv(2048).decode()
            if not data:
                break
            buffer += data
            while "UPDATE\n" in buffer and "END\n" in buffer:
                start = buffer.index("UPDATE\n") + len("UPDATE\n")
                end = buffer.index("END\n")
                body = buffer[start:end]
                parse_update(body)
                buffer = buffer[end + len("END\n"):]
        except:
            break

def draw(screen):
    screen.fill((30, 30, 30))

    # Draw food
    for x, y in foods:
        pygame.draw.circle(screen, (255, 255, 0), (x * SCALE_X, y * SCALE_Y), 4)

    # Draw players
    for pid, (x, y, size) in players.items():
        color = pygame.Color("blue") if pid == 0 else pygame.Color("green")
        if pid == 0:
            color = pygame.Color("red")  # Highlight Bot 0
        radius = max(6, size * 3)
        pygame.draw.circle(screen, color, (x * SCALE_X, y * SCALE_Y), radius)
        font = pygame.font.SysFont(None, 20)
        label = font.render(str(pid), True, (255, 255, 255))
        screen.blit(label, (x * SCALE_X - 6, y * SCALE_Y - 6))

    pygame.display.flip()

def main():
    pygame.init()
    screen = pygame.display.set_mode((WIDTH, HEIGHT))
    pygame.display.set_caption("Agar.io Viewer")
    clock = pygame.time.Clock()

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((SERVER_IP, SERVER_PORT))

    threading.Thread(target=receive_updates, args=(sock,), daemon=True).start()

    try:
        while True:
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    raise KeyboardInterrupt
            draw(screen)
            clock.tick(30)
    except KeyboardInterrupt:
        pass
    finally:
        sock.close()
        pygame.quit()

if __name__ == "__main__":
    main()