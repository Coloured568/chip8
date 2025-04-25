#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <SDL2/SDL.h>
/* ---------------- */
#define MEM 4096
#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 32
#define SCALE 10

enum opcodes {
    CLS = 0x00E0, // clear screen
    JUMP = 0x1000, // jump
    SET_VX = 0x6000, // set register VX
    ADD_VX = 0x7000, // add value to register VX
    SET_I = 0xA000, // set index register I
    DRAW = 0xD000 // display/draw
};

const unsigned char font_data[] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

typedef struct {
    unsigned char memory[MEM]; // 4KB of memory
    unsigned char V[16]; // 16 registers
    unsigned short I; // index register
    unsigned short PC; // program counter
    unsigned char delay_timer; // delay timer
    unsigned char sound_timer; // sound timer
    unsigned short stack[16]; // stack
    unsigned short SP; // stack pointer
} CPU;

uint8_t fetch(CPU *cpu){
    return cpu->memory[cpu->PC];
}

void initialize_sdl(SDL_Window **window, SDL_Renderer **renderer) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        exit(1);
    }

    *window = SDL_CreateWindow(
        "CHIP-8 Emulator",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH * SCALE, SCREEN_HEIGHT * SCALE,
        SDL_WINDOW_SHOWN
    );

    if (!*window) {
        fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_Quit();
        exit(1);
    }

    *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED);
    if (!*renderer) {
        fprintf(stderr, "Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(*window);
        SDL_Quit();
        exit(1);
    }
}

void render_display(SDL_Renderer *renderer, uint8_t display[SCREEN_HEIGHT][SCREEN_WIDTH]) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Black background
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // White pixels
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            if (display[y][x]) {
                SDL_Rect rect = { x * SCALE, y * SCALE, SCALE, SCALE };
                SDL_RenderFillRect(renderer, &rect);
            }
        }
    }

    SDL_RenderPresent(renderer);
}

void decode(uint8_t opcode, CPU *cpu, uint8_t display[SCREEN_HEIGHT][SCREEN_WIDTH]){
    switch(opcode){
        case CLS: { // Clear the display
            for (int y = 0; y < SCREEN_HEIGHT; y++) {
                for (int x = 0; x < SCREEN_WIDTH; x++) {
                    display[y][x] = 0;
                }
            }
            break;
        }
        case JUMP:{
            uint8_t addr = fetch(cpu);
            cpu->PC = addr;
            printf("Jump to address: 0x%X\n", addr);
            break;
        }
        case SET_VX: {
            uint8_t reg = fetch(cpu);
            uint8_t value = fetch(cpu);
            cpu->V[reg] = value;
            printf("Set V[%d] to %d\n", reg, value);
            break;
        }
        case ADD_VX:
            uint8_t reg = fetch(cpu);
            uint8_t value = fetch(cpu);
            cpu->V[reg] += value;
            printf("Add %d to V[%d], new value: %d\n", value, reg, cpu->V[reg]);
            break;
        case SET_I:
            break;
        case DRAW: {
            uint8_t x = cpu->V[0]; // X-coordinate
            uint8_t y = cpu->V[1]; // Y-coordinate
            uint8_t height = opcode & 0x000F; // Height of the sprite (last nibble of the opcode)
            uint8_t pixel;

            cpu->V[0xF] = 0; // Reset collision flag

            for (int row = 0; row < height; row++) {
                pixel = cpu->memory[cpu->I + row]; // Fetch sprite data from memory
                for (int col = 0; col < 8; col++) {
                    if ((pixel & (0x80 >> col)) != 0) { // Check if the current bit is set
                        uint8_t *screen_pixel = &display[(y + row) % SCREEN_HEIGHT][(x + col) % SCREEN_WIDTH];
                        if (*screen_pixel == 1) {
                            cpu->V[0xF] = 1; // Set collision flag
                        }
                        *screen_pixel ^= 1; // XOR the pixel
                    }
                }
            }

            render_display(renderer, display); // Render the updated display
            break;
        }
        default:
            printf("Unknown opcode: 0x%X\n", opcode);
            break;
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <ROM file>\n", argv[0]);
        return 1;
    }

    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    initialize_sdl(&window, &renderer);

    uint8_t display[SCREEN_HEIGHT][SCREEN_WIDTH] = {0}; // Initialize display
    CPU cpu = {0}; // Initialize CPU

    // Load font data into memory
    for (int i = 0; i < sizeof(font_data); i++) {
        cpu.memory[i] = font_data[i];
    }

    // Load the ROM file
    FILE *rom = fopen(argv[1], "rb");
    if (!rom) {
        fprintf(stderr, "Failed to open ROM file: %s\n", argv[1]);
        return 1;
    }

    // Read ROM contents into memory starting at 0x200
    size_t bytes_read = fread(&cpu.memory[0x200], 1, MEM - 0x200, rom);
    fclose(rom);

    if (bytes_read == 0) {
        fprintf(stderr, "Failed to read ROM file or ROM is empty.\n");
        return 1;
    }

    printf("Loaded %zu bytes from ROM: %s\n", bytes_read, argv[1]);

    cpu.PC = 0x200; // Set the program counter to the start of the program
    cpu.I = 0x200; // Set I to the starting address of the sprite
    bool running = true;

    while (running) {
        decode(cpu.memory[cpu.PC], &cpu, display);
        render_display(renderer, display);

        // Handle SDL events to allow quitting
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        SDL_Delay(16); // ~60 FPS
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}