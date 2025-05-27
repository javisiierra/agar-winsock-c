#include <stdio.h>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#include "mensajes.h"
#include <time.h>   


#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <windows.h>

#include <stdlib.h>
#define MAX_ENTIDADES 1000
HANDLE eventoEnviarActualizacion;




typedef struct {
    Entidad entidades[MAX_ENTIDADES];
    int num_entidades;
    CRITICAL_SECTION mutex;
} ArrayEntidadesConMutex;

ArrayEntidadesConMutex arrayEntidadesJugadores;
ArrayEntidadesConMutex arrayEntidadesComidas;


// metodo para inicializar ArrayEntidadesConMutex
void inicializarArrayEntidadesConMutex(ArrayEntidadesConMutex* array) {
    InitializeCriticalSection(&array->mutex);
    array->num_entidades = 0;
}


// metodo que recorre todo el array de entidades y devuelve la entidad que tenga el id y el tipo especificado
Entidad* getArrayEntidadesConMutex(ArrayEntidadesConMutex* array, int id) {
    EnterCriticalSection(&array->mutex); // cierro
    for (int i = 0; i < array->num_entidades; i++) {
        if (array->entidades[i].id == id) {
            LeaveCriticalSection(&array->mutex); // libero
            return &array->entidades[i];
        }
    }
    LeaveCriticalSection(&array->mutex); // abro
    return NULL;
}


// metodo que borra un elemento de un array de entidades
void removeArrayEntidadesConMutex(ArrayEntidadesConMutex* array, int id) {
    EnterCriticalSection(&array->mutex); // cerrarr
    for (int i = 0; i < array->num_entidades; i++) {
        if (array->entidades[i].id == id) {
            for (int j = i; j < array->num_entidades - 1; j++) {
                array->entidades[j] = array->entidades[j + 1];
            }
            array->num_entidades--;
            LeaveCriticalSection(&array->mutex); // abrir
            return;
        }
    }
    LeaveCriticalSection(&array->mutex); // abrir
}


typedef struct {
    Entidad entidad;
    CRITICAL_SECTION mutex;
} SafeEntidad;



typedef struct {
    int client_id;         // Identificador único del cliente
    Vector2D dir_nueva;
} MensajeRecibido;

typedef struct {
    SOCKET cliente;
    struct sockaddr_in direccion;
} Cliente_info;

typedef struct Node {
    MensajeRecibido msg;
    struct Node* next;
} Node;

typedef struct {
    Node* head;
    Node* tail;
    CRITICAL_SECTION mutex;
    CONDITION_VARIABLE cond; // Opcional, para notificar al Game Master
} ThreadSafeQueue;



SafeEntidad entidades[MAX_ENTIDADES];
int contador_entidades = 0; 


volatile int running = 1; // bandera global para controlar el bucle

DWORD WINAPI manejar_cliente(LPVOID arg);
DWORD WINAPI bucle_aceptar_solicitudes(LPVOID arg);
void process_game_tick();
int queue_dequeue(ThreadSafeQueue* q, MensajeRecibido* out_msg);
void queue_init(ThreadSafeQueue* q);
void queue_enqueue(ThreadSafeQueue* q, MensajeRecibido msg);
Entidad* buscar_entidad(TipoEntidad tipo, int id);
int agregar_entidad(TipoEntidad tipo, int id, float x, float y);



void printHora(char *mensaje) {
    SYSTEMTIME st;
    GetSystemTime(&st);
    printf("[%02d:%02d:%02d.%03d - %d] %s\n",
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, GetCurrentThreadId(), mensaje);
}


int addArrayEntidadesConMutex(ArrayEntidadesConMutex* array, Entidad entidad) {
    // se verrifica si hay espacio en el array
    EnterCriticalSection(&array->mutex);
    if (array->num_entidades >= MAX_ENTIDADES) {
        LeaveCriticalSection(&array->mutex);
        printHora("Array de entidades lleno");
        return -1;
    }
    
    array->entidades[array->num_entidades] = entidad;
    array->num_entidades++;
    LeaveCriticalSection(&array->mutex);
    return 0;
}



int num_clientes = 0;
// cliente_info clientes[10];


#define TICK_RATE 1 // ticks por segundo
#define TICK_INTERVAL_MS (1000 / TICK_RATE) // Intervalo en MILISEGUNDOS



