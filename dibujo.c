#include "dibujo.h"
#include "include/raylib.h"
#include <stdio.h>
#include <math.h>

// ----- CONFIGURACIÓN Y VARIABLES GLOBALES -----

// Dimensiones de la ventana de juego
static const int screenWidth = 800;
static const int screenHeight = 600;

// Variables para estadísticas de red - Permiten visualizar el estado de la conexión RUDP
static int paquetes_recibidos = 0;   // Contador de paquetes recibidos correctamente
static int paquetes_enviados = 0;    // Contador de paquetes enviados al servidor
static int paquetes_perdidos = 0;    // Contador de paquetes que requirieron retransmisión
static float latencia_ms = 0.0f;     // Tiempo de ida y vuelta (RTT) en milisegundos
static bool conexion_activa = false; // Estado actual de la conexión RUDP

/**
 * Inicializa la ventana de juego con Raylib.
 * Configura la resolución y el título de la ventana.
 */
void inicializar_dibujo() {
    InitWindow(screenWidth, screenHeight, "Cliente de Juego - RUDP");
    SetTargetFPS(60); // Limita el framerate a 60 FPS para rendimiento consistente
}

/**
 * Actualiza las estadísticas de red para visualización.
 * Esta función es llamada desde el cliente_udp.c con cada actualización.
 * 
 * @param recibidos Número total de paquetes recibidos correctamente
 * @param enviados Número total de paquetes enviados
 * @param perdidos Número de paquetes perdidos/retransmitidos
 * @param ms Latencia actual en milisegundos
 */
void actualizar_estadisticas_red(int recibidos, int enviados, int perdidos, float ms) {
    paquetes_recibidos = recibidos;
    paquetes_enviados = enviados;
    paquetes_perdidos = perdidos;
    latencia_ms = ms;
    conexion_activa = true; // Marca la conexión como activa
}

/**
 * Función principal de renderizado. Actualiza toda la interfaz gráfica del juego.
 * 
 * @param entidades Array con todas las entidades del juego (jugadores y comida)
 * @param num_entidades Número de entidades en el array
 * @param player_id ID del jugador local
 * @return Vector2D con la dirección de movimiento solicitada por el jugador
 */
