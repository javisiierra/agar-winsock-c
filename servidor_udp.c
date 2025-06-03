#include <stdio.h>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#include "mensajes.h"
#include <time.h>
#include <windows.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

// ----- DEFINICIONES Y CONSTANTES -----
#define MAX_ENTIDADES 1000   // Máximo número de entidades (jugadores + comida)
#define MAX_CLIENTES 32      // Número máximo de clientes simultáneos
#define TICK_RATE 30        // Reducir de 100 a 30 para menos actualizaciones pero más consistentes
#define TICK_INTERVAL_MS (1000 / TICK_RATE)  // Intervalo entre ticks en milisegundos
#define MAX_PACKET_SIZE 1400 // Tamaño máximo de paquete UDP (evita fragmentación IP)

// ----- ESTRUCTURAS DE DATOS PRINCIPALES -----

/**
 * Estructura para almacenar todas las entidades del juego con protección de concurrencia.
 * Usa una sección crítica para evitar condiciones de carrera en acceso multithread.
 */
typedef struct {
    Entidad entidades[MAX_ENTIDADES];  // Array de entidades (jugadores y comida)
    int num_entidades;                // Número actual de entidades
    CRITICAL_SECTION mutex;           // Mutex para acceso seguro desde diferentes hilos
} ArrayEntidadesConMutex;

/**
 * Estructura para mensajes recibidos de los clientes.
 * Almacena el ID del cliente, la dirección solicitada y su dirección de red.
 */
typedef struct {
    int client_id;                    // ID único del cliente
    Vector2D dir_nueva;               // Vector de dirección de movimiento
    struct sockaddr_in addr;          // Dirección IP/puerto del cliente
    int addr_len;                     // Longitud de la estructura de dirección
} MensajeRecibido;

// Nodo para la cola de mensajes (implementación lista enlazada)
typedef struct Node {
    MensajeRecibido msg;              // Mensaje del cliente
    struct Node* next;                // Puntero al siguiente nodo
} Node;

/**
 * Cola thread-safe para mensajes de los clientes.
 * Permite enqueuing/dequeuing seguro desde diferentes hilos.
 */
typedef struct {
    Node* head;                       // Puntero al primer nodo
    Node* tail;                       // Puntero al último nodo
    CRITICAL_SECTION mutex;           // Mutex para acceso thread-safe
} ThreadSafeQueue;

// Variables globales para el estado del juego
ArrayEntidadesConMutex arrayEntidadesJugadores;  // Almacena todas las entidades
ThreadSafeQueue colaMensajes;                    // Cola de mensajes de los clientes
volatile int running = 1;                        // Flag para controlar el bucle principal

/**
 * Imprime un mensaje con timestamp y thread ID.
 * Útil para depuración y registro de eventos.
 */
void printHora(char *mensaje) {
    SYSTEMTIME st;
    GetSystemTime(&st);
    printf("[%02d:%02d:%02d.%03d - %d] %s\n",
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, GetCurrentThreadId(), mensaje);
}

// ----- FUNCIONES DE MANEJO DE ENTIDADES ----/

/**
 * Inicializa el array de entidades con su mutex correspondiente.
 */
void inicializarArrayEntidadesConMutex(ArrayEntidadesConMutex* array) {
    InitializeCriticalSection(&array->mutex);
    array->num_entidades = 0;
}

/**
 * Añade una entidad al array de forma thread-safe.
 * Retorna 0 si se añadió correctamente, -1 si el array está lleno.
 */
