# Pong em C

Jogo estilo Pong feito em C com a biblioteca raylib.

## Recursos
- Modo 2 jogadores e modo contra a IA (3 dificuldades)
- Power-ups: barra grande, barra pequena no oponente, bola lenta e controles invertidos
- Menu e tela de controles

## Controles
- Jogador 1: W / S
- Jogador 2: setas cima / baixo
- Menus: W/S ou setas + ENTER

## Como compilar (Windows, MSYS2)
1. Instale a raylib: `pacman -S mingw-w64-ucrt-x86_64-raylib`
2. Compile: `gcc jogo.c -o jogo.exe -lraylib -lopengl32 -lgdi32 -lwinmm`
3. Rode: `.\jogo.exe`