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


    // Direccion desde el centro al mause
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
    // El origen del mundo 0,0 debería mapearse al centro de la pantalla cuando el jugador se encuentra en 0,0
    float offsetX = screenWidth / 2.0f - (player_pos.x * 100); // Scale by 100 for world units
    float offsetY = screenHeight / 2.0f - (player_pos.y * 100); // Scale by 100 for world units


    // Dibujar título
    DrawText("Estado del Mundo", 20, 20, 20, BLACK);

    
    // Dibujar cada entidad
    for(int i = 0; i < num_entidades; i++) {
        if(entidades[i].id == 0) continue; // Skip if ID is 0

        // Convert world coordinates to screen coordinates with camera offset
        int x = (int)(entidades[i].pos.x * 100 + offsetX);
        int y = (int)(entidades[i].pos.y * 100 + offsetY);
        
        // Draw entities only if they are within or near screen bounds
        if(x >= -50 && x < screenWidth + 50 && y >= -50 && y < screenHeight + 50) { // Add some padding for visibility
            // Draw the entity as a circle, change color for player
            Color entity_color = (entidades[i].id == player_id) ? BLUE : RED;
            DrawCircle(x, y, 10, entity_color);
            
            // Dibujar el ID de la entidad
            char id_text[32];
            sprintf(id_text, "%d", entidades[i].id);
            DrawText(id_text, x - MeasureText(id_text, 16)/2, y - 25, 16, BLACK); // Center text above circle
        }
    }
    
    // Mostrar instrucciones
    DrawText("Mueve el raton para controlar tu celula", 20, screenHeight - 40, 16, DARKGRAY);
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
