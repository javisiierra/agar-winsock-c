#include "dibujo.h"
#include "include/raylib.h"
#include <stdio.h>
#include <math.h>

static const int screenWidth = 800;
static const int screenHeight = 600;

void inicializar_dibujo() {
    InitWindow(screenWidth, screenHeight, "Cliente de Juego");
    SetTargetFPS(60);
}

Vector2D actualizar_dibujo(Entidad* entidades, int num_entidades, int player_id) {
    BeginDrawing();
    ClearBackground(RAYWHITE);

    // --- posicion del mouse ---
    Vector2D direction_vector = {0.0f, 0.0f}; // Default: no movement
    Vector2 mouse_pos = GetMousePosition();
    Vector2 screen_center = { (float)GetScreenWidth() / 2.0f, (float)GetScreenHeight() / 2.0f };

    char buffer[100];

    sprintf(buffer, "Coordenadas del mouse: (%f, %f)", mouse_pos.x, mouse_pos.y);
    DrawText(buffer, 20, 50, 20, BLACK);

    // Solo mueve si click derecho está presionado
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        direction_vector.x = mouse_pos.x - screen_center.x;
        direction_vector.y = mouse_pos.y - screen_center.y;

        sprintf(buffer, "Coordenadas del mouse respecto a centro: (%f, %f)", direction_vector.x, direction_vector.y);
        DrawText(buffer, 20, 80, 20, BLACK);

        // normalizamos el vector
        float magnitude = sqrt(direction_vector.x * direction_vector.x + direction_vector.y * direction_vector.y);
        if (magnitude > 0) {
            direction_vector.x /= magnitude;
            direction_vector.y /= magnitude;
        }

        // reducimos la velocidad de movimiento
        direction_vector.x *= 0.1f;
        direction_vector.y *= 0.1f;

        sprintf(buffer, "Coordenadas del mouse respecto a centro normalizado: (%f, %f)", direction_vector.x, direction_vector.y);
        DrawText(buffer, 20, 120, 20, BLACK);
    } else {
        sprintf(buffer, "Coordenadas del mouse respecto a centro: (0.000000, 0.000000)");
        DrawText(buffer, 20, 80, 20, BLACK);
        sprintf(buffer, "Coordenadas del mouse respecto a centro normalizado: (0.000000, 0.000000)");
        DrawText(buffer, 20, 120, 20, BLACK);
    }

    // Buscamos la entidad del jugador para centrar la vista
    Vector2D player_pos = {0.0f, 0.0f}; // Por defecto en 0,0 si no encuentra jugador
    int player_found = 0;
    for(int i = 0; i < num_entidades; i++) {
        if(entidades[i].id == player_id) {
            player_pos = entidades[i].pos;
            player_found = 1;
            break;
        }
    }
    
    sprintf(buffer, "Coordenadas jugador: (%f, %f)", player_pos.x, player_pos.y);
    DrawText(buffer, 20, 150, 20, BLACK);

    // Encontrar el offset de la cámara para centrarlo en player_pos
    float offsetX = screenWidth / 2.0f - (player_pos.x * 100);
    float offsetY = screenHeight / 2.0f - (player_pos.y * 100);

    // ========== DIBUJAR GRID Y EJES ==========

    // Configuración del grid
    const int grid_size = 100; // Tamaño de cada celda del grid en pixels (1 unidad del mundo = 100 pixels)
    const Color grid_color = LIGHTGRAY;
    const Color axis_color = DARKGRAY;
    const int axis_thickness = 2;
    
    // Calcular el rango de líneas del grid que necesitamos dibujar
    int start_x = (int)floor((-offsetX) / grid_size) - 1;
    int end_x = (int)ceil((screenWidth - offsetX) / grid_size) + 1;
    int start_y = (int)floor((-offsetY) / grid_size) - 1;
    int end_y = (int)ceil((screenHeight - offsetY) / grid_size) + 1;
    
    // Dibujar líneas verticales del grid
    for(int i = start_x; i <= end_x; i++) {
        int x = (int)(i * grid_size + offsetX);
        if(x >= 0 && x < screenWidth) {
            DrawLine(x, 0, x, screenHeight, grid_color);
        }
    }
    
    // Dibujar líneas horizontales del grid
    for(int i = start_y; i <= end_y; i++) {
        int y = (int)(i * grid_size + offsetY);
        if(y >= 0 && y < screenHeight) {
            DrawLine(0, y, screenWidth, y, grid_color);
        }
    }
    
    // Dibujar eje X (línea horizontal que pasa por Y=0 del mundo)
    int axis_y = (int)offsetY;
    if(axis_y >= 0 && axis_y < screenHeight) {
        DrawRectangle(0, axis_y - axis_thickness/2, screenWidth, axis_thickness, axis_color);
    }
    
    // Dibujar eje Y (línea vertical que pasa por X=0 del mundo)
    int axis_x = (int)offsetX;
    if(axis_x >= 0 && axis_x < screenWidth) {
        DrawRectangle(axis_x - axis_thickness/2, 0, axis_thickness, screenHeight, axis_color);
    }

    // ========== FIN DEL GRID Y EJES ==========

    // Dibujar título
    DrawText("Estado del Mundo", 20, 20, 20, BLACK);
    
    // Dibujar cada entidad
    for(int i = 0; i < num_entidades; i++) {
        if(entidades[i].id == 0) continue; // Skip if ID is 0

        // Convert world coordinates to screen coordinates with camera offset
        int x = (int)(entidades[i].pos.x * 100 + offsetX);
        int y = (int)(entidades[i].pos.y * 100 + offsetY);
        
        // Draw entities only if they are within or near screen bounds
        if(x >= -50 && x < screenWidth + 50 && y >= -50 && y < screenHeight + 50) {
            // Draw the entity as a circle, change color for player
            Color entity_color = (entidades[i].id == player_id) ? BLUE : RED;
            DrawCircle(x, y, 10, entity_color);
            
            // Dibujar el ID de la entidad
            char id_text[32];
            sprintf(id_text, "%d", entidades[i].id);
            DrawText(id_text, x - MeasureText(id_text, 16)/2, y - 25, 16, BLACK);
        }
    }
    
    // Mostrar instrucciones
    DrawText("Mueve el ratón manteniendo pulsado el botón derecho", 20, screenHeight - 60, 16, DARKGRAY);
    DrawText("Tu celula solo se movera si pulsas el click derecho", 20, screenHeight - 40, 16, DARKGRAY);
    DrawText("Tu ID: ", screenWidth - 150, 20, 20, BLACK);
    DrawText(TextFormat("%d", player_id), screenWidth - 70, 20, 20, BLUE);

    EndDrawing();

    return direction_vector;
}

int debe_cerrar_ventana() {
    return WindowShouldClose();
}

void cerrar_dibujo() {
    CloseWindow();
}
