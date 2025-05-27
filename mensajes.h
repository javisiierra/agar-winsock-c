#ifndef MENSAJES_H
#define MENSAJES_H




struct MensajeSolicitud{
    int solicitudId;
};



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

#define UNIRSE_A_PARTIDA 1

#endif