// Declaración global de la cola segura para hilos
ThreadSafeQueue colaMensajes;
int main(){

    WSADATA wsa;
    eventoEnviarActualizacion = CreateEvent(NULL, TRUE, FALSE, NULL);
     inicializarArrayEntidadesConMutex(&arrayEntidadesJugadores);
     //inicializarArrayEntidadesConMutex(&arrayEntidadesComidas);

    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        printHora("Error inicializando Winsock");
        return 1;
    }



    SOCKET servidor_sock;

    servidor_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (servidor_sock == INVALID_SOCKET) {
        printHora("Error creando el socket");
        WSACleanup();
        return 1;
    }

    struct sockaddr_in addr;

    // typedef struct sockaddr_in {
    //     short          sin_family;
    //     u_short        sin_port;
    //     struct in_addr sin_addr;
    //     char           sin_zero[8];
    //   } SOCKADDR_IN, *PSOCKADDR_IN, *LPSOCKADDR_IN;

    addr.sin_family = AF_INET;

        addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // cualquier IP


    addr.sin_port = htons(8080);


    // bind() asocia el socket con una dirección y un puerto
    if(bind(servidor_sock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printHora("Error en bind");
        closesocket(servidor_sock);
        WSACleanup();
        return 1;
    }



        if(listen(servidor_sock, 1) == SOCKET_ERROR) {
            printHora("Error en listen");
            closesocket(servidor_sock);
            WSACleanup();
            return 1;
        }

    printHora("Servidor escuchando en el puerto 8080");

    // Creamos un hilo encargado de ir aceptando las solicitudes:
    HANDLE hilo_aceptador = CreateThread(
        NULL,                   // Atributos de seguridad por defecto
        0,                      // Tamaño de pila por defecto
        bucle_aceptar_solicitudes,        // Función del hilo
        (LPVOID)servidor_sock,  // Parámetros para la función
        0,                      // Banderas de creación por defecto
        NULL                    // No necesitamos el ID del hilo
    );

    if(hilo_aceptador == NULL){
        printHora("Error creando hilo");
        closesocket(servidor_sock);
        WSACleanup();
        return 1;
    }

    CloseHandle(hilo_aceptador);

    // // Esperamos a que el hilo termine:
    // WaitForSingleObject(hilo_aceptador, INFINITE);


    // Desde aqui nos encargaremos de la logica principal del juego.
    // Tareas: inicializar un reloj, a una frecuencia de 50 hz. Este sera nuestro tick
    // IA, hazlo



            LARGE_INTEGER frequency;
            LARGE_INTEGER last_tick_time_qpc, current_tick_time_qpc;

            QueryPerformanceFrequency(&frequency);      // Obtiene la frecuencia del contador
            QueryPerformanceCounter(&last_tick_time_qpc); // Obtiene el tiempo inicial

            queue_init(&colaMensajes);
            while (running) {
                process_game_tick();

                QueryPerformanceCounter(&current_tick_time_qpc);

                // Calcula el tiempo transcurrido en microsegundos para mantener la precisión interna
                long long elapsed_microseconds = (current_tick_time_qpc.QuadPart - last_tick_time_qpc.QuadPart) * 1000000LL / frequency.QuadPart;

                long long target_interval_microseconds = 1000000LL / TICK_RATE; // Intervalo deseado en microsegundos
                long long sleep_microseconds = target_interval_microseconds - elapsed_microseconds;

                if (sleep_microseconds > 0) {
                    // Sleep() toma milisegundos. Convertimos y manejamos.
                    DWORD sleep_ms = (DWORD)(sleep_microseconds / 1000);
                    if (sleep_ms > 0) {
                        Sleep(sleep_ms); // Sleep toma milisegundos
                    } else {
                        // Si el tiempo de sleep es muy pequeño (menos de 1ms),
                        // un Sleep(0) cede el quantum de tiempo al SO, lo que es bueno.
                        Sleep(0);
                    }
                }
                // Actualiza last_tick_time_qpc para la siguiente iteración
                // Podrías hacer last_tick_time_qpc = current_tick_time_qpc;
                // o para evitar deriva: last_tick_time_qpc.QuadPart += (target_interval_microseconds * frequency.QuadPart / 1000000LL);
                // Pero para simplificar y dado que calculamos el sleep basado en el tiempo real transcurrido:
                QueryPerformanceCounter(&last_tick_time_qpc);
            }




    return 0;
}