int addArrayEntidadesConMutex(ArrayEntidadesConMutex* array, Entidad entidad) {
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

/**
 * Busca y devuelve un puntero a la entidad con el ID especificado.
 * Thread-safe. Retorna NULL si no encuentra la entidad.
 */
Entidad* getArrayEntidadesConMutex(ArrayEntidadesConMutex* array, int id) {
    EnterCriticalSection(&array->mutex);
    for (int i = 0; i < array->num_entidades; i++) {
        if (array->entidades[i].id == id) {
            LeaveCriticalSection(&array->mutex);
            return &array->entidades[i];
        }
    }
    LeaveCriticalSection(&array->mutex);
    return NULL;
}

/**
 * Elimina una entidad del array por su ID de forma thread-safe.
 */
void removeArrayEntidadesConMutex(ArrayEntidadesConMutex* array, int id) {
    EnterCriticalSection(&array->mutex);
    for (int i = 0; i < array->num_entidades; i++) {
        if (array->entidades[i].id == id) {
            // Desplaza todas las entidades una posición para eliminar
            for (int j = i; j < array->num_entidades - 1; j++) {
                array->entidades[j] = array->entidades[j + 1];
            }
            array->num_entidades--;
            break;
        }
    }
    LeaveCriticalSection(&array->mutex);
}

// ----- FUNCIONES DE COLA DE MENSAJES ----/

/**
 * Inicializa la cola de mensajes thread-safe.
 */
void queue_init(ThreadSafeQueue* q) {
    q->head = q->tail = NULL;
    InitializeCriticalSection(&q->mutex);
}

/**
 * Añade un mensaje a la cola de forma thread-safe.
 */
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
}

/**
 * Extrae un mensaje de la cola de forma thread-safe.
 * Retorna 1 si extrajo un mensaje, 0 si la cola está vacía.
 */
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
    free(node);
    return 1;
}

// ----- GESTIÓN DE CLIENTES ----/

/**
 * Estructura para almacenar información de los clientes conectados.
 */
typedef struct {
    struct sockaddr_in addr;   // Dirección IP/puerto del cliente
    int addr_len;              // Longitud de la dirección
    int id;                    // ID único asignado al cliente
} ClienteUDP;

// Array de clientes conectados
ClienteUDP clientes[MAX_CLIENTES];
int num_clientes = 0;

/**
 * Busca un cliente por su dirección IP/puerto.
 * Retorna el ID del cliente si lo encuentra, -1 en caso contrario.
 */
int buscar_cliente(struct sockaddr_in* addr) {
    for (int i = 0; i < num_clientes; i++) {
        if (clientes[i].addr.sin_addr.s_addr == addr->sin_addr.s_addr &&
            clientes[i].addr.sin_port == addr->sin_port) {
            return clientes[i].id;
        }
    }
    return -1;
}

/**
 * Registra un nuevo cliente.
 * Retorna el ID asignado o -1 si no hay espacio para más clientes.
 */
int registrar_cliente(struct sockaddr_in* addr, int addr_len) {
    if (num_clientes >= MAX_CLIENTES) return -1;
    int id = num_clientes + 1;
    clientes[num_clientes].addr = *addr;
    clientes[num_clientes].addr_len = addr_len;
    clientes[num_clientes].id = id;
    num_clientes++;
    return id;
}

// ----- LÓGICA DE JUEGO ----/

/**
 * Procesa un tick del juego.
 * - Procesa los mensajes recibidos de los clientes
 * - Actualiza las posiciones de las entidades
 * - Detecta colisiones entre jugadores y comida
 * - Elimina las entidades marcadas para eliminación
 */
