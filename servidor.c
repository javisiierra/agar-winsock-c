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

// -- DECLARAMOS METODOS --

void recibir_mensaje(SOCKET* socket_servidor_ptr);

typedef struct
{
    Entidad entidades[MAX_ENTIDADES];
    int num_entidades;
    CRITICAL_SECTION mutex;
} ArrayEntidadesConMutex;


typedef struct
{
    uint32_t idCliente;
    struct sockaddr_in direccion;
    uint32_t ultima_secuencia_recibida;
    uint32_t esperando_secuencia;
    TipoPaquete ultimo_paquete_recibido;
} Cliente_info;

typedef struct {
uint32_t contadorID;
uint32_t numero_clientes;
Cliente_info clientes [MAX_CLIENTES];
} ArrayCliente_info;



ArrayEntidadesConMutex arrayEntidadesJugadores;
ArrayEntidadesConMutex arrayEntidadesComidas;

// metodo para inicializar ArrayEntidadesConMutex
void inicializarArrayEntidadesConMutex(ArrayEntidadesConMutex *array)
{
    InitializeCriticalSection(&array->mutex);
    array->num_entidades = 0;
}

// metodo que recorre todo el array de entidades y devuelve la entidad que tenga el id y el tipo especificado
Entidad *getArrayEntidadesConMutex(ArrayEntidadesConMutex *array, int id)
{
    EnterCriticalSection(&array->mutex); // cierro
    for (int i = 0; i < array->num_entidades; i++)
    {
        if (array->entidades[i].id == id)
        {
            LeaveCriticalSection(&array->mutex); // libero
            return &array->entidades[i];
        }
    }
    LeaveCriticalSection(&array->mutex); // abro
    return NULL;
}

// metodo que borra un elemento de un array de entidades
void removeArrayEntidadesConMutex(ArrayEntidadesConMutex *array, int id)
{
    EnterCriticalSection(&array->mutex); // cerrarr
    for (int i = 0; i < array->num_entidades; i++)
    {
        if (array->entidades[i].id == id)
        {
            for (int j = i; j < array->num_entidades - 1; j++)
            {
                array->entidades[j] = array->entidades[j + 1];
            }
            array->num_entidades--;
            LeaveCriticalSection(&array->mutex); // abrir
            return;
        }
    }
    LeaveCriticalSection(&array->mutex); // abrir
}

typedef struct
{
    Entidad entidad;
    CRITICAL_SECTION mutex;
} SafeEntidad;

typedef struct
{
    uint32_t idCliente; // Identificador único del cliente
    Vector2D dir_nueva;
} MensajeRecibido;


typedef struct { // una vez recibamos un paquete PACKET_TYPE_UNIRSE o PACKET_TYPE_DESCONECTAR, 
    // se agrega a la cola de mensajes pendientes de ACK, donde esperamos recibir un ACK del cliente
    uint32_t idCliente;
    TipoPaquete tipoPaquete; // Aqui pondremos o PACKET_TYPE_UNIRSE o PACKET_TYPE_DESCONECTAR
} PaquetesPendientesAck;





typedef struct Node
{
    MensajeRecibido msg;
    struct Node *next;
} Node;

typedef struct
{
    Node *head;
    Node *tail;
    CRITICAL_SECTION mutex;
    CONDITION_VARIABLE cond;
} ThreadSafeQueue;

SafeEntidad entidades[MAX_ENTIDADES];
int contador_entidades = 0;

volatile int running = 1; // bandera global para controlar el bucle

DWORD WINAPI bucle_aceptar_solicitudes(LPVOID arg);
void process_game_tick();
int queue_dequeue(ThreadSafeQueue *q, MensajeRecibido *out_msg);
void queue_init(ThreadSafeQueue *q);
void queue_enqueue(ThreadSafeQueue *q, MensajeRecibido msg);
Entidad *buscar_entidad(TipoEntidad tipo, int id);
int agregar_entidad(TipoEntidad tipo, int id, float x, float y);

void printHora(char *mensaje)
{
    SYSTEMTIME st;
    GetSystemTime(&st);
    printf("[%02d:%02d:%02d.%03d - %d] %s\n",
           st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, GetCurrentThreadId(), mensaje);
}

