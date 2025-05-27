#include <stdio.h>
#include <winsock2.h>
#include "mensajes.h"
#include "dibujo.h"

#pragma comment(lib, "ws2_32.lib")


Entidad miEntidad;
void printHora(char *mensaje) {
    SYSTEMTIME st;
    GetSystemTime(&st);
    printf("[%02d:%02d:%02d.%03d - %d] %s\n",
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, GetCurrentThreadId(), mensaje);
}

int main(){

    inicializar_dibujo();

    WSADATA wsa;

    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        printHora("Error inicializando Winsock");
        cerrar_dibujo();
        return 1;
    }

    SOCKET cliente_sock;

    
    cliente_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (cliente_sock == INVALID_SOCKET) {
        printHora("Error creando el socket cliente");
        WSACleanup();
        cerrar_dibujo();
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
        cerrar_dibujo();
        return 1;
    }

    printHora("Conectado al servidor");

    char buffer[1024];
    int bytes_recibidos;
    Entidad entidades_recibidas[100];
    int num_entidades = 0;

    while(!debe_cerrar_ventana()){

        actualizar_dibujo(entidades_recibidas, num_entidades);
        
        printf("> ");
        fgets(buffer, sizeof(buffer), stdin);

        buffer[strcspn(buffer, "\n")] = '\0';

        if (strcmp(buffer, "") == 0) {
            continue;
        }

        if (strcmp(buffer, "0") == 0) {
            // Enviamos solicitud para unirnos a la partida
            int mensaje = UNIRSE_A_PARTIDA;
            if(send(cliente_sock, (char*)&mensaje, sizeof(mensaje), 0) == SOCKET_ERROR){
            printHora("Error en send");
            break;
            } else {
                printHora("Unirse a partida enviado");
                
                if(recv(cliente_sock, (char*)&miEntidad, sizeof(miEntidad), 0) == SOCKET_ERROR) {
                    printHora("Error en recv");
                    break;
                } else {
                    printHora("Entidad recibida. ID: %d", miEntidad.id);
                }
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

        // envia sus coordenadas
        if(send(cliente_sock, (char*)&vec, sizeof(vec), 0) == SOCKET_ERROR){
            printHora("Error en send");
            break;
        }


        // Espera a recibir la actualizacion del estado del mundo
        bytes_recibidos = recv(cliente_sock, (char*)entidades_recibidas, 100 * sizeof(Entidad), 0);


        if(bytes_recibidos > 0 ){
            num_entidades  = bytes_recibidos / sizeof(Entidad); // Calculate actual count
            // imprimimos el estado del mundo
            for(int i = 0; i < num_entidades; i++){
                if(entidades_recibidas[i].id == 0) continue;;
                char buffer2[1024];
                sprintf(buffer2, "Entidad %d: %f, %f", entidades_recibidas[i].id, entidades_recibidas[i].pos.x, entidades_recibidas[i].pos.y);
                printHora(buffer2);
            }
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
    cerrar_dibujo();

    return 0;
}