DWORD WINAPI manejar_cliente(LPVOID arg){

    Cliente_info info = *(Cliente_info*)arg;
    SOCKET cliente_sock = info.cliente;
    struct sockaddr_in cliente_direccion = info.direccion;

    char buffer[1024];
    int bytes_recibidos;

    SafeEntidad* entidad;




    // PRIMERA CONEXION DEL CLIENTE
    int mensaje_recibido;

    bytes_recibidos = recv(cliente_sock, (char*)&mensaje_recibido, sizeof(mensaje_recibido), 0);
    // si recibimos algo que no sea un 1, dejamos de atender al cliente
    if(mensaje_recibido != UNIRSE_A_PARTIDA){
        printHora("Mensaje del cliente desconocido, no se puede atender");
        closesocket(cliente_sock);
        return 0;
    } else {
        printHora("Unirse a partida recibido");
        printf("Se le ha asignado el thread con id %d\n", GetCurrentThreadId());
        Entidad entidad_jugador;
        entidad_jugador.id = GetCurrentThreadId();
        entidad_jugador.pos.x = 0;
        entidad_jugador.pos.y = 0;
        entidad_jugador.dir.x = 0;
        entidad_jugador.dir.y = 0;

        if (addArrayEntidadesConMutex(&arrayEntidadesJugadores, entidad_jugador) != 0) {
            printHora("No se ha podido agregar una nueva entidad al array");
            closesocket(cliente_sock);
            return 0;
        }

        // Le enviamos al cliente su ID asignado.
        if (send(cliente_sock, (char*)&entidad_jugador.id, sizeof(entidad_jugador.id), 0) == SOCKET_ERROR) {
            printHora("Error al enviar el ID del jugador al cliente");
            closesocket(cliente_sock);
            return 0;
        } else {
            char debug_msg[256];
            sprintf(debug_msg, "ID %d enviado al cliente", entidad_jugador.id);
            printHora(debug_msg);
        }

   

    }


    while(1){ // En este bucle se atiende la partida del jugador

        Vector2D vec;
        
        // recibimos el vector de direccion del jugador
        bytes_recibidos = recv(cliente_sock, (char*)&vec, sizeof(vec), 0);


        printf("bytes_recibidos: %d\n", bytes_recibidos);

        if(bytes_recibidos == SOCKET_ERROR) {
            printHora("Error en recv");
            break;
        }

        if(bytes_recibidos > 0){


            char mensaje[1024];

            printHora(mensaje);


            // XXX
            MensajeRecibido msg;
            msg.client_id = GetCurrentThreadId(); // el id sera el identificador del thread (de momento thread ID = ID DEL JUGADOR)
            msg.dir_nueva = vec;
            queue_enqueue(&colaMensajes, msg);
            // XXX


            WaitForSingleObject(eventoEnviarActualizacion, INFINITE);

            send(cliente_sock, (char*)arrayEntidadesJugadores.entidades, arrayEntidadesJugadores.num_entidades * sizeof(Entidad), 0);


        } else if(bytes_recibidos == 0){
            printHora("Cliente desconectado");
            closesocket(cliente_sock);
            break;
        } else {
            int error = WSAGetLastError();

            char error_str[256];
            sprintf(error_str, "Error en recv: %d", error);
            printHora(error_str);

            closesocket(cliente_sock);
            break;
        }


    }


    printHora("Cliente desconectado");
    closesocket(cliente_sock);
    //free(info);  // Liberar la memoria de los parámetros MMMMMMMMMMMMMMAAAAAAAAAAAAAAAAALLLLLLLLLL. El contenido esta en la pila, no en el heap. Asi que al liberar en la pila se lia.
    return 0;
}