int addArrayEntidadesConMutex(ArrayEntidadesConMutex *array, Entidad entidad)
{
    // se verrifica si hay espacio en el array
    EnterCriticalSection(&array->mutex);
    if (array->num_entidades >= MAX_ENTIDADES)
    {
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


// Declaración global de la cola segura para hilos
ThreadSafeQueue colaMensajes;

SOCKET socket_servidor;
int main()
{

    WSADATA wsa;
    inicializarArrayEntidadesConMutex(&arrayEntidadesJugadores);
    // inicializarArrayEntidadesConMutex(&arrayEntidadesComidas);


    // 1. Inicializamos Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        printHora("Error inicializando Winsock");
        return 1;
    }
 
    // 2.  Creamos el socket del servidor, donde se recibiran todos los datos de los clientes.
    
    socket_servidor = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_servidor == INVALID_SOCKET)
    {
        printHora("Error creando el socket");
        WSACleanup();
        return 1;
    }

    // 2. Lo configuramos como no bloqueante.
    u_long non_blocking_mode = 1;
    if (ioctlsocket(socket_servidor, FIONBIO, &non_blocking_mode) == SOCKET_ERROR) {
        printHora("Error al configurar el socket como no bloqueante");
        closesocket(socket_servidor);
        WSACleanup();
        return 1;
    }


    // 3. Configuramos la direccion en la que el servidor va a estar escuchando. Configuramos el puerto en el que el servidor estara escuchando.
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    // addr.sin_addr.s_addr = inet_addr("192.168.1.129");
    
    addr.sin_port = htons(8080);

    // 4. Con bind() asociaremos la direccion de antes al socket.
    if (bind(socket_servidor, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        printHora("Error en bind");
        closesocket(socket_servidor);
        WSACleanup();
        return 1;
    }

    printHora("Servidor escuchando en el puerto 8080");

    // 5. Inicializamos la cola de mensajes
    queue_init(&colaMensajes);

    // 6. Inicializamos las variables que se encargar de hacer que se realize una accion cada tick.
    // 6. Mientras no estemos en el tick, estaremos atendiendo solocitudes.

    LARGE_INTEGER frecuencia;
    LARGE_INTEGER tiempo_ultimo_tick_qpc, tiempo_actual_qpc;

    QueryPerformanceFrequency(&frecuencia);
    QueryPerformanceCounter(&tiempo_ultimo_tick_qpc); // Tiempo inicial del primer tick

    while (running)
    {
        // recibo solicitudes
        recibir_mensaje(&socket_servidor);
        QueryPerformanceCounter(&tiempo_actual_qpc);

        // Calcula el tiempo transcurrido en microsegundos para mantener la precisión interna
        long long microsegundos_transcurridos_desde_ultimo_tick = (tiempo_actual_qpc.QuadPart - tiempo_ultimo_tick_qpc.QuadPart) * 1000000LL / frecuencia.QuadPart;
        long long intervalo_objetivo_microsegundos = 1000000LL / TICK_RATE; // Intervalo deseado en microsegundos

        // 7.  Si es hora del tick, ejecutamos la logica del juego
        if (microsegundos_transcurridos_desde_ultimo_tick >= intervalo_objetivo_microsegundos)
        {

            // Realiza acciones de tick
            process_game_tick();

            tiempo_ultimo_tick_qpc = tiempo_actual_qpc; // Actualiza el tiempo del ultimo tick
        } else {

            //sleep(1);

        }




    }

    closesocket(socket_servidor);
    WSACleanup();
    return 0;


}


int addCliente_info(ArrayCliente_info* array, uint32_t idCliente, struct sockaddr_in direccion, uint32_t secuencia) {
    if(array->numero_clientes >= MAX_CLIENTES) {
        return -1;
    }
    array->clientes[array->numero_clientes].idCliente = idCliente;
    array->clientes[array->numero_clientes].direccion = direccion;
    array->clientes[array->numero_clientes].ultima_secuencia_recibida = secuencia;
    array->numero_clientes++;

    return 0;
}


void deleteCliente_info(ArrayCliente_info* array, uint32_t idCliente) {

    int found_index = -1;

    // 1. Buscar el cliente por su ID en el array compactado
    for (int i = 0; i < array->numero_clientes; i++) {
        if (array->clientes[i].idCliente == idCliente) {
            found_index = i;
            break; // Cliente encontrado
        }
    }

    if (found_index != -1) {
        // 2. Cliente encontrado: Eliminar su entidad de juego
        removeArrayEntidadesConMutex(&arrayEntidadesJugadores, idCliente); // Asegúrate de que esto también use mutex
        
        // 3. Compactar el array: Mover los elementos posteriores un lugar hacia adelante
        for (int i = found_index; i < array->numero_clientes - 1; i++) {
            array->clientes[i] = array->clientes[i + 1];
        }

        // 4. Decrementar el contador de clientes activos
        array->numero_clientes--;

        printHora("Cliente desconectado y array compactado.");
    } else {
        printHora("Intento de eliminar cliente no encontrado (ID no válido o ya desconectado).");
    }

}

int getCliente_infoIndex(ArrayCliente_info* array, uint32_t idCliente) {
    for (int i = 0; i < array->numero_clientes; i++) {
        if (array->clientes[i].idCliente == idCliente) {
            return i;
        }
    }
    return -1;
}




ArrayCliente_info arrayClientes = {1,0};

void recibir_mensaje(SOCKET* socket_servidor_ptr) {

    SOCKET socket_servidor = *socket_servidor_ptr;

    char buffer[TAM_MTU];
    struct sockaddr_in direccion_cliente;
    int direccion_cliente_len = sizeof(direccion_cliente);

    int num_bytes = recvfrom(socket_servidor, buffer, TAM_MTU, 0, (struct sockaddr *)&direccion_cliente, &direccion_cliente_len);

    if(num_bytes < 0) {
        // printHora("No hay mensaje que procesar.");
        return;
    }

    CabeceraRUDP cabecera;
    memcpy(&cabecera, buffer, sizeof(CabeceraRUDP));
    TipoPaquete tipoPaquete = cabecera.tipoPaquete;


    switch(tipoPaquete){
        case PACKET_TYPE_UNIRSE:
            printHora("Solicitud de unirse recibida.");
            CabeceraRUDP cabeceraRespuesta;
            cabeceraRespuesta.numero_secuencia = cabecera.numero_secuencia + 1; // deberia ser un 2

            if(arrayClientes.numero_clientes >= MAX_CLIENTES){
                // rechazamos la solicitud
                cabeceraRespuesta.tipoPaquete = PACKET_TYPE_UNIDO_RECHAZADO;
                PaqueteUnirseRechazado paqueteUnirseRechazado = {cabeceraRespuesta};

                // le envio al cliente su mensaje
                sendto(socket_servidor, (char *) &paqueteUnirseRechazado, sizeof(paqueteUnirseRechazado), 0, (struct sockaddr *)&direccion_cliente, sizeof(direccion_cliente));
            } else {
            
                cabeceraRespuesta.tipoPaquete = PACKET_TYPE_UNIDO_OK;
                uint32_t idCliente = arrayClientes.contadorID++;
                addCliente_info(&arrayClientes, idCliente, direccion_cliente, cabecera.numero_secuencia);
                Entidad nuevaEntidadJugador = {idCliente, ENTIDAD_JUGADOR, {0, 0}, {0, 0}};
                addArrayEntidadesConMutex(&arrayEntidadesJugadores, nuevaEntidadJugador);
                PaqueteUnirseAceptado paqueteUnirseAceptado = {cabeceraRespuesta, idCliente};
                // le envio al cliente su mensaje
                sendto(socket_servidor,(char *) &paqueteUnirseAceptado, sizeof(paqueteUnirseAceptado), 0, (struct sockaddr *)&direccion_cliente, sizeof(direccion_cliente));

            }
            break;
        case PACKET_TYPE_UNIDO_OK:
            break;
        case PACKET_TYPE_UNIDO_RECHAZADO:
            break;
        case PACKET_TYPE_MOVIMIENTO:
            MensajeRecibido movimientoRecibido;
            PaqueteMovimiento paqueteMovimiento;
            memcpy(&paqueteMovimiento, buffer, sizeof(PaqueteMovimiento));
            if(paqueteMovimiento.dir_nueva.x == 0.0 && paqueteMovimiento.dir_nueva.y == 0.0 ) {
                // printHora("Movimiento recibido sin movimiento.");
                break;
            }
            printf("Movimiento recibido del cliente %d: %f, %f\n", paqueteMovimiento.idCliente, paqueteMovimiento.dir_nueva.x, paqueteMovimiento.dir_nueva.y);

            movimientoRecibido.idCliente = paqueteMovimiento.idCliente;
            movimientoRecibido.dir_nueva = paqueteMovimiento.dir_nueva;

            queue_enqueue(&colaMensajes, movimientoRecibido);
            // no devolvemos nada
            break;
        case PACKET_TYPE_ESTADO_JUEGO:
            break;
        case PACKET_TYPE_ACK:




            break;
        case PACKET_TYPE_PING:
            break;
        case PACKET_TYPE_DESCONECTAR:
            {
                PaqueteDesconectar paqueteDesconectar;
                memcpy(&paqueteDesconectar, buffer, sizeof(PaqueteDesconectar));
                

                printf("Solicitud de desconectar recibida para cliente ID: %d.", paqueteDesconectar.id);
      
                // 1. Busca si el cliente existe.
                int cliente_index = getCliente_infoIndex(&arrayClientes, paqueteDesconectar.id);

                if (cliente_index != -1) {
                    // ¡Cliente encontrado!  Lo borramos.
                    printHora("Cliente encontrado. Eliminando sus datos...");
                    deleteCliente_info(&arrayClientes, paqueteDesconectar.id); 
                    removeArrayEntidadesConMutex(&arrayEntidadesJugadores, paqueteDesconectar.id);
                } else {
                    // El cliente no existe. Quizás es una petición duplicada y ya lo borramos.
                    printHora("Cliente no encontrado en el array (puede ser una petición duplicada).");
                }
                
                // 2. En cualquier caso (exista o no), enviamos un ACK de vuelta como cortesía.
                //  Esto maneja tanto la primera petición como los reintentos del cliente.
                PaqueteDesconectarAck paqueteDesconectarAck;
                paqueteDesconectarAck.cabecera.tipoPaquete = PACKET_TYPE_DESCONECTAR_ACK;
                paqueteDesconectarAck.cabecera.numero_secuencia = paqueteDesconectar.header.numero_secuencia + 1; // Hacemos eco de su secuencia

                sendto(socket_servidor, (char *) &paqueteDesconectarAck, sizeof(paqueteDesconectarAck), 0, (struct sockaddr *)&direccion_cliente, sizeof(direccion_cliente));
                
                printf("DESCONECTAR_ACK enviado a la dirección de origen para ID: %d.", paqueteDesconectar.id);

            }
            break;
    }

}

void process_game_tick()
{
    // Aquí va la lógica del juego a ejecutar en cada tick
    printHora("Inicio TICK");
    MensajeRecibido msg;
    char buffer[256];

    //1. Procesamos todos los mensajes recibidos
    //1. Actualizamos el estado del mundo
    while (queue_dequeue(&colaMensajes, &msg))
    {

        printf("-\n");
        sprintf(buffer, "Cliente %d envia el vector: %f, %f", msg.idCliente, msg.dir_nueva.x, msg.dir_nueva.y);
        printHora(buffer);
        // recorremos el array de entidades y actualizamos la direccion de las entidades

        Entidad *ent = getArrayEntidadesConMutex(&arrayEntidadesJugadores, msg.idCliente); // >>> ESTO NO GARANTIZA LA EXCLUSION MUTUA. Puedo
        // usar entidad despues de haber liberado el mutex porque tengo un puntero.

        if (ent != NULL)
        {
            printHora("Incrementado el movimiento de una entidad.");
            ent->dir = msg.dir_nueva;
            ent->pos.x += ent->dir.x;
            ent->pos.y += ent->dir.y;
        }
        else
        {
            sprintf(buffer, "No se ha encontrado la entidad con id %d", msg.idCliente);
            printHora(buffer);
        }
    }

    //2. Enviamos el estado del juego a los clientes
    PaqueteEstadoJuego paqueteEstadoJuego;
    paqueteEstadoJuego.cabecera.numero_secuencia = 0;
    paqueteEstadoJuego.cabecera.tipoPaquete = PACKET_TYPE_ESTADO_JUEGO;
    paqueteEstadoJuego.num_entidades = arrayEntidadesJugadores.num_entidades;
    memcpy(paqueteEstadoJuego.entidades, arrayEntidadesJugadores.entidades, sizeof(Entidad) * arrayEntidadesJugadores.num_entidades);




    for(uint32_t i = 0; i < arrayClientes.numero_clientes; i++){
        printHora("Enviando el estado del juego a un cliente.");
        Cliente_info clienteI = arrayClientes.clientes[i];
        sendto(socket_servidor, (char*)&paqueteEstadoJuego, sizeof(paqueteEstadoJuego), 0, (struct sockaddr *)&clienteI.direccion, sizeof(clienteI.direccion));
    }


    printHora("Fin TICK");
}

void queue_init(ThreadSafeQueue *q)
{
    q->head = q->tail = NULL;
    InitializeCriticalSection(&q->mutex);
    InitializeConditionVariable(&q->cond);
}

void queue_enqueue(ThreadSafeQueue *q, MensajeRecibido msg)
{
    printf("+\n");
    Node *node = malloc(sizeof(Node));
    node->msg = msg;
    node->next = NULL;

    EnterCriticalSection(&q->mutex);
    if (q->tail)
    {
        q->tail->next = node;
        q->tail = node;
    }
    else
    {
        q->head = q->tail = node;
    }
    LeaveCriticalSection(&q->mutex);
    WakeConditionVariable(&q->cond);
}

// Devuelve 1 si hay mensaje, 0 si la cola está vacía
int queue_dequeue(ThreadSafeQueue *q, MensajeRecibido *out_msg)
{
    EnterCriticalSection(&q->mutex);
    if (!q->head)
    {
        LeaveCriticalSection(&q->mutex);
        return 0;
    }
    Node *node = q->head;
    *out_msg = node->msg;
    q->head = node->next;
    if (!q->head)
        q->tail = NULL;

    LeaveCriticalSection(&q->mutex);
    free(node); // CONDICION DE CARRERA?¿?
    return 1;
}

void queue_destroy(ThreadSafeQueue *q)
{
    // Primero, vaciar la cola y liberar todos los nodos
    Node *current = q->head;
    Node *next_node;
    while (current != NULL)
    {
        next_node = current->next;
        free(current);
        current = next_node;
    }
    q->head = q->tail = NULL;

    DeleteCriticalSection(&q->mutex);
}
