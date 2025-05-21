#include <stdio.h>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

#define PUERTO 12345

int main() {
    WSADATA wsa;
    SOCKET servidor, cliente, conexion;
    struct sockaddr_in addr;
    char buffer[128] = "Hola desde el cliente!";
    char recibido[128] = {0};
    int addrlen = sizeof(addr);

    // Inicializar Winsock
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        printf("Error inicializando Winsock\n");
        return 1;
    }

    // 1. Crear socket servidor
    servidor = socket(AF_INET, SOCK_STREAM, 0);
    if (servidor == INVALID_SOCKET) {
        printf("No se pudo crear el socket servidor\n");
        WSACleanup();
        return 1;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1
    addr.sin_port = htons(PUERTO);

    if (bind(servidor, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("Error en bind\n");
        closesocket(servidor);
        WSACleanup();
        return 1;
    }

    listen(servidor, 1);

    // 2. Crear socket cliente
    cliente = socket(AF_INET, SOCK_STREAM, 0);
    if (cliente == INVALID_SOCKET) {
        printf("No se pudo crear el socket cliente\n");
        closesocket(servidor);
        WSACleanup();
        return 1;
    }

    // 3. Conectar cliente al servidor (en la misma máquina)
    if (connect(cliente, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("Error conectando el cliente\n");
        closesocket(cliente);
        closesocket(servidor);
        WSACleanup();
        return 1;
    }

    // 4. El servidor acepta la conexión
    conexion = accept(servidor, NULL, NULL);
    if (conexion == INVALID_SOCKET) {
        printf("Error en accept\n");
        closesocket(cliente);
        closesocket(servidor);
        WSACleanup();
        return 1;
    }

    // 5. Cliente envía mensaje al servidor
    send(cliente, buffer, sizeof(buffer), 0);

    // 6. Servidor recibe mensaje
    recv(conexion, recibido, sizeof(recibido), 0);

    printf("Servidor recibió: %s\n", recibido);

    // Cerrar sockets y limpiar
    closesocket(conexion);
    closesocket(cliente);
    closesocket(servidor);
    WSACleanup();
    return 0;
}