void process_game_tick(SOCKET sock) {
    MensajeRecibido msg;
    char buffer[256];

    // Procesar mensajes recibidos (movimientos solicitados por los clientes)
    while (queue_dequeue(&colaMensajes, &msg)) {
        sprintf(buffer, "Cliente %d envia el vector: %f, %f", msg.client_id, msg.dir_nueva.x, msg.dir_nueva.y);
        printHora(buffer);

        // Actualiza la dirección del jugador
        Entidad* ent = getArrayEntidadesConMutex(&arrayEntidadesJugadores, msg.client_id);
        if (ent != NULL) {
            ent->dir = msg.dir_nueva;
        }
    }

    // TODA LA LÓGICA DENTRO DE UNA ÚNICA SECCIÓN CRÍTICA
    // para garantizar la consistencia de los datos
    EnterCriticalSection(&arrayEntidadesJugadores.mutex);
    
    // 1. Actualizar posiciones de todas las entidades
    for (int i = 0; i < arrayEntidadesJugadores.num_entidades; i++) {
        Entidad* ent = &arrayEntidadesJugadores.entidades[i];
        
        // Solo los jugadores (ID < 1000) se mueven, la comida es estática
        if (ent->id < 1000) {  
            // Debug para verificar dirección
            char dir_debug[128];
            sprintf(dir_debug, "DEBUG MOVIMIENTO: Entidad %d, dir=(%f,%f)", 
                   ent->id, ent->dir.x, ent->dir.y);
            printHora(dir_debug);
            
            // Actualiza posición aplicando el vector de dirección
            ent->pos.x += ent->dir.x * 0.3f;  // Reducir de 1.0f a 0.3f para movimiento más suave
            ent->pos.y += ent->dir.y * 0.3f;
            
            // Solo imprime si hay movimiento significativo
            if (fabs(ent->dir.x) > 0.001f || fabs(ent->dir.y) > 0.001f) {
                char buf[128];
                sprintf(buf, "Jugador %d: pos=(%f,%f), dir=(%f,%f)", 
                        ent->id, ent->pos.x, ent->pos.y, ent->dir.x, ent->dir.y);
                printHora(buf);
            }
        }
    }
    
    // 2. Detección de colisiones (jugadores comen comida)
    for (int i = 0; i < arrayEntidadesJugadores.num_entidades; i++) {
        Entidad* jugador = &arrayEntidadesJugadores.entidades[i];
        
        // Debug del tipo de entidad
        char tipo_debug[128];
        sprintf(tipo_debug, "Entidad %d: tipo=%d (¿Es jugador? %s)", 
                jugador->id, jugador->tipo, 
                (jugador->tipo == ENTIDAD_JUGADOR) ? "SI" : "NO");
        printHora(tipo_debug);
        
        // Solo los jugadores pueden comer
        if (jugador->tipo != ENTIDAD_JUGADOR) continue;
        
        float radio_jugador = jugador->tamanio;
        
        // Comprueba colisiones con todas las demás entidades
        for (int j = 0; j < arrayEntidadesJugadores.num_entidades; j++) {
            if (i == j) continue; // No compares consigo mismo
            
            Entidad* comida = &arrayEntidadesJugadores.entidades[j];
            if (comida->tipo != ENTIDAD_COMIDA) continue;
            
            // Cálculo de distancia entre entidades (Pitágoras)
            float dx = jugador->pos.x - comida->pos.x;
            float dy = jugador->pos.y - comida->pos.y;
            float distancia = sqrt(dx*dx + dy*dy);
            
            // Debug de la comprobación de colisión
            char col_debug[128];
            sprintf(col_debug, "Verificando: Jugador %d (%f,%f) vs Comida %d (%f,%f), dist=%f, radio=%f", 
                   jugador->id, jugador->pos.x, jugador->pos.y,
                   comida->id, comida->pos.x, comida->pos.y,
                   distancia, radio_jugador);
            printHora(col_debug);
            
            // Si la distancia es menor que el radio del jugador + margen, se produce colisión
            float margen_extra = 15.0f; // Aumentado de 5.0f a 10.0f
            if (distancia < radio_jugador + margen_extra) {
                char msg[128];
                sprintf(msg, "¡COLISIÓN! Jugador %d comió comida %d", jugador->id, comida->id);
                printHora(msg);
                
                // Marcar para eliminar (se eliminará después)
                comida->id = 0;
                
                // Jugador crece al comer
                jugador->tamanio += 0.5f;
                
                // Regenerar comida en posición aleatoria
                if (rand() % 2 == 0) { // 50% de probabilidad
                    Entidad nueva_comida;
                    nueva_comida.id = comida->id + 1000; // Nuevo ID
                    nueva_comida.tipo = ENTIDAD_COMIDA;
                    nueva_comida.pos.x = (float)(rand() % 16 - 8); // Rango más cercano (-8 a 8)
                    nueva_comida.pos.y = (float)(rand() % 16 - 8); // Rango más cercano (-8 a 8)
                    nueva_comida.dir.x = 0;
                    nueva_comida.dir.y = 0;
                    nueva_comida.tamanio = 5.0f;
                    addArrayEntidadesConMutex(&arrayEntidadesJugadores, nueva_comida);
                }
            }
        }
    }
    
    // 3. Eliminar entidades marcadas (ID = 0)
    int j = 0;
    for (int i = 0; i < arrayEntidadesJugadores.num_entidades; i++) {
        if (arrayEntidadesJugadores.entidades[i].id != 0) {
            // Compacta el array desplazando las entidades válidas
            if (i != j) {
                arrayEntidadesJugadores.entidades[j] = arrayEntidadesJugadores.entidades[i];
            }
            j++;
        }
    }
    arrayEntidadesJugadores.num_entidades = j;
    
    LeaveCriticalSection(&arrayEntidadesJugadores.mutex);
}

