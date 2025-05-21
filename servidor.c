#include <stdio.h>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#include "mensajes.h"
#include <time.h>   // Para clock_gettime (asumiendo que MinGW lo proporciona)
// #include <unistd.h> // Para usleep. Considerar Sleep() de Windows si usleep no está disponible.
// Si usleep no está en tu <unistd.h> de MinGW, puedes usar Sleep() de <windows.h>
// Sleep() toma milisegundos. Para microsegundos, necesitarías algo más o aceptar la granularidad de ms.

// Definir la versión mínima de Windows ANTES de incluir windows.h
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <windows.h>

#include <stdlib.h>
#define MAX_ENTIDADES 1000

typedef enum {
    ENTIDAD_JUGADOR,
    ENTIDAD_COMIDA,
} TipoEntidad;



typedef struct {
    int id;           // Identificador único de la entidad
    TipoEntidad tipo; // Tipo de entidad (p.ej. 0: jugador, 1: comida, 2: virus, etc.)
    Vector2D pos;      // Coordenadas de la entidad
    Vector2D dir;      // Dirección de movimiento de la entidad
} Entidad;


typedef struct {
    Entidad entidad;
    CRITICAL_SECTION mutex;
} SafeEntidad;



typedef struct {
    int client_id;         // Identificador único del cliente
    char data[256];        // Buffer con los datos o comando recibido
    Vector2D dir_nueva;
    
    // int command_type;
    // int data_length;
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


int num_clientes = 0;
// cliente_info clientes[10];


#define TICK_RATE 1 // ticks por segundo
#define TICK_INTERVAL_MS (1000 / TICK_RATE) // Intervalo en MILISEGUNDOS



// Declaración global de la cola segura para hilos
ThreadSafeQueue queue;
int main(){

    WSADATA wsa;

    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        printHora("Error inicializando Winsock");
        return 1;
    }

    // ahora tengo que crear el socket

    SOCKET servidor_sock;
    // ------------------------------
    // Creación de un socket en Winsock
    // ------------------------------

    // La función socket() se utiliza para crear un nuevo socket de red.
    // Su prototipo es: SOCKET socket(int af, int type, int protocol);
    //
    // Parámetros:
    //   1. af (Address Family): Especifica la familia de direcciones a utilizar.
    //      - AF_INET: Indica que se usará la familia de direcciones IPv4.
    //
    //   2. type (Tipo de socket): Define el comportamiento del canal de comunicación.
    //      - SOCK_STREAM: Crea un socket de tipo flujo, que transmite datos como una
    //        secuencia continua de bytes (usado por TCP). Garantiza entrega ordenada,
    //        confiable y sin duplicados.
    //      - SOCK_DGRAM: Crea un socket de tipo datagrama, que transmite datos en
    //        paquetes independientes llamados datagramas (usado por UDP). No garantiza
    //        entrega ni orden.
    //
    //   3. protocol (Protocolo): Especifica el protocolo concreto a utilizar.
    //      - IPPROTO_TCP: Protocolo TCP (usualmente se usa con SOCK_STREAM).
    //      - IPPROTO_UDP: Protocolo UDP (usualmente se usa con SOCK_DGRAM).
    //      - 0: Permite que el sistema seleccione automáticamente el protocolo adecuado
    //        según la combinación de familia y tipo de socket. Por ejemplo, si se elige
    //        AF_INET y SOCK_STREAM, el sistema seleccionará TCP.
    //
    // Notas importantes:
    //   - El segundo parámetro (type) determina si el socket usará un flujo de datos
    //     confiable y orientado a conexión (TCP/segmentos) o datagramas independientes (UDP).
    //   - El tercer parámetro (protocol) especifica el protocolo exacto, pero normalmente
    //     se puede dejar en 0 para que el sistema elija el más apropiado.
    //   - Ambos parámetros afectan tanto el envío como la recepción de datos; no se dividen
    //     en "enviar" o "recibir".
    //
    // Valor de retorno:
    //   - Si la creación del socket es exitosa, se devuelve un descriptor de socket válido.
    //   - Si ocurre un error, se devuelve INVALID_SOCKET.
    //

    servidor_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (servidor_sock == INVALID_SOCKET) {
        printHora("Error creando el socket");
        WSACleanup();
        return 1;
    }

    struct sockaddr_in addr;
    // ----------------------------------------
    // Inicialización de la estructura sockaddr_in
    // ----------------------------------------

    // typedef struct sockaddr_in {
    //     short          sin_family;
    //     u_short        sin_port;
    //     struct in_addr sin_addr;
    //     char           sin_zero[8];
    //   } SOCKADDR_IN, *PSOCKADDR_IN, *LPSOCKADDR_IN;

    // sin_family: Especifica la familia de direcciones que se va a utilizar.
    //   - Debe establecerse en AF_INET para indicar que se usará IPv4.
    //   - Este valor es obligatorio para que las funciones de sockets
    // interpreten correctamente la estructura.
    // ALTERNATIVA A AF_INET: AF_UNSPEC, que significa "no especificada"; 
    // que significa que el socket puede ser usado para cualquier familia de direcciones.
    // Tambien se puede usar AF_INET6 para IPv6.
    addr.sin_family = AF_INET;

    // sin_addr.s_addr: Especifica la dirección IP a la que se asociará el socket.
    //   - inet_addr("192.168.1.1") convierte la dirección IP en formato texto
    //  a un valor binario de 32 bits en "network byte order" (orden de bytes de red).
    //   - El campo sin_addr es una estructura (struct in_addr) cuyo miembro
    // s_addr almacena la dirección IP.
    // La estructura in_addr de sin_addr es:
    //struct in_addr {
    //    union {
    //      struct {
    //        u_char s_b1;
    //        u_char s_b2;
    //        u_char s_b3;
    //        u_char s_b4;
    //      } S_un_b;
    //      struct {
    //        u_short s_w1;
    //        u_short s_w2;
    //      } S_un_w;
    //      u_long S_addr;
    //    } S_un;
    //  };
    //   - Si se desea aceptar conexiones en cualquier interfaz local,
    // se puede usar INADDR_ANY en lugar de una IP concreta.
        addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // cualquier IP



    // sin_port: Especifica el número de puerto en el que el socket 
    // escuchará o al que se conectará.
    //   - El puerto debe estar en "network byte order" (big endian), 
    // por eso se utiliza la función htons() ("host to network short").
    //   - Esto garantiza la portabilidad entre diferentes arquitecturas,
    //  ya que cada sistema puede almacenar los números en un orden de bytes distinto.
    //   - En este ejemplo, se está configurando el puerto 8080.
    addr.sin_port = htons(8080);

    // Nota adicional:
    // La estructura sockaddr_in se utiliza habitualmente para definir
    //  la dirección y el puerto de un socket IPv4 en operaciones como bind(),
    //  connect(), sendto(), etc.
    // Al pasar esta estructura a funciones de la API de sockets, se
    //  suele castear a (struct sockaddr*) porque las funciones esperan un puntero
    //  genérico a sockaddr.
    // Es recomendable inicializar el campo sin_zero a cero usando memset o bzero,
    //  aunque en la mayoría de los casos no es estrictamente necesario, ya que
    //  solo sirve de relleno para igualar el tamaño de struct sockaddr.



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

            queue_init(&queue);
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
    if(mensaje_recibido != 1){
        printHora("Mensaje del cliente desconocido, no se puede atender");
        closesocket(cliente_sock);
        return 0;
    }


    while(1){ // En este bucle se atiende la partida del jugador

        Vector2D vec;
        
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
            strncpy(msg.data, buffer, sizeof(msg.data) - 1);
            msg.data[sizeof(msg.data) - 1] = '\0';
            msg.client_id = GetCurrentThreadId(); // el id sera el identificador del thread
            msg.vec = vec;
            queue_enqueue(&queue, msg);
            // XXX

            send(cliente_sock, "Recibido", strlen("Recibido"), 0);


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


    while(queue_dequeue(&queue, &msg)){
        // sprintf(buffer, "Cliente %d envia el mensaje: %s", msg.client_id, msg.data);
        sprintf(buffer, "Cliente %d envia el vector: %f, %f", msg.client_id, msg.vec.x, msg.vec.y);
        printHora(buffer);
    }
    
    printHora("Fin TICK");
}

void queue_init(ThreadSafeQueue* q) {
    q->head = q->tail = NULL;
    InitializeCriticalSection(&q->mutex);
    InitializeConditionVariable(&q->cond);
}

void queue_enqueue(ThreadSafeQueue* q, MensajeRecibido msg) {
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
    // CONDITION_VARIABLE no tiene una función de "delete" explícita.
}




