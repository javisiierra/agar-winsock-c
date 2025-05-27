#include "dibujo.h"
#include "include/raylib.h"
#include <stdio.h>

static const int screenWidth = 800;
static const int screenHeight = 600;

void inicializar_dibujo() {
    InitWindow(screenWidth, screenHeight, "Cliente de Juego");
    SetTargetFPS(60);
}

void actualizar_dibujo(Entidad* entidades, int num_entidades) {
    BeginDrawing();
    ClearBackground(RAYWHITE);
    
    // Dibujar título
    DrawText("Estado del Mundo", 20, 20, 20, BLACK);
    
    // Dibujar cada entidad
    for(int i = 0; i < num_entidades; i++) {
        if(entidades[i].id == 0) continue;
        
        // Convertir coordenadas del mundo a coordenadas de pantalla
        int x = (int)(entidades[i].pos.x * 100) + 100; // Escalar y offset
        int y = (int)(entidades[i].pos.y * 100) + 100;
        
        // Asegurar que esté dentro de los límites de pantalla
        if(x >= 0 && x < screenWidth && y >= 0 && y < screenHeight) {
            // Dibujar la entidad como un círculo
            DrawCircle(x, y, 10, RED);
            
            // Dibujar el ID de la entidad
            char id_text[32];
            sprintf(id_text, "%d", entidades[i].id);
            DrawText(id_text, x - 5, y - 25, 16, BLACK);
        }
    }
    
    // Mostrar instrucciones
    DrawText("Escribe en consola para interactuar", 20, screenHeight - 40, 16, DARKGRAY);
    
    EndDrawing();
}

int debe_cerrar_ventana() {
    return WindowShouldClose();
}

void cerrar_dibujo() {
    CloseWindow();
}