DWORD WINAPI bucle_aceptar_solicitudes(LPVOID arg){

    SOCKET servidor_sock = (SOCKET)arg;

    while(1){

        Cliente_info cliente_inf;
        SOCKET cliente_sock;
        struct sockaddr_in cliente_addr;
        int cliente_addr_len = sizeof(cliente_addr);

        cliente_sock = accept(servidor_sock, (struct sockaddr*)&cliente_addr, &cliente_addr_len);

        if(cliente_sock == INVALID_SOCKET) {
            printHora("Error en accept");
            continue;
        }

        if(num_clientes >= 10){
            printHora("Demasiados clientes");
            closesocket(cliente_sock);
            continue;
        }
        printHora("Cliente conectado");

        cliente_inf.cliente = cliente_sock;
        cliente_inf.direccion = cliente_addr;

        // clientes[num_clientes] = cliente_inf;
        num_clientes++;


            // Crear el hilo
            // HANDLE hilo:
                // Es el identificador que Windows asigna al hilo recién creado.
                // Se obtiene como valor de retorno de CreateThread().  
                // Se usa para controlar el hilo (esperar a que termine, cambiar su 
                // prioridad, etc.).
        HANDLE hilo = CreateThread(
            NULL,                   // Atributos de seguridad por defecto
            0,                      // Tamaño de pila por defecto
            manejar_cliente,        // Función del hilo
            (LPVOID)&cliente_inf,                 // Parámetros para la función
            0,                      // Banderas de creación por defecto
            NULL                    // No necesitamos el ID del hilo
        );

        if(hilo == NULL){
            printHora("Error creando hilo");
            closesocket(cliente_sock);
            num_clientes--;
            continue;
        }


        // CloseHandle(hilo):
            // No termina el hilo, solo libera la referencia al mismo.
            // El hilo continúa ejecutándose normalmente.
            // Es necesario para evitar fugas de recursos del sistema.
            // Después de cerrar el handle, ya no podrás usar ese handle para
            // interactuar con el hilo.
        CloseHandle(hilo);
    }


}

void process_game_tick() {
    // Aquí va la lógica del juego a ejecutar en cada tick
    printHora("Inicio TICK");
    MensajeRecibido msg;
    char buffer[256];


    while(queue_dequeue(&colaMensajes, &msg)){

        printf("-\n");
        sprintf(buffer, "Cliente %d envia el vector: %f, %f", msg.client_id, msg.dir_nueva.x, msg.dir_nueva.y);
        printHora(buffer);
        // recorremos el array de entidades y actualizamos la direccion de las entidades
        
        Entidad *ent = getArrayEntidadesConMutex(&arrayEntidadesJugadores, msg.client_id); // >>> ESTO NO GARANTIZA LA EXCLUSION MUTUA. Puedo 
        // usar entidad despues de haber liberado el mutex porque tengo un puntero.


        if(ent != NULL){
            ent->dir = msg.dir_nueva;
        } else{
            sprintf(buffer, "No se ha encontrado la entidad con id %d", msg.client_id);
            printHora(buffer);
        }

        // actualizamos la posicion de las entidades
        for(int i = 0; i < arrayEntidadesJugadores.num_entidades; i++){
            Entidad* ent = &arrayEntidadesJugadores.entidades[i];
            ent->pos.x += ent->dir.x;
            ent->pos.y += ent->dir.y;
        }

    }

    SetEvent(eventoEnviarActualizacion);
    
    // Restablecer para el siguiente tick
    ResetEvent(eventoEnviarActualizacion);
    printHora("Fin TICK");
}

void queue_init(ThreadSafeQueue* q) {
    q->head = q->tail = NULL;
    InitializeCriticalSection(&q->mutex);
    InitializeConditionVariable(&q->cond);
}

void queue_enqueue(ThreadSafeQueue* q, MensajeRecibido msg) {
    printf("+\n");
    Node* node = malloc(sizeof(Node));
    node->msg = msg;
    node->next = NULL;

    EnterCriticalSection(&q->mutex);
    if (q->tail) {
        q->tail->next = node;
        q->tail = node;
    } else {
        q->head = q->tail = node;
    }
    LeaveCriticalSection(&q->mutex);
    WakeConditionVariable(&q->cond);
}

// Devuelve 1 si hay mensaje, 0 si la cola está vacía
int queue_dequeue(ThreadSafeQueue* q, MensajeRecibido* out_msg) {
    EnterCriticalSection(&q->mutex);
    if (!q->head) {
        LeaveCriticalSection(&q->mutex);
        return 0;
    }
    Node* node = q->head;
    *out_msg = node->msg;
    q->head = node->next;
    if (!q->head) q->tail = NULL;
    
    LeaveCriticalSection(&q->mutex);
    free(node); // CONDICION DE CARRERA?¿?
    return 1;
}



void queue_destroy(ThreadSafeQueue* q) {
    // Primero, vaciar la cola y liberar todos los nodos
    Node* current = q->head;
    Node* next_node;
    while (current != NULL) {
        next_node = current->next;
        free(current);
        current = next_node;
    }
    q->head = q->tail = NULL;

    DeleteCriticalSection(&q->mutex);

}




