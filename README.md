
# Agar.io Simplificado con Winsockets en C (agar-winsock-c)

Este proyecto es una implementación simplificada del juego multijugador **Agar.io**, desarrollada en C utilizando **Winsockets**. Utiliza una arquitectura cliente-servidor para permitir que múltiples jugadores se conecten, se muevan en un entorno compartido y compitan por comida y tamaño. Incluye además herramientas de prueba y visualización en Python.

## Características

- Arquitectura cliente-servidor con Winsockets (TCP)
- Manejo de múltiples clientes concurrentes
- Detección de colisiones y lógica de "comer"
- Consola como interfaz para cliente
- Scripts de prueba automática y bots simulados
- Visualización experimental con Pygame

## Estructura del proyecto

```
├── server.c              # Código del servidor
├── client.c              # Código del cliente
├── client.exe            # Ejecutable cliente
├── server.exe            # Ejecutable servidor
├── bot_client.py         # Cliente simulado para bots
├── client_pygame.py      # Cliente con visualización gráfica (experimental)
├── autotest.py           # Script de pruebas automatizadas
├── run_bots.py           # Ejecuta múltiples bots para pruebas
├── run_bots.bat          # Script batch para lanzar bots en Windows
├── viewer.py             # Herramienta de visualización de estado
├── .vscode/              # Configuración de entorno para Visual Studio Code
└── Memoria.docx          # Documento con especificación y resultados
```

## Cómo ejecutar

### Requisitos

- Windows con soporte para Winsockets
- Compilador C compatible (Visual Studio recomendado)
- Python 3.x (para los scripts y visualizadores)

### Compilación

Puedes compilar los archivos `.c` con Visual Studio o usando este comando (en Windows):

```bash
cl /D_CRT_SECURE_NO_WARNINGS server.c ws2_32.lib
cl /D_CRT_SECURE_NO_WARNINGS client.c ws2_32.lib
```

### Ejecución

1. Inicia el servidor:
   ```bash
   server.exe
   ```

2. Abre otra terminal y lanza uno o más clientes:
   ```bash
   client.exe
   ```

3. (Opcional) Ejecuta bots para pruebas:
   ```bash
   python run_bots.py
   ```

## Pruebas

El sistema incluye scripts para pruebas automatizadas y simulación de carga usando bots. Las pruebas verifican la conexión simultánea, movimiento, colisiones y estabilidad general del sistema.


## Licencia

Este proyecto está licenciado bajo la Licencia MIT. Consulta el archivo `LICENSE` para más detalles.

## Créditos

Desarrollado como parte de un proyecto académico para explorar programación en red y diseño de juegos multijugador.

Inspirado en el juego original [Agar.io](https://agar.io).