// ----- IMPLEMENTACIÓN DE PROTOCOLO RUDP (Reliable UDP) ----/

/**
 * Estructura de cabecera para el protocolo RUDP.
 * Permite implementar fiabilidad sobre UDP mediante confirmaciones (ACKs).
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t seq_num;       // Número de secuencia del paquete
    uint32_t ack_num;       // Número de ACK (confirma recepción hasta este número)
    uint16_t payload_len;   // Longitud del payload en bytes
    uint16_t checksum;      // Checksum para verificar integridad
    uint8_t  flags;         // Flags (DATA, ACK, etc.)
    uint8_t  reserved;      // Reservado para uso futuro
} RUDPHeader;
#pragma pack(pop)

// Definiciones de flags RUDP
#define RUDP_FLAG_DATA 0x01   // Paquete contiene datos
#define RUDP_FLAG_ACK  0x02   // Paquete es una confirmación (ACK)
#define RUDP_TIMEOUT_MS 100   // Timeout para retransmisiones en ms
#define MAX_RETRIES 5         // Número máximo de reintentos

/**
 * Estructura para almacenar el estado RUDP de cada cliente.
 * Mantiene los contadores de secuencia y ACK.
 */
typedef struct {
    uint32_t last_seq_sent;    // Último número de secuencia enviado
    uint32_t last_ack_recv;    // Último ACK recibido
} ClienteRUDPState;

// Estado RUDP para cada cliente
ClienteRUDPState rudp_states[MAX_CLIENTES];

/**
 * Calcula el checksum de un bloque de datos.
 * Usado para verificar la integridad de los paquetes RUDP.
 */
uint16_t calc_checksum(const void* data, size_t len) {
    uint32_t sum = 0;
    const uint16_t* ptr = (const uint16_t*)data;
    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    if (len) sum += *(const uint8_t*)ptr;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)~sum;
}

/**
 * Envía un paquete RUDP.
 * Construye la cabecera, calcula el checksum y envía el paquete.
 */
int rudp_sendto(SOCKET sock, const struct sockaddr_in* addr, uint32_t seq, uint32_t ack, uint8_t flags, const uint8_t* payload, uint16_t payload_len) {
    uint8_t packet[MAX_PACKET_SIZE];
    // Configura la cabecera
    RUDPHeader hdr = { seq, ack, payload_len, 0, flags, 0 };
    // Copia la cabecera y el payload en el buffer
    memcpy(packet, &hdr, sizeof(RUDPHeader));
    memcpy(packet + sizeof(RUDPHeader), payload, payload_len);
    // Calcula y establece el checksum
    hdr.checksum = calc_checksum(packet, sizeof(RUDPHeader) + payload_len);
    memcpy(packet, &hdr, sizeof(RUDPHeader));
    // Envía el paquete
    return sendto(sock, (const char*)packet, sizeof(RUDPHeader) + payload_len, 0, (const struct sockaddr*)addr, sizeof(*addr));
}

/**
 * Recibe un paquete RUDP.
 * Verifica el checksum y extrae los datos.
 * Retorna el tamaño del payload o un código de error.
 */
