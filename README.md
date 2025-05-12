# Agar.io 2D - Versión por Consola y Pygame

Este proyecto es un clon simplificado de Agar.io con gráficos en 2D usando Pygame en el cliente, y un servidor en C para manejar el estado del juego. Los jugadores pueden conectarse a través de red local y competir por recolectar comida y crecer. También es posible agregar **bots automáticos** como oponentes.

## 🎮 ¿Cómo jugar?

1. **Inicia el servidor**
   - Compila `server.c` en Windows:
     ```bash
     gcc server.c -o server -lws2_32
     ```
   - Luego ejecuta el servidor:
     ```bash
     ./server
     ```

2. **Ejecuta el cliente Pygame**
   - Asegúrate de tener Python 3 y `pygame` instalado:
     ```bash
     pip install pygame
     ```
   - Ejecuta el cliente:
     ```bash
     python client_pygame.py
     ```

3. **Controles del jugador**
   - Usa las **flechas del teclado** para mover tu célula:
     - ⬆️ Arriba
     - ⬇️ Abajo
     - ⬅️ Izquierda
     - ➡️ Derecha

   - Come comida amarilla para crecer.
   - Puedes absorber a otros jugadores si eres más grande que ellos.

---

## 🤖 Añadir bots (opcional)

### Opción 1: Ejecutar manualmente varios bots

Puedes lanzar uno o más **bots automáticos** que se conectan al servidor y se mueven aleatoriamente.

1. Crea un archivo llamado `bot_client.py` con este contenido:
   *(Este archivo ya te fue proporcionado anteriormente)*

2. Ejecuta uno o varios bots desde diferentes terminales:
   ```bash
   python bot_client.py
   ```

### Opción 2: Ejecutar múltiples bots automáticamente

#### Script en Python (`run_bots.py`)

Este script lanza varios bots automáticamente:

```python
import subprocess
import time

BOT_COUNT = 5  # Cambia este número para más bots

for i in range(BOT_COUNT):
    subprocess.Popen(["python", "bot_client.py"])
    time.sleep(0.2)
```

Ejecuta con:

```bash
python run_bots.py
```

#### Script para Windows (`run_bots.bat`)

Crea un archivo llamado `run_bots.bat`:

```bat
@echo off
setlocal enabledelayedexpansion

set BOTS=5

for /L %%i in (1,1,%BOTS%) do (
    start python bot_client.py
    timeout /t 1 >nul
)
```

Ejecuta haciendo doble clic o desde la terminal:

```cmd
run_bots.bat
```

Esto abrirá varias ventanas, cada una ejecutando un bot.



Puedes lanzar uno o más **bots automáticos** que se conectan al servidor y se mueven aleatoriamente.


## 🌐 Modo multijugador local

Puedes tener hasta **10 jugadores conectados al mismo tiempo** en red local.

1. Ejecuta el servidor en una máquina.
2. En cada dispositivo (cliente), asegúrate de que `client_pygame.py` esté configurado para conectarse a la IP del servidor:
   ```python
   sock.connect(("IP_DEL_SERVIDOR", 12345))
   ```
3. Ejecuta `client_pygame.py` en cada cliente.

---

## 🛠 Requisitos

- **Servidor:**
  - Windows
  - Compilador `gcc` con `ws2_32.lib`
- **Cliente:**
  - Python 3.7+
  - `pygame` (`pip install pygame`)

---

## 📌 Notas

- El servidor envía el estado del juego a todos los clientes cada 200 ms.
- Si un jugador cierra la ventana, se desconecta automáticamente.
- El juego no guarda estado; al cerrar el servidor, todo se reinicia.

---

## 📄 Licencia

Este proyecto es de uso libre para fines educativos y recreativos.