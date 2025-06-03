#include <stdio.h>
#include <winsock2.h>
#include <stdint.h>
#include <string.h>
#include "mensajes.h"
#include "dibujo.h"

#pragma comment(lib, "ws2_32.lib")  // Enlaza con la biblioteca Winsock

// Prototipos de funciones auxiliares
void printHora(char *mensaje);  // Imprime mensaje con timestamp
void actualizar_estadisticas_red(int recibidos, int enviados, int perdidos, float ms);  // Actualiza estadísticas visuales

#define MAX_PACKET_SIZE 1400  // Tamaño máximo de paquete para evitar fragmentación IP

// ----- IMPLEMENTACIÓN DEL PROTOCOLO RUDP (Reliable UDP) ----

/**
 * Estructura de cabecera para el protocolo RUDP.
 * Permite implementar fiabilidad sobre UDP mediante confirmaciones (ACKs).
 * El pragma pack garantiza que no haya padding entre campos para la correcta serialización.
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

/**
 * Calcula el checksum de un bloque de datos.
 * Utiliza el algoritmo de suma de Internet estándar para verificar integridad.
 * @param data Puntero a los datos
 * @param len Longitud de los datos en bytes
 * @return Checksum calculado
 */
uint16_t calc_checksum(const void* data, size_t len) {
    uint32_t sum = 0;
    const uint16_t* ptr = (const uint16_t*)data;
    // Suma todos los words de 16 bits
    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    // Si queda un byte, súmalo también
    if (len) sum += *(const uint8_t*)ptr;
    // Fold 32-bit sum to 16 bits (add carry to result)
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)~sum;  // Complemento a 1 del resultado
}

/**
 * Serializa un vector 2D en un buffer de bytes.
 * Garantiza la correcta transferencia de datos float por red.
 * @param buf Buffer de destino
 * @param v Vector a serializar
 */
