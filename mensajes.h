#ifndef MENSAJES_H
#define MENSAJES_H

#include <stdint.h>


#define MAX_ENTIDADES 15
#define MAX_CLIENTES 10

#define TAM_MTU 1500
#define TICK_RATE 60                       // ticks por segundo
#define TICK_INTERVAL_MS (1000 / TICK_RATE) // Intervalo en MILISEGUNDOS


typedef enum {
    PACKET_TYPE_UNIRSE,         // Cliente -> Servidor
    PACKET_TYPE_UNIDO_OK,       // Servidor -> Cliente
    PACKET_TYPE_UNIDO_RECHAZADO,// Servidor -> Cliente
    PACKET_TYPE_MOVIMIENTO,     // Cliente -> Servidor
    PACKET_TYPE_ESTADO_JUEGO,   // Servidor -> Cliente
    PACKET_TYPE_ACK,            // Bidireccional
    PACKET_TYPE_PING,           // Bidireccional
    PACKET_TYPE_DESCONECTAR     // Bidireccional
} TipoPaquete;

typedef struct {
    float x;
    float y;
} Vector2D;

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


#pragma pack(push, 1) // Alineamos

typedef struct {
    uint32_t numero_secuencia; // numero de secuencia.
    TipoPaquete tipoPaquete;         // tipo de paquete que se esta enviando.
} CabeceraRUDP;

// paquete solicitar unirse al juego
typedef struct {
    CabeceraRUDP header; // cabecera del paquete que se va a enviar a los clientes
} PaqueteUnirse;

typedef struct {
    CabeceraRUDP header; // cabecera del paquete que se va a enviar a los clientes
    uint32_t id;         // id del cliente que se va a desconectar
} PaqueteDesconectar;

typedef struct {
    CabeceraRUDP cabecera; // cabecera del paquete que se va a enviar a los clientes
} PaqueteDesconectarAck;

typedef struct {
    CabeceraRUDP header;  // cabecera del paquete que se va a recibir de los clientes
    uint32_t idCliente;         // id del cliente que se mueve
    Vector2D dir_nueva;  // direccion de movimiento que me dice el cliente.
} PaqueteMovimiento;

// Ejemplo de paquete de estado del juego
typedef struct { 
    CabeceraRUDP cabecera;    // cabecera del paquede de que se va a enviar a los clientes
    uint16_t num_entidades; // total de entidades que se van a enviar
    Entidad entidades[MAX_ENTIDADES]; // Todas las entidades que enviara el servidor a los clientes.
} PaqueteEstadoJuego;

typedef struct {
    CabeceraRUDP header; // cabecera del paquete que se va a recibir de los clientes
    uint32_t id;         // id del cliente que se va a desconectar
} PaqueteUnirseAceptado;

typedef struct {
    CabeceraRUDP header; // cabecera del paquete que se va a recibir de los clientes
} PaqueteUnirseRechazado;


#pragma pack(pop) // terminamos alineacion




#endif