int rudp_recvfrom(SOCKET sock, struct sockaddr_in* addr, uint32_t* seq, uint32_t* ack, uint8_t* flags, uint8_t* payload, uint16_t* payload_len) {
    uint8_t packet[MAX_PACKET_SIZE];
    int addrlen = sizeof(*addr);
    
    // Recibe el paquete
    int n = recvfrom(sock, (char*)packet, MAX_PACKET_SIZE, 0, (struct sockaddr*)addr, &addrlen);
    
    // Manejo de errores
    if (n == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK) {
            // No hay datos disponibles (normal en sockets no-bloqueantes)
            return -1;
        } else {
            // Otro error de socket
            char error_msg[256];
            sprintf(error_msg, "ERROR en recvfrom: %d", error);
            printHora(error_msg);
            return -3;  // Error de socket
        }
    }
    
    // Verifica que el paquete tenga al menos la cabecera completa
    if (n < sizeof(RUDPHeader)) {
        return -1;  // Datos insuficientes
    }
    
    // Extrae la cabecera
    RUDPHeader hdr;
    memcpy(&hdr, packet, sizeof(RUDPHeader));
    uint16_t recv_checksum = hdr.checksum;
    // Establece checksum a 0 para verificar
    hdr.checksum = 0;
    memcpy(packet, &hdr, sizeof(RUDPHeader));
    
    // Verifica el checksum
    if (calc_checksum(packet, n) != recv_checksum) {
        return -2;  // Checksum inválido
    }
    
    // Extrae los datos de la cabecera
    *seq = hdr.seq_num;
    *ack = hdr.ack_num;
    *flags = hdr.flags;
    *payload_len = hdr.payload_len;
    
    // Verificación de seguridad para evitar desbordamiento
    if (hdr.payload_len > MAX_PACKET_SIZE - sizeof(RUDPHeader)) {
        printHora("Payload demasiado grande para el buffer");
        return -4;
    }
    
    // Copia el payload
    memcpy(payload, packet + sizeof(RUDPHeader), hdr.payload_len);
    return hdr.payload_len;
}

/**
 * Envía las entidades a todos los clientes de forma fiable (RUDP).
 * Para cada cliente, intenta hasta MAX_RETRIES veces si no recibe ACK.
 */
void enviar_entidades_a_clientes(SOCKET sock) {
    // Añade este debug
    int comida_count = 0;
    for (int i = 0; i < arrayEntidadesJugadores.num_entidades; i++) {
        if (arrayEntidadesJugadores.entidades[i].id >= 1000) {
            comida_count++;
        }
    }
    char debug[256];
    sprintf(debug, "Enviando %d entidades totales (%d son comida)", 
            arrayEntidadesJugadores.num_entidades, comida_count);
    printHora(debug);
    
    // Prepara el buffer con todas las entidades
    uint8_t entidades_buf[sizeof(int) + MAX_ENTIDADES * sizeof(Entidad)];
    int num = arrayEntidadesJugadores.num_entidades;
    memcpy(entidades_buf, &num, sizeof(int));
    memcpy(entidades_buf + sizeof(int), arrayEntidadesJugadores.entidades, num * sizeof(Entidad));
    
    // Envía a cada cliente
    for (int i = 0; i < num_clientes; i++) {
        uint32_t myseq = ++rudp_states[i].last_seq_sent;
        int retries = 0;
        int acked = 0;
        
        // Intenta hasta recibir ACK o agotar reintentos
        while (retries < MAX_RETRIES && !acked) {
            // Envía el paquete con las entidades
            rudp_sendto(sock, &clientes[i].addr, myseq, 0, RUDP_FLAG_DATA, entidades_buf, sizeof(int) + num * sizeof(Entidad));
            DWORD start = GetTickCount();
            
            // Espera ACK durante un tiempo limitado
            while (GetTickCount() - start < RUDP_TIMEOUT_MS) {
                struct sockaddr_in ack_addr;
                uint32_t ack_seq, ack_ack;
                uint8_t ack_flags;
                uint16_t ack_plen;
                uint8_t ack_payload[MAX_PACKET_SIZE];
                int ack_res = rudp_recvfrom(sock, &ack_addr, &ack_seq, &ack_ack, &ack_flags, ack_payload, &ack_plen);
                
                // Verifica si es un ACK válido para este cliente y paquete
                if (ack_res >= 0 && (ack_flags & RUDP_FLAG_ACK) &&
                    ack_addr.sin_addr.s_addr == clientes[i].addr.sin_addr.s_addr &&
                    ack_addr.sin_port == clientes[i].addr.sin_port &&
                    ack_ack == myseq) {
                    rudp_states[i].last_ack_recv = ack_ack;
                    acked = 1;
                    break;
                }
                Sleep(1);
            }
            
            // Si no recibió ACK, reintenta
            if (!acked) {
                retries++;
                printHora("Reintentando envío de entidades por falta de ACK");
            }
        }
        
        // Si agotó los reintentos sin ACK, informa
        if (!acked) {
            printHora("No se recibió ACK tras varios intentos, se omite el envío de entidades");
        }
    }
}