Vector2D actualizar_dibujo(Entidad* entidades, int num_entidades, int player_id) {

    // Debug - imprime información sobre las entidades que se van a dibujar
    printf("\n--- ENTIDADES PASADAS A DIBUJO ---\n");
    printf("Número total: %d\n", num_entidades);
    for (int i = 0; i < num_entidades; i++) {
        printf("Entidad[%d]: ID=%d, Tipo=%d, Pos=(%.1f,%.1f)\n",
               i, entidades[i].id, entidades[i].tipo, entidades[i].pos.x, entidades[i].pos.y);
    }
    printf("--------------------------------\n\n");
    
    BeginDrawing();
    ClearBackground(RAYWHITE); // Limpia la pantalla con fondo blanco

    // ----- CAPTURA DE ENTRADA Y CÁLCULO DE DIRECCIÓN -----
    
    // Vector de dirección inicializado a cero (sin movimiento)
    Vector2D direction_vector = {0.0f, 0.0f};
    
    // Obtiene la posición actual del mouse y el centro de la pantalla
    Vector2 mouse_pos = GetMousePosition();
    Vector2 screen_center = { (float)GetScreenWidth() / 2.0f, (float)GetScreenHeight() / 2.0f };

    char buffer[100]; // Buffer para formatear texto

    // ----- CÁLCULO DE LA POSICIÓN DEL JUGADOR -----
    
    // Busca la entidad del jugador en el array para centrar la cámara
    Vector2D player_pos = {0.0f, 0.0f}; // Posición por defecto si no se encuentra
    int player_found = 0;
    for(int i = 0; i < num_entidades; i++) {
        if(entidades[i].id == player_id) {
            player_pos = entidades[i].pos;
            player_found = 1;
            break;
        }
    }
    
    // ----- CÁLCULO DEL VECTOR DE DIRECCIÓN (CONTROL DEL JUGADOR) -----
    
    // Solo calcula dirección si el botón izquierdo del ratón está presionado
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        // Vector desde el centro hasta el mouse
        direction_vector.x = mouse_pos.x - screen_center.x;
        direction_vector.y = mouse_pos.y - screen_center.y;

        // Normaliza el vector (convierte a longitud 1.0)
        float magnitude = sqrt(direction_vector.x * direction_vector.x + direction_vector.y * direction_vector.y);
        if (magnitude > 0) {
            direction_vector.x /= magnitude;
            direction_vector.y /= magnitude;
        }

        // Ajusta la velocidad - factor que determina qué tan rápido se mueve el jugador
        direction_vector.x *= 3.0f; // Multiplicador de velocidad
        direction_vector.y *= 3.0f;

        // Visualiza la dirección con una línea desde el centro hacia donde apunta el mouse
        DrawLineEx(
            (Vector2){screen_center.x, screen_center.y},
            (Vector2){screen_center.x + direction_vector.x * 200, screen_center.y + direction_vector.y * 200},
            3, RED
        );
    }

    // ----- CÁLCULO DE LA CÁMARA (VISTA CENTRADA EN EL JUGADOR) -----
    
    // Calcula el desplazamiento para centrar la cámara en el jugador
    // El factor 20 es la escala de conversión entre coordenadas de mundo y pantalla
    float offsetX = screenWidth / 2.0f - (player_pos.x * 20);
    float offsetY = screenHeight / 2.0f - (player_pos.y * 20);

    // ----- RENDERIZADO DEL GRID DE FONDO (SISTEMA DE COORDENADAS) -----
    
    // Configuración del grid
    const int grid_size = 100; // Tamaño de cada celda del grid en pixels
    const Color grid_color = LIGHTGRAY;
    const Color axis_color = DARKGRAY;
    const int axis_thickness = 2;
    
    // Calcula qué líneas del grid son visibles en la pantalla actual
    int start_x = (int)floor((-offsetX) / grid_size) - 1;
    int end_x = (int)ceil((screenWidth - offsetX) / grid_size) + 1;
    int start_y = (int)floor((-offsetY) / grid_size) - 1;
    int end_y = (int)ceil((screenHeight - offsetY) / grid_size) + 1;
    
    // Dibuja líneas verticales del grid
    for(int i = start_x; i <= end_x; i++) {
        int x = (int)(i * grid_size + offsetX);
        if(x >= 0 && x < screenWidth) {
            DrawLine(x, 0, x, screenHeight, grid_color);
        }
    }
    
    // Dibuja líneas horizontales del grid
    for(int i = start_y; i <= end_y; i++) {
        int y = (int)(i * grid_size + offsetY);
        if(y >= 0 && y < screenHeight) {
            DrawLine(0, y, screenWidth, y, grid_color);
        }
    }
    
    // Dibuja el eje X con mayor grosor (línea horizontal en Y=0)
    int axis_y = (int)offsetY;
    if(axis_y >= 0 && axis_y < screenHeight) {
        DrawRectangle(0, axis_y - axis_thickness/2, screenWidth, axis_thickness, axis_color);
    }
    
    // Dibuja el eje Y con mayor grosor (línea vertical en X=0)
    int axis_x = (int)offsetX;
    if(axis_x >= 0 && axis_x < screenWidth) {
        DrawRectangle(axis_x - axis_thickness/2, 0, axis_thickness, screenHeight, axis_color);
    }

    // ----- RENDERIZADO DE ENTIDADES (JUGADORES Y COMIDA) -----
    
    // Itera a través de todas las entidades y las dibuja en pantalla
    for(int i = 0; i < num_entidades; i++) {
        // Omite entidades con ID 0 (marcadas para eliminación)
        if(entidades[i].id == 0) continue;

        // Convierte coordenadas de mundo a coordenadas de pantalla usando offset de cámara
        int x = (int)(entidades[i].pos.x * 20 + offsetX);
        int y = (int)(entidades[i].pos.y * 20 + offsetY);
        
        // VERIFICA VISIBILIDAD DE CADA ENTIDAD PARA DEBUGGING
        if (entidades[i].id >= 1000) {
            printf("\n🟢 VERIFICANDO COMIDA ID=%d: Pos=(%f,%f), Screen=(%d,%d), ¿Visible? %s\n",
                   entidades[i].id, entidades[i].pos.x, entidades[i].pos.y, x, y,
                   (x >= -500 && x < screenWidth + 500 && y >= -500 && y < screenHeight + 500) ? "SÍ" : "NO");
        }
        
        // DIBUJAR TODAS LAS ENTIDADES SIN IMPORTAR SU POSICIÓN (ELIMINAR VERIFICACIÓN DE RANGO TEMPORALMENTE)
        
        // Determina el color según el tipo de entidad
        Color entity_color;
        if (entidades[i].id == player_id) {
            entity_color = BLUE;  // Jugador local
        } else if (entidades[i].id >= 1000) {
            // COMIDA - DIBUJADA CON MÁXIMA VISIBILIDAD
            printf("🍔 DIBUJANDO COMIDA ID=%d en pos=(%d,%d)\n", entidades[i].id, x, y);
            
            // Círculo grande y rojo
            DrawCircle(x, y, 25, RED);
            
            // Contorno para destacar
            DrawCircleLines(x, y, 27, BLACK);
            
            // Texto con ID y coordenadas para debug
            char id_text[32];
            sprintf(id_text, "ID:%d", entidades[i].id);
            DrawText(id_text, x - MeasureText(id_text, 14)/2, y - 15, 14, WHITE);
            
            continue;
        } else {
            entity_color = RED;   // Otros jugadores
        }
        
        // Efectos visuales especiales para el jugador local
        if (entidades[i].id == player_id) {
            // Halo alrededor del jugador para destacarlo
            DrawCircle(x, y, 15, Fade(SKYBLUE, 0.5f));
            
            // Flecha que indica la dirección de movimiento actual
            if (entidades[i].dir.x != 0 || entidades[i].dir.y != 0) {
                float dir_magnitude = sqrt(entidades[i].dir.x * entidades[i].dir.x + 
                                          entidades[i].dir.y * entidades[i].dir.y);
                if (dir_magnitude > 0) {
                    float nx = entidades[i].dir.x / dir_magnitude;
                    float ny = entidades[i].dir.y / dir_magnitude;
                    DrawLineEx(
                        (Vector2){x, y},
                        (Vector2){x + nx * 20, y + ny * 20},
                        2, GREEN
                    );
                }
            }
        }
        
        // Dibuja la célula con su tamaño real (crece al comer)
        DrawCircle(x, y, entidades[i].tamanio, entity_color);
        
        // Etiqueta con el ID sobre cada jugador
        char id_text[32];
        sprintf(id_text, "%d", entidades[i].id);
        DrawText(id_text, x - MeasureText(id_text, 16)/2, y - 25, 16, BLACK);
    }
    
    // ----- INSTRUCCIONES Y AYUDA VISUAL -----
    
    // Muestra instrucciones en la parte inferior de la pantalla
    DrawText("Mueve el ratón manteniendo pulsado el botón izquierdo", 20, screenHeight - 60, 16, DARKGRAY);
    DrawText("Tu celula solo se movera si pulsas el click izquierdo", 20, screenHeight - 40, 16, DARKGRAY);
    
    // ----- PANEL DE ESTADÍSTICAS E INFORMACIÓN -----
    
    // Panel semitransparente para mostrar información
    DrawRectangle(screenWidth - 250, 10, 240, 140, Fade(LIGHTGRAY, 0.7f));
    DrawRectangleLines(screenWidth - 250, 10, 240, 140, GRAY);
    
    // Información del jugador: ID y posición
    DrawText("Tu ID: ", screenWidth - 240, 20, 20, BLACK);
    DrawText(TextFormat("%d", player_id), screenWidth - 160, 20, 20, BLUE);
    
    sprintf(buffer, "Pos: (%.2f, %.2f)", player_pos.x, player_pos.y);
    DrawText(buffer, screenWidth - 240, 45, 16, BLACK);
    
    // Estadísticas del protocolo RUDP (fiabilidad)
    Color estado_color = conexion_activa ? GREEN : RED;
    DrawText("Estado RUDP:", screenWidth - 240, 65, 16, BLACK);
    DrawText(conexion_activa ? "CONECTADO" : "ESPERANDO", screenWidth - 130, 65, 16, estado_color);
    
    // Contadores de paquetes
    sprintf(buffer, "Paquetes Rx: %d", paquetes_recibidos);
    DrawText(buffer, screenWidth - 240, 85, 16, BLACK);
    
    sprintf(buffer, "Paquetes Tx: %d", paquetes_enviados);
    DrawText(buffer, screenWidth - 240, 105, 16, BLACK);
    
    // Estadísticas de pérdida de paquetes con porcentaje
    sprintf(buffer, "Perdidos: %d (%.1f%%)", paquetes_perdidos, 
            paquetes_enviados > 0 ? (paquetes_perdidos * 100.0f / paquetes_enviados) : 0.0f);
    DrawText(buffer, screenWidth - 240, 125, 16, BLACK);
    
    // Contador de FPS para monitorear rendimiento
    DrawFPS(10, 10);

    // ----- AYUDAS VISUALES ADICIONALES -----
    
    // Círculo amarillo que marca el centro de la pantalla (referencia)
    DrawCircle(screenWidth/2, screenHeight/2, 5, YELLOW);
    DrawText("Centro de pantalla", screenWidth/2 + 10, screenHeight/2, 16, YELLOW);

    // Información de posición y controles en la parte superior
    char info[256];
    sprintf(info, "Tu posición: (%.2f, %.2f) | Usar click izquierdo para mover", 
            player_pos.x, player_pos.y);
    DrawText(info, 10, 40, 18, DARKBLUE);

    // Etiqueta explicativa para el grid
    DrawText("Área visible (líneas grises)", 10, 70, 18, GRAY);

    // ----- EFECTO DE ESTELA DE MOVIMIENTO -----
    
    // Almacena las últimas 10 posiciones para crear efecto de estela
    static Vector2D last_positions[10] = {0}; // Historial de posiciones
    static int pos_index = 0;                 // Índice circular para el historial

    // Actualiza el historial solo si se encontró el jugador
    if (player_found) {
        // Guarda la posición actual en el historial
        last_positions[pos_index] = player_pos;
        pos_index = (pos_index + 1) % 10; // Avanza circularmente en el historial
        
        // Dibuja líneas entre posiciones anteriores para crear efecto de estela
        for (int i = 0; i < 9; i++) {
            int idx1 = (pos_index + i) % 10;
            int idx2 = (pos_index + i + 1) % 10;
            
            // Solo dibuja si las posiciones son válidas (no son cero)
            if (last_positions[idx1].x != 0 || last_positions[idx1].y != 0) {
                // Convierte coordenadas de mundo a pantalla
                int x1 = (int)(last_positions[idx1].x * 20 + offsetX);
                int y1 = (int)(last_positions[idx1].y * 20 + offsetY);
                int x2 = (int)(last_positions[idx2].x * 20 + offsetX);
                int y2 = (int)(last_positions[idx2].y * 20 + offsetY);
                
                // Dibuja línea with transparencia decreciente para efecto de desvanecimiento
                DrawLine(x1, y1, x2, y2, Fade(BLUE, 0.3f - (i * 0.03f)));
            }
        }
    }

    // ----- CÍRCULOS DE PRUEBA PARA VERIFICAR ESCALA Y OFFSET -----
    
    // Descomentar para dibujar círculos de pruebaS
    /*
    DrawCircle((int)(0 * 20 + offsetX), (int)(0 * 20 + offsetY), 20, RED); // Origen
    DrawCircle((int)(10 * 20 + offsetX), (int)(10 * 20 + offsetY), 20, BLUE); // Posición (10,10)
    DrawCircle((int)(-10 * 20 + offsetX), (int)(-10 * 20 + offsetY), 20, GREEN); // Posición (-10,-10)
    */

    // Comida de prueba en posiciones fijas (sólo para debugging)
    DrawCircle((int)(5 * 20 + offsetX), (int)(5 * 20 + offsetY), 15, GREEN);
    DrawCircleLines((int)(5 * 20 + offsetX), (int)(5 * 20 + offsetY), 17, BLACK);
    DrawCircle((int)(-5 * 20 + offsetX), (int)(-5 * 20 + offsetY), 15, GREEN);
    DrawCircleLines((int)(-5 * 20 + offsetX), (int)(-5 * 20 + offsetY), 17, BLACK);

    // Visualización forzada de comida - MOVER AQUÍ, ANTES DE EndDrawing()
    for (int i = 0; i < num_entidades; i++) {
        if (entidades[i].id >= 1000) {
            // Coordenadas absolutas en pantalla (sin offset)
            DrawCircle(400, 300 + i*30, 15, RED);
            char id_text[32];
            sprintf(id_text, "Comida ID=%d (%f,%f)", entidades[i].id, entidades[i].pos.x, entidades[i].pos.y);
            DrawText(id_text, 420, 295 + i*30, 16, BLACK);
        }
    }


    // Finaliza el frame de dibujo
    EndDrawing();

    // Retorna el vector de dirección para que el cliente lo envíe al servidor
    return direction_vector;
}

/**
 * Verifica si el usuario ha solicitado cerrar la ventana.
 * @return 1 si se debe cerrar, 0 en caso contrario
 */
int debe_cerrar_ventana() {
    return WindowShouldClose(); // Detecta si se presionó ESC o el botón de cerrar
}

/**
 * Libera recursos y cierra la ventana gráfica.
 * Se llama antes de terminar el programa.
 */
void cerrar_dibujo() {
    CloseWindow(); // Cierra la ventana de Raylib y libera recursos
}
