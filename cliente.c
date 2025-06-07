#include <stdio.h>
#include <winsock2.h>
#include "mensajes.h"
#include "dibujo.h"
#include <math.h>
#include <stdbool.h>
#include <time.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

typedef enum {
    CLIENTE_CONECTADO,
    CLIENTE_SOLICITANDO_CONEXION, // de aqui partimos
    CLIENTE_ESPERANDO_RESPUESTA,
    CLIENTE_DESCONECTANDO  // HEMOS HECHO DESCONEXION
} EstadoCliente;



Entidad miEntidad;

int debugger = 0;

void printHora(char *mensaje) {
    SYSTEMTIME st;
    GetSystemTime(&st);
    printf("[%02d:%02d:%02d.%03d - %d] %s\n",
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, GetCurrentThreadId(), mensaje);
}

int main(){

    srand((unsigned int)time(NULL));
    int bit = 0;


    inicializar_dibujo();

    WSADATA wsa;
    //1. Inicializamos Winsock
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        printHora("Error inicializando Winsock");
        cerrar_dibujo();
        return 1;
    }

    //2. Creamos el socket UDP
    SOCKET socket_cliente = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_cliente == INVALID_SOCKET) {
        printHora("Error creando el socket cliente");
        WSACleanup();
        cerrar_dibujo();
        return 1;
    }

    //2. Lo configuramos como no bloqueante
    u_long non_blocking_mode = 1;
    if (ioctlsocket(socket_cliente, FIONBIO, &non_blocking_mode) == SOCKET_ERROR) {
        printHora("Error al configurar el socket cliente como no bloqueante");
        closesocket(socket_cliente);
        WSACleanup();
        cerrar_dibujo();
        return 1;
    }
    
    //3. Escribimos la direccion del servidor
    struct sockaddr_in direccion_servidor;
    direccion_servidor.sin_family = AF_INET;
    direccion_servidor.sin_port = htons(8080);
    direccion_servidor.sin_addr.s_addr = inet_addr("127.0.0.1");
    // direccion_servidor.sin_addr.s_addr = inet_addr("192.168.1.129");


    printHora("Conectado al servidor");


    EstadoCliente estadoCliente = CLIENTE_SOLICITANDO_CONEXION;
    Entidad entidades_mundo[MAX_ENTIDADES*2];
    uint32_t numEntidadesActualesMundo = 0;
    uint32_t idCliente = 0;
    uint32_t contadorSecuencia = 0;
    char buffer[TAM_MTU];


    // iniciamos la movida del tick

    LARGE_INTEGER frecuencia;
    LARGE_INTEGER tiempo_ultimo_tick_qpc, tiempo_actual_qpc;

    QueryPerformanceFrequency(&frecuencia);
    QueryPerformanceCounter(&tiempo_ultimo_tick_qpc); // Tiempo inicial del primer tick


    // --- NUEVO: Variables para la lógica de conexión robusta ---
    LARGE_INTEGER tiempo_inicio_espera_conexion;
    int intentos_conexion = 0;
    const int MAX_INTENTOS_CONEXION = 5;
    const long long TIMEOUT_CONEXION_MICROSEGUNDOS = 2000000LL; // 2 segundos



    int bytes_recibidos;
    int running = 1;
    int intentos_desconexion = 0;
    while(running == 1){

        if( estadoCliente != CLIENTE_DESCONECTANDO  && debe_cerrar_ventana() ){
            estadoCliente = CLIENTE_DESCONECTANDO ;
        }

        switch(estadoCliente){

            case CLIENTE_CONECTADO: // EN CADA TICK ENVIAMOS UN MOVIMIENTO (A) Y RECIBIMOS EL ESTADO DEL MUNDO (B)

                QueryPerformanceCounter(&tiempo_actual_qpc);

                long long microsegundos_transcurridos_desde_ultimo_tick = (tiempo_actual_qpc.QuadPart - tiempo_ultimo_tick_qpc.QuadPart) * 1000000LL / frecuencia.QuadPart;
                long long intervalo_objetivo_microsegundos = 1000000LL / TICK_RATE; // Intervalo deseado en microsegundos

                if (microsegundos_transcurridos_desde_ultimo_tick >= intervalo_objetivo_microsegundos)
                 {
                    //A.1. Miramos si el servidor nos ha mandado algun estado del mundo

                        // --- INICIO

                        // Buffer para guardar temporalmente el último paquete de estado de juego válido que recibamos.
                        char ultimoPaqueteBuffer[sizeof(PaqueteEstadoJuego)]; 
                        bool seRecibioUnPaqueteValido = false;

                        // Bucle para recibir todos los datagramas pendientes en la cola del socket.
                        while (true) {
                            char buffer[TAM_MTU]; // Buffer para la recepción de cada paquete individual
                            int bytes_recibidos;
                            int server_addr_len = sizeof(direccion_servidor);

                            bytes_recibidos = recvfrom(socket_cliente, buffer, TAM_MTU, 0, (struct sockaddr *)&direccion_servidor, &server_addr_len);

                            if (bytes_recibidos > 0) {
                                // Recibimos un paquete. Verificamos si es un estado de juego válido.
                                CabeceraRUDP* cabecera = (CabeceraRUDP*)buffer;

                                if (cabecera->tipoPaquete == PACKET_TYPE_ESTADO_JUEGO && bytes_recibidos == sizeof(PaqueteEstadoJuego)) {
                                    // Es un paquete de estado de juego válido. Guardamos su contenido.
                                    // Si llegan más, este se sobrescribirá, lo cual es exactamente lo que queremos.
                                    memcpy(ultimoPaqueteBuffer, buffer, sizeof(PaqueteEstadoJuego));
                                    seRecibioUnPaqueteValido = true;
                                }
                                // Si no es un paquete de estado de juego, simplemente lo ignoramos y continuamos el bucle
                                // para seguir vaciando la cola.

                            } else if (bytes_recibidos == SOCKET_ERROR) {
                                int error = WSAGetLastError();
                                if (error == WSAEWOULDBLOCK) {
                                    // ¡Esto es lo esperado! Significa que no hay más mensajes en la cola.
                                    // Rompemos el bucle para proceder a procesar el último paquete que guardamos.
                                    break; 
                                } else {
                                    // Ocurrió un error de red real.
                                    // Puedes imprimirlo para depurar si es necesario.
                                    // printf("recvfrom falló con error: %d\n", error);
                                    break;
                                }
                            } else {
                                // recvfrom devolvió 0 o un valor inesperado. La conexión podría estar cerrada.
                                break;
                            }
                        }

                        // Después del bucle, si hemos recibido al menos un paquete válido, procesamos el último.
                        if (seRecibioUnPaqueteValido) {
                            // Apuntamos al buffer donde guardamos el último paquete.
                            PaqueteEstadoJuego* paqueteEstadoJuego = (PaqueteEstadoJuego*)ultimoPaqueteBuffer;
                            
                            // Actualizamos el estado del juego local con los datos del último paquete.
                            numEntidadesActualesMundo = paqueteEstadoJuego->num_entidades;

                            for (int i = 0; i < paqueteEstadoJuego->num_entidades; i++) {
                                entidades_mundo[i] = paqueteEstadoJuego->entidades[i];
                            }
                            
                            // Opcional: puedes poner un log para saber que se actualizó
                            // printf("Estado del juego actualizado con el paquete más reciente.\n");
                        }

                        // --- FIN 



                    Vector2D vec = actualizar_dibujo(entidades_mundo, numEntidadesActualesMundo, idCliente);

                    // Solo para testear, vamos a generar un movimiento en circulos
                    
                    // --- INICIO DEL NUEVO CÓDIGO DE TESTEO ---
                if(bit) {
                    // 1. Definimos los parámetros de la "jaula" invisible
                    const Vector2D PUNTO_CENTRAL = {0.0f, 0.0f}; // Centro del área de juego
                    
                    // ESTE ES EL VALOR CLAVE: el radio máximo.
                    // Lo pongo en 50.0f para que el efecto sea visible.
                    // ¡Cámbialo a 3.0f para cumplir tu requisito exacto!
                    const float RADIO_MAXIMO = 2.0f; 

                    // 2. Buscamos la posición actual de nuestro jugador
                    Vector2D pos_actual = {0,0};
                    bool entidad_encontrada = false;
                    for (int i = 0; i < numEntidadesActualesMundo; i++) {
                        if (entidades_mundo[i].id == idCliente) {
                            pos_actual = entidades_mundo[i].pos;
                            entidad_encontrada = true;
                            break;
                        }
                    }

                    if (entidad_encontrada) {
                        // 3. Calculamos el vector desde el centro hasta nuestra posición actual
                        Vector2D vector_desde_centro;
                        vector_desde_centro.x = pos_actual.x - PUNTO_CENTRAL.x;
                        vector_desde_centro.y = pos_actual.y - PUNTO_CENTRAL.y;

                        // 4. Calculamos la distancia al centro
                        float distancia_al_centro = sqrtf(vector_desde_centro.x * vector_desde_centro.x + vector_desde_centro.y * vector_desde_centro.y);

                        // 5. LÓGICA PRINCIPAL: ¿Estamos fuera o dentro del radio máximo?
                        if (distancia_al_centro > RADIO_MAXIMO) {
                            // ESTAMOS FUERA: La única dirección posible es de vuelta al centro.
                            // El vector hacia el centro es el inverso de `vector_desde_centro`.
                            vec.x = -vector_desde_centro.x;
                            vec.y = -vector_desde_centro.y;
                            
                        } else {
                            // ESTAMOS DENTRO: Nos movemos en una dirección tangencial (perpendicular)
                            // para que parezca que nos movemos por el interior.
                            // Un vector perpendicular a (x, y) es (-y, x).
                            vec.x = -vector_desde_centro.y;
                            vec.y =  vector_desde_centro.x;
                        }

                        // 6. Normalizamos el vector final para que solo represente una dirección pura.
                        // Esto es crucial para que el servidor aplique la velocidad correctamente.
                        float magnitud = sqrtf(vec.x * vec.x + vec.y * vec.y);
                        if (magnitud > 0.001f) { // Evitamos la división por cero si estamos justo en el centro
                            vec.x /= magnitud;
                            vec.y /= magnitud;
                        } else {
                            // Si estamos en el centro exacto, damos un empujón inicial para empezar a movernos.
                            vec.x = 1.0f;
                            vec.y = 0.0f;
                        }
                                // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
                                // 7. APLICAMOS EL MODIFICADOR DE VELOCIDAD (¡LA SOLUCIÓN!)
                                // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
                                // Define qué tan lento quieres que vaya. 0.5f es la mitad de rápido.
                                const float factor_velocidad = 0.1f; 

                                // Multiplicamos el vector de dirección por nuestro factor.
                                vec.x *= factor_velocidad;
                                vec.y *= factor_velocidad;

                    }

                    
                }
                   // --- FIN DEL NUEVO CÓDIGO DE TESTEO ---


                    //B.2. Enviamos el vector al servidor
                    if(vec.x == 0 && vec.y == 0){ // Si no hay movimiento, no enviamos nada.
                        break;
                    }
                    printf("Enviando Vector: %f, %f\n", vec.x, vec.y);
                    PaqueteMovimiento paqueteMovimiento;
                    paqueteMovimiento.header.numero_secuencia = 0;
                    paqueteMovimiento.header.tipoPaquete = PACKET_TYPE_MOVIMIENTO;
                    paqueteMovimiento.idCliente = idCliente;
                    paqueteMovimiento.dir_nueva = vec;

                    sendto(socket_cliente,  (char*) &paqueteMovimiento, sizeof(paqueteMovimiento), 0, (struct sockaddr *)&direccion_servidor, sizeof(direccion_servidor));

                    tiempo_ultimo_tick_qpc = tiempo_actual_qpc; // Actualiza el tiempo del ultimo tick
                }

                break;

            case CLIENTE_SOLICITANDO_CONEXION:
                if(intentos_conexion >= MAX_INTENTOS_CONEXION) {
                    printHora("Se supero el numero maximo de intentos de conexion. Abortando.");
                    running = 0;
                    break;
                }
                intentos_conexion++;
                printf("Enviando solicitud de union... (Intento %d/%d)\n", intentos_conexion, MAX_INTENTOS_CONEXION);
                
                PaqueteUnirse paqueteUnirse;
                paqueteUnirse.header.numero_secuencia = 0;
                paqueteUnirse.header.tipoPaquete = PACKET_TYPE_UNIRSE;
                sendto(socket_cliente, (char *)&paqueteUnirse, sizeof(paqueteUnirse), 0, (struct sockaddr *)&direccion_servidor, sizeof(direccion_servidor));
                
                QueryPerformanceCounter(&tiempo_inicio_espera_conexion);
                
                estadoCliente = CLIENTE_ESPERANDO_RESPUESTA;
                break;
             case CLIENTE_ESPERANDO_RESPUESTA:
                // 1. Comprobar si ha expirado el tiempo de espera (timeout)
                QueryPerformanceCounter(&tiempo_actual_qpc);
                long long microsegundos_esperando = (tiempo_actual_qpc.QuadPart - tiempo_inicio_espera_conexion.QuadPart) * 1000000LL / frecuencia.QuadPart;

                if (microsegundos_esperando > TIMEOUT_CONEXION_MICROSEGUNDOS) {
                    printHora("Timeout esperando respuesta del servidor. Reintentando...");
                    estadoCliente = CLIENTE_SOLICITANDO_CONEXION; // Volver al estado anterior para reenviar
                    break; // Salir del switch para que el próximo ciclo reenvíe la solicitud
                }

                // 2. Si no hay timeout, intentar leer del socket
                // Usamos un bucle para procesar todos los paquetes que hayan llegado.
                bool conexion_resuelta = false;
                while (!conexion_resuelta) {
                    int tam_direccion_servidor = sizeof(direccion_servidor);
                    bytes_recibidos = recvfrom(socket_cliente, buffer, TAM_MTU, 0, (struct sockaddr *)&direccion_servidor, &tam_direccion_servidor);

                    if (bytes_recibidos > 0) {
                        CabeceraRUDP* cabecera = (CabeceraRUDP*)buffer;

                        if (cabecera->tipoPaquete == PACKET_TYPE_UNIDO_OK) {
                            // if(debugger <= 3){
                            //     printHora("SIMULANDO PAQUETE UNIDO_OK PERDIDO");
                            //     debugger++;
                            //     continue;
                            // }
                            printHora("¡Servidor nos ha aceptado!");
                            PaqueteUnirseAceptado* paqueteAceptado = (PaqueteUnirseAceptado*)buffer;
                            idCliente = paqueteAceptado->id;
                            estadoCliente = CLIENTE_CONECTADO;
                            conexion_resuelta = true; // Salimos del bucle interno y del switch
                        } else if (cabecera->tipoPaquete == PACKET_TYPE_UNIDO_RECHAZADO) {
                            printHora("Servidor nos ha rechazado la conexion.");
                            running = 0;
                            conexion_resuelta = true; // Salimos para terminar el programa
                        } else {
                            // Ignoramos los paquetes que no sean PACKET_TYPE_UNIDO_RECHAZADO o PACKET_TYPE_UNIDO_OK
                            printf("Recibido paquete inesperado (tipo %d) mientras se esperaba conexion. Ignorando.\n", cabecera->tipoPaquete);
                        }
                    } else if (bytes_recibidos == SOCKET_ERROR) {
                        int error = WSAGetLastError();
                        if (error == WSAEWOULDBLOCK) {
                            // No hay más datos por ahora. Salimos del bucle interno y esperamos
                            // a la siguiente iteración del bucle principal (while(running)).
                            break;
                        } else {
                            printf("recvfrom fallo con error critico: %d\n", error);
                            running = 0;
                            conexion_resuelta = true;
                        }
                    } else {
                        // recvfrom devolvió 0.
                        break;
                    }
                } // Fin del bucle while (!conexion_resuelta)
                break; // Salir del switch

            case CLIENTE_DESCONECTANDO:
            
                if(intentos_desconexion > 10) {
                    running = 0;
                    break;
                }
                // 1. Escuchar por un ACK del servidor
                bytes_recibidos = recvfrom(socket_cliente, buffer, TAM_MTU, 0, NULL, NULL);

                    while (bytes_recibidos > 0) {
                        //1. Compruebo que cabecera hemos recibido. ¡ Necesitamos = PACKET_TYPE_DESCONECTAR_ACK !
                        CabeceraRUDP cabecera;
                        memcpy(&cabecera, buffer, sizeof(CabeceraRUDP));
                        if( cabecera.tipoPaquete == PACKET_TYPE_DESCONECTAR_ACK ){
                            printHora("Desconectado");
                            running = 0;
                            break;
                        } else {
                            //2. Si la cabecera no es un PACKET_TYPE_DESCONECTAR_ACK, volvemos a leer de nuevo hasta que lo sea o este vacia.
                            bytes_recibidos = recvfrom(socket_cliente, buffer, TAM_MTU, 0, NULL, NULL);
                        }

                    }
                Sleep(1000);
            
                // 2. Reenviamos el paquete de desconectar
                PaqueteDesconectar paqueteDesconectar;
                paqueteDesconectar.header.numero_secuencia = contadorSecuencia; // Reenviamos con el mismo número
                paqueteDesconectar.header.tipoPaquete = PACKET_TYPE_DESCONECTAR;
                paqueteDesconectar.id = idCliente;

                sendto(socket_cliente, (char *)&paqueteDesconectar, sizeof(paqueteDesconectar), 0, (struct sockaddr *)&direccion_servidor, sizeof(direccion_servidor));
                
                printf("FInalizando intento de desconectar... (%d)\n", intentos_desconexion);
                intentos_desconexion++;
            break;    




        }

    }



    printHora("Desconectado");
    closesocket(socket_cliente);
    WSACleanup();
    cerrar_dibujo();

    return 0;
}