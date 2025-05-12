// Servidor básico para Agar.io en consola, sin SDL
// Compilar con: gcc server.c -o server -lws2_32

#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <windows.h>
#include <time.h>
#include <string.h>

#define MAX_CLIENTS 10
#define WIDTH 40
#define HEIGHT 20
#define FOOD_COUNT 30

#pragma comment(lib, "ws2_32.lib")

typedef struct {
    int id;
    int x, y;
    int size;
    SOCKET socket;
    int active;
} Player;

typedef struct {
    int x, y;
    int active;
} Food;

Player players[MAX_CLIENTS];
Food food[FOOD_COUNT];
int next_id = 1;

void init_food() {
    for (int i = 0; i < FOOD_COUNT; i++) {
        food[i].x = rand() % WIDTH;
        food[i].y = rand() % HEIGHT;
        food[i].active = 1;
    }
}

void check_collisions(Player *p) {
    // Comida
    for (int i = 0; i < FOOD_COUNT; i++) {
        if (food[i].active && food[i].x == p->x && food[i].y == p->y) {
            p->size++;
            food[i].active = 0;
        }
    }
    // Jugadores
    for (int i = 0; i < MAX_CLIENTS; i++) {
        Player *op = &players[i];
        if (op != p && op->active && op->x == p->x && op->y == p->y) {
            if (p->size > op->size) {
                p->size += op->size;
                op->active = 0;
                closesocket(op->socket);
            }
        }
    }
}

void send_state() {
    char buffer[2048] = "UPDATE\n";
    char line[128];

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (players[i].active) {
            snprintf(line, sizeof(line), "PLAYER %d %d %d %d\n", players[i].id, players[i].x, players[i].y, players[i].size);
            strcat(buffer, line);
        }
    }
    for (int i = 0; i < FOOD_COUNT; i++) {
        if (food[i].active) {
            snprintf(line, sizeof(line), "FOOD %d %d\n", food[i].x, food[i].y);
            strcat(buffer, line);
        }
    }
    strcat(buffer, "END\n");

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (players[i].active) {
            send(players[i].socket, buffer, strlen(buffer), 0);
        }
    }
}

DWORD WINAPI state_thread(void *arg) {
    while (1) {
        send_state();
        Sleep(200);
    }
    return 0;
}

// 🆕 Nuevo hilo: reaparecer comida aleatoriamente
DWORD WINAPI food_respawn_thread(void *arg) {
    while (1) {
        for (int i = 0; i < FOOD_COUNT; i++) {
            if (!food[i].active) {
                if (rand() % 100 < 10) { // 10% de probabilidad de reaparecer
                    food[i].x = rand() % WIDTH;
                    food[i].y = rand() % HEIGHT;
                    food[i].active = 1;
                }
            }
        }
        Sleep(2000); // revisar cada 2 segundos
    }
    return 0;
}

DWORD WINAPI client_thread(void *arg) {
    SOCKET client_socket = *(SOCKET*)arg;
    int id = next_id++;

    // Añadir jugador
    int slot = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!players[i].active) {
            slot = i;
            break;
        }
    }
    if (slot == -1) {
        closesocket(client_socket);
        return 0;
    }

    players[slot] = (Player){.id = id, .x = rand() % WIDTH, .y = rand() % HEIGHT, .size = 1, .socket = client_socket, .active = 1};

    char buffer[128];
    while (1) {
        int recv_size = recv(client_socket, buffer, sizeof(buffer)-1, 0);
        if (recv_size <= 0) break;
        buffer[recv_size] = '\0';

        if (strncmp(buffer, "MOVE", 4) == 0) {
            int dx, dy;
            sscanf(buffer, "MOVE %d %d", &dx, &dy);
            players[slot].x += dx;
            players[slot].y += dy;
            if (players[slot].x < 0) players[slot].x = 0;
            if (players[slot].x >= WIDTH) players[slot].x = WIDTH - 1;
            if (players[slot].y < 0) players[slot].y = 0;
            if (players[slot].y >= HEIGHT) players[slot].y = HEIGHT - 1;
            check_collisions(&players[slot]);
        }
    }

    players[slot].active = 0;
    closesocket(client_socket);
    return 0;
}

int main() {
    WSADATA wsa;
    SOCKET server_socket, client_socket;
    struct sockaddr_in server, client;
    int c;

    srand(time(NULL));
    init_food();

    WSAStartup(MAKEWORD(2,2), &wsa);
    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(12345);
    bind(server_socket, (struct sockaddr *)&server, sizeof(server));

    listen(server_socket, MAX_CLIENTS);
    printf("Servidor esperando conexiones...\n");

    CreateThread(NULL, 0, state_thread, NULL, 0, NULL);
    CreateThread(NULL, 0, food_respawn_thread, NULL, 0, NULL); // 🆕 lanzar hilo de comida

    c = sizeof(struct sockaddr_in);
    while ((client_socket = accept(server_socket, (struct sockaddr *)&client, &c))) {
        SOCKET *pclient = malloc(sizeof(SOCKET));
        *pclient = client_socket;
        CreateThread(NULL, 0, client_thread, (void*)pclient, 0, NULL);
    }

    closesocket(server_socket);
    WSACleanup();
    return 0;
}