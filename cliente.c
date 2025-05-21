#include <stdio.h>
#include <winsock2.h>
#include "mensajes.h"

#pragma comment(lib, "ws2_32.lib")



void printHora(char *mensaje) {
    SYSTEMTIME st;
    GetSystemTime(&st);
    printf("[%02d:%02d:%02d.%03d - %d] %s\n",
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, GetCurrentThreadId(), mensaje);
}

int main(){

    WSADATA wsa;

    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        printHora("Error inicializando Winsock");
        return 1;
    }

    SOCKET cliente_sock;

    
    cliente_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (cliente_sock == INVALID_SOCKET) {
        printHora("Error creando el socket cliente");
        WSACleanup();
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");



    if(connect(cliente_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR){
        printHora("Error en connect");
        closesocket(cliente_sock);
        WSACleanup();
        return 1;
    }

    printHora("Conectado al servidor");

    char buffer[1024];
    int bytes_recibidos;

    while(1){
        
        printf("> ");
        fgets(buffer, sizeof(buffer), stdin);

        buffer[strcspn(buffer, "\n")] = '\0';

        if (strcmp(buffer, "") == 0) {
            continue;
        }

        if (strcmp(buffer, "1") == 0) {
            // Enviamos solicitud para unirnos a la partida
            if(send(cliente_sock, UNIRSE_A_PARTIDA, sizeof(UNIRSE_A_PARTIDA), 0) == SOCKET_ERROR){
            printHora("Error en send");
            break;
            }
            continue;
        }

        // Salir si el usuario escribe 'salir'
        if (strcmp(buffer, "salir") == 0) {
            printHora("Desconectando...");
            break;
        }


        Vector2D vec = {1.23f, 4.56f};
        // vec.x = htonf(vec.x);
        // vec.y = htonf(vec.y);
        printf("Vector: %f, %f\n", vec.x, vec.y);

        if(send(cliente_sock, (char*)&vec, sizeof(vec), 0) == SOCKET_ERROR){
            printHora("Error en send");
            break;
        }


        bytes_recibidos = recv(cliente_sock, buffer, sizeof(buffer)-1, 0);

        if(bytes_recibidos > 0 ){
            buffer[bytes_recibidos] = '\0';
            char buffer2[1024];
            sprintf(buffer2, "Servidor: %s", buffer);
            printHora(buffer2);
        } else if (bytes_recibidos == 0){
            printHora("Servidor desconectado");
            break;
        } else {
            printHora("Error en recv");
            break;
        }

    }

    printHora("Desconectado");
    closesocket(cliente_sock);
    WSACleanup();

    return 0;
}