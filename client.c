// Cliente Agar.io por consola con Windows Console API
// Compilar: gcc client.c -o client -lws2_32 -std=c99

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>

#define WIDTH 40
#define HEIGHT 20

#pragma comment(lib, "ws2_32.lib")

void gotoxy(int x, int y) {
    COORD c = {x, y};
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), c);
}

void draw_border() {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
    for (int y = 0; y <= HEIGHT; y++) {
        for (int x = 0; x <= WIDTH; x++) {
            if (x == 0 || x == WIDTH || y == 0 || y == HEIGHT) {
                gotoxy(x, y);
                printf("#");
            }
        }
    }
}

void clear_screen() {
    system("cls");
}

void draw_game(const char *state) {
    clear_screen();
    draw_border();

    const char *line = state;
    while (*line) {
        if (strncmp(line, "PLAYER", 6) == 0) {
            int id, x, y, size;
            sscanf(line, "PLAYER %d %d %d %d", &id, &x, &y, &size);
            SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 10 + (id % 6));
            gotoxy(x, y);
            printf("%c", (size < 10) ? ('0' + size) : '*');
        } else if (strncmp(line, "FOOD", 4) == 0) {
            int x, y;
            sscanf(line, "FOOD %d %d", &x, &y);
            SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 14);
            gotoxy(x, y);
            printf("*");
        }
        while (*line && *line != '\n') line++;
        if (*line == '\n') line++;
    }
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
}

DWORD WINAPI recv_thread(LPVOID param) {
    SOCKET sock = *(SOCKET*)param;
    char buff[2048];
    int len;
    while ((len = recv(sock, buff, sizeof(buff)-1, 0)) > 0) {
        buff[len] = '\0';
        const char *start = strstr(buff, "UPDATE\n");
        if (start) {
            const char *end = strstr(start, "END\n");
            if (end) {
                char temp[2048];
                strncpy(temp, start + 7, end - (start + 7));
                temp[end - (start + 7)] = '\0';
                draw_game(temp);
            }
        }
    }
    return 0;
}

int main() {
    WSADATA wsa;
    SOCKET s;
    struct sockaddr_in server;

    WSAStartup(MAKEWORD(2,2), &wsa);
    s = socket(AF_INET, SOCK_STREAM, 0);

    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;
    server.sin_port = htons(12345);

    connect(s, (struct sockaddr *)&server, sizeof(server));

    CreateThread(NULL, 0, recv_thread, (void*)&s, 0, NULL);

    while (1) {
        if (GetAsyncKeyState(VK_UP) & 0x8000) send(s, "MOVE 0 -1", 9, 0);
        if (GetAsyncKeyState(VK_DOWN) & 0x8000) send(s, "MOVE 0 1", 8, 0);
        if (GetAsyncKeyState(VK_LEFT) & 0x8000) send(s, "MOVE -1 0", 10, 0);
        if (GetAsyncKeyState(VK_RIGHT) & 0x8000) send(s, "MOVE 1 0", 9, 0);
        Sleep(100);
    }

    closesocket(s);
    WSACleanup();
    return 0;
}