/**
 * Genera comida en posiciones aleatorias del mapa.
 * Se llama durante la inicialización del servidor.
 */
void generar_comida(int cantidad) {
    for (int i = 0; i < cantidad; i++) {
        if (arrayEntidadesJugadores.num_entidades < MAX_ENTIDADES) {
            Entidad comida;
            comida.id = 1000 + i;
            comida.tipo = ENTIDAD_COMIDA;
            // Posiciones más cercanas al origen (donde empieza el jugador)
            comida.pos.x = (float)(rand() % 20 - 10); // Rango (-10 a 10)
            comida.pos.y = (float)(rand() % 20 - 10); // Rango (-10 a 10)
            comida.dir.x = 0;
            comida.dir.y = 0;
            comida.tamanio = 20.0f;  // Tamaño aumentado para mejor visibilidad
            
            char debug[128];
            sprintf(debug, "⭐ COMIDA GENERADA CERCANA: ID=%d, Pos=(%f,%f)", 
                   comida.id, comida.pos.x, comida.pos.y);
            printHora(debug);
            
            addArrayEntidadesConMutex(&arrayEntidadesJugadores, comida);
        }
    }
    
    // Añadir comida en posiciones fijas conocidas para pruebas
    if (arrayEntidadesJugadores.num_entidades < MAX_ENTIDADES) {
        Entidad comida_test;
        comida_test.id = 1998;
        comida_test.tipo = ENTIDAD_COMIDA;
        comida_test.pos.x = 0.0f;  // Origen - debería ser muy visible
        comida_test.pos.y = 0.0f;
        comida_test.dir.x = 0;
        comida_test.dir.y = 0;
        comida_test.tamanio = 30.0f;  // Extra grande para mejor visibilidad
        addArrayEntidadesConMutex(&arrayEntidadesJugadores, comida_test);
        printHora("⭐⭐⭐ COMIDA DE PRUEBA AÑADIDA EN EL ORIGEN (0,0)");
    }
    
    printHora("Generación de comida completada");
}

// ----- FUNCIÓN PRINCIPAL -----