void serialize_vector2d(uint8_t* buf, const Vector2D* v) {
    char debug[128];
    sprintf(debug, "Serializando vector: X=%.4f, Y=%.4f", v->x, v->y);
    printHora(debug);
    
    memcpy(buf, &v->x, sizeof(float));
    memcpy(buf + sizeof(float), &v->y, sizeof(float));
    
    // Verificar bytes serializados
    sprintf(debug, "Bytes: %02X %02X %02X %02X | %02X %02X %02X %02X",
            buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
    printHora(debug);
}

/**
 * Deserializa un array de entidades desde un buffer de bytes.
 * Procesa cuidadosamente cada campo para evitar problemas de alineación.
 * @param buf Buffer de origen con datos serializados
 * @param entidades Array donde se almacenarán las entidades deserializadas
 * @param num Puntero donde se guardará el número de entidades
 */
void deserialize_entidades(const uint8_t* buf, Entidad* entidades, int* num) {
    // Primero extrae el número de entidades
    memcpy(num, buf, sizeof(int));
    
    char msg[128];
    sprintf(msg, "Recibidas %d entidades para deserializar", *num);
    printHora(msg);
    
    // Deserializa cada entidad individualmente
    for (int i = 0; i < *num; i++) {
        // Deserializar campo por campo para evitar problemas de alineación
        int offset = sizeof(int) + i * sizeof(Entidad);
        
        // ID de la entidad
        memcpy(&entidades[i].id, buf + offset, sizeof(int));
        offset += sizeof(int);
        
        // Tipo de entidad (jugador, comida, etc.)
        memcpy(&entidades[i].tipo, buf + offset, sizeof(TipoEntidad));
        offset += sizeof(TipoEntidad);
        
        // Posición (Vector2D)
        memcpy(&entidades[i].pos.x, buf + offset, sizeof(float));
        offset += sizeof(float);
        memcpy(&entidades[i].pos.y, buf + offset, sizeof(float));
        offset += sizeof(float);
        
        // Dirección (Vector2D)
        memcpy(&entidades[i].dir.x, buf + offset, sizeof(float));
        offset += sizeof(float);
        memcpy(&entidades[i].dir.y, buf + offset, sizeof(float));
        offset += sizeof(float);
        
        // Tamaño de la entidad
        memcpy(&entidades[i].tamanio, buf + offset, sizeof(float));
        
        // Log detallado para depuración
        sprintf(msg, "Entidad[%d]: ID=%d, Tipo=%d, Pos=(%f,%f), Dir=(%f,%f), Tamaño=%f", 
                i, entidades[i].id, entidades[i].tipo, entidades[i].pos.x, entidades[i].pos.y,
                entidades[i].dir.x, entidades[i].dir.y, entidades[i].tamanio);
        printHora(msg);
    }
    
    // log específico para comida
    int comida_count = 0;
    for (int i = 0; i < *num; i++) {
        if (entidades[i].id >= 1000) {
            comida_count++;
            char food_msg[128];
            sprintf(food_msg, "COMIDA DETECTADA: ID=%d, Pos=(%f,%f), Tipo=%d", 
                    entidades[i].id, entidades[i].pos.x, entidades[i].pos.y, entidades[i].tipo);
            printHora(food_msg);
        }
    }
    printHora(comida_count > 0 ? "⭐ COMIDA PRESENTE EN DATOS RECIBIDOS" : "❌ NO HAY COMIDA EN DATOS RECIBIDOS");
}

/**
 * Envía un paquete RUDP.
 * Construye la cabecera, calcula el checksum y envía el paquete por UDP.
 * @return Bytes enviados o -1 si hay error
 */
int rudp_sendto(SOCKET sock, const struct sockaddr_in* addr, uint32_t seq, uint32_t ack, uint8_t flags, const uint8_t* payload, uint16_t payload_len) {
    uint8_t packet[MAX_PACKET_SIZE];
    
    // Configura la cabecera RUDP
    RUDPHeader hdr = { seq, ack, payload_len, 0, flags, 0 };
    
    // Copia la cabecera y el payload en el buffer
    memcpy(packet, &hdr, sizeof(RUDPHeader));
    memcpy(packet + sizeof(RUDPHeader), payload, payload_len);
    
    // Calcula y establece el checksum (primero con checksum=0)
    hdr.checksum = calc_checksum(packet, sizeof(RUDPHeader) + payload_len);
    memcpy(packet, &hdr, sizeof(RUDPHeader));
    
    // Envía el paquete completo
    return sendto(sock, (const char*)packet, sizeof(RUDPHeader) + payload_len, 0, 
                 (const struct sockaddr*)addr, sizeof(*addr));
}

/**
 * Recibe un paquete RUDP.
 * Verifica el checksum y extrae los datos de la cabecera y el payload.
 * @return Tamaño del payload o código de error negativo
 */
int rudp_recvfrom(SOCKET sock, struct sockaddr_in* addr, uint32_t* seq, uint32_t* ack, uint8_t* flags, uint8_t* payload, uint16_t* payload_len) {
    uint8_t packet[MAX_PACKET_SIZE];
    int addrlen = sizeof(*addr);
    
    // Recibe el paquete UDP
    int n = recvfrom(sock, (char*)packet, MAX_PACKET_SIZE, 0, (struct sockaddr*)addr, &addrlen);
    
    // Manejo de errores para sockets no bloqueantes
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
    
    // Verifica que el paquete tenga al menos la cabecera
    if (n < sizeof(RUDPHeader)) {
        return -1;  // Datos insuficientes
    }
    
    // Extrae la cabecera
    RUDPHeader hdr;
    memcpy(&hdr, packet, sizeof(RUDPHeader));
    uint16_t recv_checksum = hdr.checksum;
    
    // Para verificar el checksum, primero lo establece a 0
    hdr.checksum = 0;
    memcpy(packet, &hdr, sizeof(RUDPHeader));
    
    // Verifica el checksum
    if (calc_checksum(packet, n) != recv_checksum) {
        return -2;  // Checksum inválido (paquete corrupto)
    }
    
    // Extrae los datos de la cabecera
    *seq = hdr.seq_num;
    *ack = hdr.ack_num;
    *flags = hdr.flags;
    *payload_len = hdr.payload_len;
    
    // Verificación de seguridad para evitar desbordamiento del buffer
    if (hdr.payload_len > MAX_PACKET_SIZE - sizeof(RUDPHeader)) {
        printHora("Payload demasiado grande para el buffer");
        return -4;
    }
    
    // Copia el payload al buffer del usuario
    memcpy(payload, packet + sizeof(RUDPHeader), hdr.payload_len);
    return hdr.payload_len;
}

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

/**
 * Espera recibir un ACK específico durante un tiempo limitado.
 * Parte fundamental del mecanismo de fiabilidad RUDP.
 * @param esperado_ack Número de ACK que se espera recibir
 * @param max_retries Número máximo de intentos
 * @return 1 si se recibió el ACK, 0 si se agotaron los intentos
 */
int esperar_rudp_ack(SOCKET sock, struct sockaddr_in* server_addr, uint32_t esperado_ack, int max_retries) {
    uint32_t rseq, rack;
    uint8_t flags;
    uint16_t plen;
    uint8_t payload[MAX_PACKET_SIZE];
    int intentos = 0;
    
    // Intenta recibir el ACK hasta agotar reintentos
    while (intentos < max_retries) {
        int res = rudp_recvfrom(sock, server_addr, &rseq, &rack, &flags, payload, &plen);
        // Verifica si es el ACK que esperamos
        if (res >= 0 && (flags & RUDP_FLAG_ACK) && rack == esperado_ack) {
            return 1; // ACK recibido correctamente
        }
        Sleep(10); // Pequeña pausa entre intentos
        intentos++;
    }
    return 0; // No se recibió el ACK esperado
}

/**
 * Función principal del cliente.
 * Implementa la conexión, registro y bucle principal de juego.
 */
int main() {
    // Inicialización de la interfaz gráfica
    inicializar_dibujo();

    // Inicialización de Winsock
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        printHora("Error inicializando Winsock");
        cerrar_dibujo();
        return 1;
    }

    // Creación del socket UDP
    SOCKET cliente_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (cliente_sock == INVALID_SOCKET) {
        printHora("Error creando el socket cliente UDP");
        WSACleanup();
        cerrar_dibujo();
        return 1;
    }

    // Configura socket como no bloqueante para evitar que se bloquee en recvfrom
    u_long mode = 1;
    ioctlsocket(cliente_sock, FIONBIO, &mode);
    printHora("Socket en modo no bloqueante");

    // Configuración de la dirección del servidor
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Buffers y variables para el juego
    Entidad entidades_recibidas[100];  // Almacena todas las entidades del juego
    int num_entidades = 0;             // Número actual de entidades
    int player_id = -1;                // ID del jugador (asignado por el servidor)
    uint32_t seq = 0;                  // Contador de secuencia para RUDP

    // --- FASE 1: REGISTRO DEL CLIENTE ---
    // Envía solicitud de UNIRSE_A_PARTIDA con fiabilidad RUDP
    int mensaje = UNIRSE_A_PARTIDA;
    int max_retries = 5;
    int acked = 0;
    
    // Intenta registrarse hasta recibir confirmación o agotar intentos
    for (int intento = 0; intento < max_retries && !acked; intento++) {
        // Envía la solicitud
        rudp_sendto(cliente_sock, &server_addr, seq, 0, RUDP_FLAG_DATA, (uint8_t*)&mensaje, sizeof(int));
        printHora("Unirse a partida enviado (RUDP)");
        
        // Espera respuesta (ID) y envía ACK
        uint8_t payload[1400];
        uint32_t rseq, rack;
        uint8_t flags;
        uint16_t plen;
        int res = rudp_recvfrom(cliente_sock, &server_addr, &rseq, &rack, &flags, payload, &plen);
        
        // Procesa la respuesta
        if (res > 0 && (flags & RUDP_FLAG_DATA)) {
            // Extrae el ID de jugador asignado
            memcpy(&player_id, payload, sizeof(int));
            char msg[128];
            sprintf(msg, "ID de jugador recibido: %d", player_id);
            printHora(msg);
            
            // Confirma la recepción enviando un ACK
            rudp_sendto(cliente_sock, &server_addr, 0, rseq, RUDP_FLAG_ACK, NULL, 0);
            acked = 1;
            seq++;
            break;
        } else if (res == -2) {
            printHora("Paquete de ID con checksum incorrecto. Esperando siguiente...");
        } else {
            printHora("Error al recibir ID (recvfrom)");
        }
        Sleep(100);  // Espera antes de reintentar
    }

    // Verifica que se haya registrado correctamente
    if (player_id == -1) {
        printHora("No se pudo obtener ID válido del servidor.");
        closesocket(cliente_sock);
        WSACleanup();
        cerrar_dibujo();
        return 1;
    }

    // --- FASE 2: BUCLE PRINCIPAL DEL JUEGO ---
    // Variables para estadísticas de red
    static int total_rx = 0;  // Paquetes recibidos
    static int total_tx = 0;  // Paquetes enviados
    static int total_perdidos = 0;  // Paquetes perdidos

    // Buffers para suavizado de movimiento
    static Entidad entidades_anteriores[100];  // Para almacenar estado anterior
    static int hay_estado_anterior = 0;

    // Bucle principal del juego
    while (!debe_cerrar_ventana()) {
        // Actualiza la interfaz gráfica y obtiene el vector de dirección del usuario
        Vector2D vec = actualizar_dibujo(entidades_recibidas, num_entidades, player_id);

        // Serializa el vector de dirección para enviarlo
        uint8_t vec_payload[8];
        serialize_vector2d(vec_payload, &vec);

        // --- ENVÍA DIRECCIÓN AL SERVIDOR ---
        // Envía el vector de dirección con fiabilidad RUDP (reintenta si no hay ACK)
        int enviado = 0;
        for (int intento = 0; intento < max_retries && !enviado; intento++) {
            rudp_sendto(cliente_sock, &server_addr, seq, 0, RUDP_FLAG_DATA, vec_payload, sizeof(vec_payload));
            // Espera ACK del servidor
            if (esperar_rudp_ack(cliente_sock, &server_addr, seq, 10)) {
                enviado = 1;
                seq++;  // Incrementa secuencia solo si se confirmó la recepción
                break;
            }
            Sleep(10);  // Pequeña pausa entre intentos
        }

        // --- RECIBE ENTIDADES DEL SERVIDOR ---
        uint8_t payload[1400];
        uint32_t rseq, rack;
        uint8_t flags;
        uint16_t plen;
        int res = rudp_recvfrom(cliente_sock, &server_addr, &rseq, &rack, &flags, payload, &plen);
        
        // Procesa los paquetes de entidades recibidos
        if (res > 0 && (flags & RUDP_FLAG_DATA)) {
            printHora("Entidades recibidas, enviando ACK");
            // Verifica que el payload contenga al menos el número de entidades
            if (plen >= sizeof(int)) {
                // Deserializa las entidades
                deserialize_entidades(payload, entidades_recibidas, &num_entidades);
                
                // Implementa suavizado de movimiento
                if (hay_estado_anterior) {
                    for (int i = 0; i < num_entidades; i++) {
                        // Busca la entidad correspondiente en el estado anterior
                        for (int j = 0; j < num_entidades; j++) {
                            if (entidades_recibidas[i].id == entidades_anteriores[j].id) {
                                // Suaviza el movimiento interpolando posiciones
                                float factor = 0.5f; // Factor de suavizado (0.0-1.0)
                                entidades_recibidas[i].pos.x = entidades_anteriores[j].pos.x + 
                                    (entidades_recibidas[i].pos.x - entidades_anteriores[j].pos.x) * factor;
                                entidades_recibidas[i].pos.y = entidades_anteriores[j].pos.y + 
                                    (entidades_recibidas[i].pos.y - entidades_anteriores[j].pos.y) * factor;
                                break;
                            }
                        }
                    }
                }

                // Envía ACK inmediatamente para confirmar recepción
                rudp_sendto(cliente_sock, &server_addr, 0, rseq, RUDP_FLAG_ACK, NULL, 0);
            } else {
                printHora("Paquete de datos recibido pero no contiene entidades válidas");
            }
        } else if (res == -2) {
            printHora("Paquete de entidades con checksum inválido. Descartado.");
        } else if (res == -1) {
            // No hay datos, simplemente sigue esperando (normal en sockets no bloqueantes)
            Sleep(1);
            continue;
        } else {
            printHora("Error en rudp_recvfrom");
            Sleep(1);
            continue;
        }

        // Guarda el estado actual para la próxima vez
        memcpy(entidades_anteriores, entidades_recibidas, sizeof(Entidad) * num_entidades);
        hay_estado_anterior = 1;

        // Actualiza contadores para estadísticas de red
        if (res > 0) total_rx++;  // Incrementa si recibió algo válido
        total_tx++;  // Incrementa cada vez que envía

        // Actualiza las estadísticas visuales en la interfaz
        actualizar_estadisticas_red(total_rx, total_tx, total_perdidos, 0.0f);
    }

    // --- FASE 3: LIMPIEZA Y CIERRE ---
    printHora("Desconectado");
    closesocket(cliente_sock);
    WSACleanup();
    cerrar_dibujo();

    return 0;
}