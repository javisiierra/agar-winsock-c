// --- Contenido de: cola_generica.h ---
#ifndef COLA_GENERICA_H
#define COLA_GENERICA_H

// Incluimos windows.h por CRITICAL_SECTION y CONDITION_VARIABLE
#include <windows.h>
#include <stdbool.h> // Para usar 'bool' en lugar de 'int' para valores booleanos

// El nodo de la cola. Almacena un puntero a "void",
// lo que le permite apuntar a cualquier tipo de dato.
typedef struct NodoGenerico {
    void* dato;
    struct NodoGenerico* siguiente;
} NodoGenerico;

// La estructura principal de la cola.
typedef struct {
    NodoGenerico* cabeza;
    NodoGenerico* cola;
    CRITICAL_SECTION seccion_critica;
    CONDITION_VARIABLE variable_condicion;
} ColaGenerica;

/**
 * @brief Inicializa una cola genérica.
 * @param q Puntero a la cola a inicializar.
 */
void cola_inicializar(ColaGenerica* q);

/**
 * @brief Agrega un puntero a un dato al final de la cola.
 * @param q Puntero a la cola.
 * @param dato Puntero al dato a encolar. El dato debe ser alocado dinámicamente (con malloc).
 */
void cola_encolar(ColaGenerica* q, void* dato);

/**
 * @brief Saca un puntero a un dato del inicio de la cola.
 * El llamador se hace responsable de liberar la memoria del dato devuelto.
 * @param q Puntero a la cola.
 * @return Puntero al dato, o NULL si la cola está vacía.
 */
void* cola_desencolar(ColaGenerica* q);

/**
 * @brief Verifica si la cola está vacía.
 * @param q Puntero a la cola.
 * @return true si la cola está vacía, false en caso contrario.
 */
bool cola_esta_vacia(ColaGenerica* q);

/**
 * @brief Destruye la cola, liberando todos los nodos y opcionalmente los datos contenidos.
 * @param q Puntero a la cola a destruir.
 * @param funcion_liberar_dato Un puntero a una función que sabe cómo liberar la memoria
 *                               del tipo de dato específico almacenado en la cola.
 *                               Si es NULL, los datos no se liberarán, solo los nodos.
 */
void cola_destruir(ColaGenerica* q, void (*funcion_liberar_dato)(void* dato));

#endif // COLA_GENERICA_H