int main() {
    WSADATA wsa;
    printHora("INICIO main()");
    
    // Inicialización de estructuras de datos
    inicializarArrayEntidadesConMutex(&arrayEntidadesJugadores);
    printHora("ArrayEntidadesConMutex inicializado");
    queue_init(&colaMensajes);
    printHora("Cola de mensajes inicializada");

    // Inicialización de Winsock
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        printHora("Error inicializando Winsock");
        return 1;
    }
    printHora("Winsock inicializado");

    // Creación del socket UDP
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        printHora("Error creando el socket UDP");
        WSACleanup();
        return 1;
    }
    printHora("Socket UDP creado");

    // Configuración de la dirección del servidor
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");  // Localhost
    addr.sin_port = htons(8080);                    // Puerto 8080

    // Asociar socket a la dirección (bind)
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printHora("Error en bind");
        closesocket(sock);
        WSACleanup();
        return 1;
    }
    printHora("Servidor UDP escuchando en el puerto 8080");

    // Configurar socket como no bloqueante
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
    printHora("Socket en modo no bloqueante");

    // Configuración para medir tiempo entre ticks
    LARGE_INTEGER frequency, last_tick, current_tick;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&last_tick);

    // Buffers para recepción de datos
    uint8_t payload[MAX_PACKET_SIZE];
    uint32_t seq, ack;
    uint8_t flags;
    uint16_t plen;

    printHora("Esperando paquetes...");
    printHora("INICIO DEL BUCLE PRINCIPAL");

    // Semilla para números aleatorios (comida)
    srand(time(NULL));
    // Generar comida inicial
    generar_comida(50);

    // Al iniciar servidor, crea comida en la misma posición inicial del jugador
    Entidad comida_test;
    comida_test.id = 1999; // ID especial para pruebas
    comida_test.tipo = ENTIDAD_COMIDA;
    comida_test.pos.x = 10.0f; // Igual que posición inicial del jugador
    comida_test.pos.y = 10.0f;
    comida_test.dir.x = 0;
    comida_test.dir.y = 0;
    comida_test.tamanio = 15.0f;
    addArrayEntidadesConMutex(&arrayEntidadesJugadores, comida_test);
    printHora("⭐⭐⭐ COMIDA DE PRUEBA AÑADIDA EN POSICIÓN DEL JUGADOR");

    // ----- BUCLE PRINCIPAL DEL SERVIDOR -----
    while (1) {
        struct sockaddr_in cli_addr;
        int cli_addr_len = sizeof(cli_addr);

        // Intenta recibir un paquete RUDP
        int res = rudp_recvfrom(sock, &cli_addr, &seq, &ack, &flags, payload, &plen);

        if (res == -1) {
            // No hay datos disponibles (normal en sockets no bloqueantes)
            Sleep(1);
            continue;
        } else if (res == -3) {
            // Error grave de socket, pero continuamos
            printHora("Error de socket detectado - continuando bucle");
            Sleep(100); // Esperar más para evitar CPU al 100%
            continue;
        } else if (res < 0) {
            // Otros errores
            printHora("Error en recepción de paquete");
            Sleep(1);
            continue;
        }

        printHora("Paquete recibido o error de checksum");

        // Responder con ACK si recibimos DATA
        if (res > 0 && (flags & RUDP_FLAG_DATA)) {
            printHora("Recibido DATA, enviando ACK");
            rudp_sendto(sock, &cli_addr, 0, seq, RUDP_FLAG_ACK, NULL, 0);
        }

        // Procesar paquete recibido
        if (res > 0) {
            char msg[128];
            sprintf(msg, "Payload recibido de %s:%d, tamaño %d bytes", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port), plen);
            printHora(msg);

            // TIPO 1: Solicitud de UNIRSE_A_PARTIDA
            if (plen == sizeof(int)) {
                int msg_type;
                memcpy(&msg_type, payload, sizeof(int));
                sprintf(msg, "Mensaje tipo: %d", msg_type);
                printHora(msg);

                if (msg_type == UNIRSE_A_PARTIDA) {
                    printHora("Solicitud de UNIRSE_A_PARTIDA recibida");
                    int id = buscar_cliente(&cli_addr);
                    if (id == -1) {
                        // Registrar nuevo cliente
                        id = registrar_cliente(&cli_addr, cli_addr_len);
                        if (id == -1) {
                            printHora("Demasiados clientes UDP");
                            continue;
                        }
                        
                        // Crear entidad jugador para el nuevo cliente
                        Entidad entidad_jugador;
                        entidad_jugador.id = id;
                        entidad_jugador.tipo = ENTIDAD_JUGADOR;  // Tipo explícito
                        entidad_jugador.pos.x = 10;  // Posición inicial
                        entidad_jugador.pos.y = 10;
                        entidad_jugador.dir.x = 0;   // Sin movimiento inicial
                        entidad_jugador.dir.y = 0;
                        entidad_jugador.tamanio = 10.0f;  // Tamaño inicial
                        addArrayEntidadesConMutex(&arrayEntidadesJugadores, entidad_jugador);
                        printHora("Entidad jugador añadida");

                        // Enviar ID al cliente con RUDP (fiable)
                        uint32_t myseq = ++rudp_states[id-1].last_seq_sent;
                        int retries = 0;
                        int acked = 0;
                        while (retries < MAX_RETRIES && !acked) {
                            printHora("Enviando ID al cliente...");
                            rudp_sendto(sock, &cli_addr, myseq, 0, RUDP_FLAG_DATA, (uint8_t*)&id, sizeof(int));
                            DWORD start = GetTickCount();
                            
                            // Esperar ACK durante un tiempo limitado
                            while (GetTickCount() - start < RUDP_TIMEOUT_MS) {
                                struct sockaddr_in ack_addr;
                                uint32_t ack_seq, ack_ack;
                                uint8_t ack_flags;
                                uint16_t ack_plen;
                                uint8_t ack_payload[MAX_PACKET_SIZE];
                                int ack_res = rudp_recvfrom(sock, &ack_addr, &ack_seq, &ack_ack, &ack_flags, ack_payload, &ack_plen);
                                if (ack_res >= 0 && (ack_flags & RUDP_FLAG_ACK) &&
                                    ack_addr.sin_addr.s_addr == cli_addr.sin_addr.s_addr &&
                                    ack_addr.sin_port == cli_addr.sin_port &&
                                    ack_ack == myseq) {
                                    rudp_states[id-1].last_ack_recv = ack_ack;
                                    acked = 1;
                                    printHora("ACK de ID recibido");
                                    break;
                                }
                                Sleep(1);
                            }
                            
                            // Si no recibió ACK, reintenta
                            if (!acked) {
                                retries++;
                                printHora("Reintentando envío de ID por falta de ACK");
                            }
                        }
                        
                        // Resultado final del envío
                        if (!acked) {
                            printHora("No se recibió ACK tras varios intentos, se omite el envío de ID");
                        } else {
                            printHora("Cliente UDP registrado y enviado ID");
                        }
                    } else {
                        printHora("Cliente ya registrado");
                    }
                }
            } 
            // TIPO 2: Vector de dirección del jugador
            else if (plen == sizeof(Vector2D)) {
                int id = buscar_cliente(&cli_addr);
                if (id != -1) {
                    Vector2D dir;
                    memcpy(&dir, payload, sizeof(Vector2D));

                    // Debug detallado del vector recibido
                    char debug_vec[256];
                    sprintf(debug_vec, "MOVIMIENTO CLIENTE %d: X=%.4f, Y=%.4f (bytes: %02X %02X %02X %02X | %02X %02X %02X %02X)",
                            id, dir.x, dir.y,
                            payload[0], payload[1], payload[2], payload[3],  // bytes de X
                            payload[4], payload[5], payload[6], payload[7]); // bytes de Y
                    printHora(debug_vec);

                    // Encolar el mensaje para ser procesado en el próximo tick
                    MensajeRecibido msg;
                    msg.client_id = id;
                    msg.dir_nueva = dir;
                    msg.addr = cli_addr;
                    msg.addr_len = cli_addr_len;
                    queue_enqueue(&colaMensajes, msg);
                    printHora("Vector2D recibido y encolado");
                } else {
                    printHora("Vector2D recibido de cliente no registrado");
                }
            } else {
                printHora("Payload recibido de tamaño inesperado");
            }
        } else if (res == -2) {
            printHora("Paquete recibido con checksum inválido. Descartado.");
        }

        // Comprobar si es momento de procesar un tick del juego
        QueryPerformanceCounter(&current_tick);
        long long elapsed_ms = (current_tick.QuadPart - last_tick.QuadPart) * 1000 / frequency.QuadPart;
        if (elapsed_ms >= TICK_INTERVAL_MS) {
            printHora("Tick de juego");
            // Actualizar estado del juego
            process_game_tick(sock);
            // Enviar estado actualizado a todos los clientes
            printHora("Enviando entidades a clientes");
            enviar_entidades_a_clientes(sock);
            // Actualizar timestamp del último tick
            last_tick = current_tick;
        }
        Sleep(1);  // Pequeña pausa para no saturar CPU
    }

    // Código de limpieza (nunca se ejecuta en este caso)
    printHora("Cerrando socket y limpiando Winsock");
    closesocket(sock);
    WSACleanup();
    return 0;
}