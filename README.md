# Agar.io Clone in C using WinSock and Raylib

This project is a basic 2D multiplayer game inspired by **Agar.io**, built entirely in C using **WinSock** for networking and **Raylib** for graphics rendering. It features a minimal implementation of a real-time multiplayer environment where players can move, collide, and grow.

## Features

- Server-client architecture over UDP (WinSock)
- Custom binary serialization for game messages
- Data integrity validation
- Multithreaded server to handle multiple clients
- Real-time 2D rendering with Raylib
- Dynamic object growth and collision detection

## Requirements

- Windows OS
- C compiler with WinSock and Raylib support (e.g., GCC, MinGW, MSVC)
- [Raylib](https://www.raylib.com/) (included static lib & header provided)
- Basic command-line usage

## Getting Started

### 1. Clone the Repository

```bash
git clone https://github.com/javisiierra/agar-winsock-c.git
cd agar-winsock-c
```

### 2. Build the Project

Use a terminal or your preferred C build system. Here’s how to compile manually with GCC:

```bash
   	gcc servidor.c -o servidor.exe -lopengl32 -lgdi32 -lwinmm -lws2_32 
  	gcc cliente.c dibujo.c -o cliente.exe -L"./lib" -lraylib -lopengl32 -lgdi32 -lwinmm -lws2_32 ```

> **Note:** Ensure `libraylib.a` and `raylib.h` are placed in `/lib` and `/include`, respectively.

### 3. Run the Game

Start the server first:

```bash
./servidor
```

Then launch one or more clients:

```bash
./cliente
```

Clients will connect to the server over UDP and display a small window where players can move around and grow by consuming food or other players.

## Project Structure

```
.
├── cliente.c            # Client implementation
├── servidor.c           # Server implementation
├── dibujo.[c|h]         # Rendering and drawing logic
├── cola_generica.[c|h]  # Generic queue for internal use
├── miLinkedList.c       # Simple linked list logic
├── mensajes.h           # Network message format definitions
├── include/             # Includes raylib.h
├── lib/                 # Contains libraylib.a
└── .vscode/             # Visual Studio Code settings (optional)
```

## Limitations

- Not secure (no encryption or authentication)
- No graphical UI or menu system
- Intended for LAN play (local IP, no NAT traversal)
- Not scalable beyond small groups

## License

This project is licensed under the Apache 2.0 License. See `LICENSE` for details.

## Credits

- Inspired by [Agar.io](https://agar.io/)
- Networking via WinSock (Windows)
- Graphics via [Raylib](https://www.raylib.com/)