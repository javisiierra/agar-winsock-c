// --- Contenido de: cola_generica.c ---
#include "cola_generica.h"
#include <stdlib.h> // Para malloc y free
#include <stdio.h>  // Para printf en caso de error

void cola_inicializar(ColaGenerica* q) {
    q->cabeza = q->cola = NULL;
    InitializeCriticalSection(&q->seccion_critica);
    InitializeConditionVariable(&q->variable_condicion);
}

void cola_encolar(ColaGenerica* q, void* dato) {
    NodoGenerico* nodo = (NodoGenerico*)malloc(sizeof(NodoGenerico));
    if (!nodo) {
        fprintf(stderr, "Error: Fallo al alocar memoria para un nodo de la cola.\n");
        return;
    }
    nodo->dato = dato;
    nodo->siguiente = NULL;

    EnterCriticalSection(&q->seccion_critica);
    if (q->cola) {
        q->cola->siguiente = nodo;
        q->cola = nodo;
    } else {
        q->cabeza = q->cola = nodo;
    }
    LeaveCriticalSection(&q->seccion_critica);

    // Despierta a un posible hilo que esté esperando
    WakeConditionVariable(&q->variable_condicion);
}

void* cola_desencolar(ColaGenerica* q) {
    EnterCriticalSection(&q->seccion_critica);

    if (q->cabeza == NULL) {
        LeaveCriticalSection(&q->seccion_critica);
        return NULL; // Indica que la cola está vacía
    }

    NodoGenerico* nodo = q->cabeza;
    void* dato = nodo->dato;
    q->cabeza = nodo->siguiente;

    if (q->cabeza == NULL) {
        q->cola = NULL;
    }

    LeaveCriticalSection(&q->seccion_critica);

    // Libera la memoria del nodo (el contenedor), pero no del dato.
    free(nodo);

    return dato;
}

bool cola_esta_vacia(ColaGenerica* q) {
    EnterCriticalSection(&q->seccion_critica);
    bool vacia = (q->cabeza == NULL);
    LeaveCriticalSection(&q->seccion_critica);
    return vacia;
}

void cola_destruir(ColaGenerica* q, void (*funcion_liberar_dato)(void* dato)) {
    NodoGenerico* actual = q->cabeza;
    while (actual != NULL) {
        NodoGenerico* siguiente = actual->siguiente;
        
        if (funcion_liberar_dato && actual->dato) {
            funcion_liberar_dato(actual->dato);
        }
        
        free(actual);
        actual = siguiente;
    }
    q->cabeza = q->cola = NULL;

    DeleteCriticalSection(&q->seccion_critica);
}