#ifndef DIBUJO_H
#define DIBUJO_H

#include "mensajes.h"

// Inicializar el sistema de dibujo
void inicializar_dibujo();

// Actualizar y dibujar el juego
void actualizar_dibujo(Entidad* entidades, int num_entidades);

// Verificar si la ventana debe cerrarse
int debe_cerrar_ventana();

// Limpiar recursos de dibujo
void cerrar_dibujo();

#endif
