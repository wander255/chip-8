#include <SDL2/SDL_error.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_video.h>
#include <chrono>
#include <iostream>
#include <cstdint>
#include <fstream>
#include <string>
#include <cstdlib>
#include <thread>
#include <SDL2/SDL.h>

const uint8_t chip8_fontset[80] = {
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

class Chip8 {
    public:
        uint8_t memory[4096];
        uint8_t V[16];
        uint16_t I;
        uint16_t pc;
        uint16_t stack[16];
        uint8_t sp;
        uint8_t delay_timer;
        uint8_t sound_timer;
        uint8_t kbd[16];
        uint32_t display[64*32];
        bool draw_flag;

        void init() {
            pc = 0x200;
            I = 0;
            sp = 0;
            delay_timer = 0;
            sound_timer = 0;
            draw_flag = 0;
            for (int i = 0; i != 16; i++) {
                V[i] = 0;
                kbd[i] = 0;
                stack[i] = 0;
            }
            for (int i = 0; i != 4096; i++) {
                memory[i] = 0;
            }
            for (int i = 0; i != 64*32; i++) {
                display[i] = 0;
            }

            for (int i = 0; i < 80; i++) {
                memory[i] = chip8_fontset[i];
            }
        }
        void updateTimers() {
            if (delay_timer > 0) {
                delay_timer--;
            }
            if (sound_timer > 0 ) {
                sound_timer--;
            }
        }
        bool loadROM(const std::string& filename) {
            
            std::ifstream file(filename, std::ios::binary | std::ios::ate);
            if (!file.is_open()) {
                std::cerr << "Не удалось открыть файл " << filename << std::endl; 
                return false;
            }
            std::streampos size = file.tellg();
            if (size > (4096 - 0x200)) {
                std::cerr << "Слишком большой размер файла! (" << size << ")" << std::endl;
                return false;
            }
            file.seekg(0, std::ios::beg);

            file.read(reinterpret_cast<char*>(&memory[0x200]), size);
            file.close();
            return true;

        }

        void cycle() {
            uint16_t opcode = (memory[pc] << 8) | memory[pc+1];
            pc+=2;
            uint16_t NNN = opcode & 0x0FFF;
            uint8_t X   = (opcode & 0x0F00) >> 8;
            uint8_t Y   = (opcode & 0x00F0) >> 4;
            uint8_t KK  = opcode & 0x00FF;
            switch (opcode & 0xF000) {
                case 0x0000:
                    switch (KK) {
                        case 0xEE:
                            sp--;
                            pc = stack[sp];
                            break;
                    }
                    break;
                case 0xA000:
                    I = NNN;
                    break;
                case 0x1000:
                    pc = NNN;
                    break;
                case 0x2000:
                    stack[sp] = pc;
                    sp++;
                    pc = NNN;
                    break;
                case 0x3000:
                    if (V[X] == KK) {
                        pc+=2;
                    }
                    break;
                case 0x4000:
                    if (V[X] != KK) {
                        pc+=2;
                    }
                    break;
                case 0x5000:
                    if (V[X] == V[Y]) {
                        pc+=2;
                    }
                    break;
                case 0x6000:
                    V[X] = KK;
                    break;
                case 0x7000:
                    V[X] += KK;
                    break;
                case 0x8000:
                    switch (opcode & 0x000F) {
                        case 0x0:
                            V[X] = V[Y];
                            break;
                        case 0x1:
                            V[X] = V[X] | V[Y];
                            break;
                        case 0x2:
                            V[X] = V[X] & V[Y];
                            break;
                        case 0x3:
                            V[X] = V[X] ^ V[Y];
                            break;
                        case 0x4: {
                            uint16_t sum = V[X] + V[Y];
                            if (sum > 255) {
                                V[15] = 1;
                            } else {
                                V[15] = 0;
                            }
                            V[X] = sum & 0xff;
                            break;
                        }
                        
                        case 0x5:
                            if (V[X] >= V[Y]) {
                                V[15] = 1;
                            }
                            else {
                                V[15] = 0;
                            }

                            V[X] -= V[Y];
                            break;
                        case 0x6:
                            V[15] = V[X] & 0x1;
                            V[X] >>= 1;
                            break;

                        case 0xE:
                            V[15] = (V[X] & 0x80) >> 7;
                            V[X] <<= 1;
                            break;
                            
                    }
                    break;
                case 0x9000:
                    if (V[X] != V[Y]) {
                        pc += 2;
                    }
                    break;    
                case 0xB000:
                    pc = NNN + V[0];
                    break;
                case 0xF000:
                    switch (KK) {
                        case 0x07:
                            V[X] = delay_timer;
                            break;
                        case 0x15:
                            delay_timer = V[X];
                            break;
                        case 0x18:
                            sound_timer = V[X];
                            break;
                        case 0x1E:
                            I += V[X];
                            break;
                        case 0x29:
                            I = V[X] * 5;
                            break;
                        case 0x33:
                            memory[I] = V[X] / 100;
                            memory[I+1] = (V[X] / 10) % 10;
                            memory[I+2] = V[X] % 10;
                            break;
                        case 0x55:
                            for (int i = 0; i <= X; i++) {
                                memory[I+i] = V[i];
                            }
                            
                            break;
                        case 0x65:
                            for (int i = 0; i <= X; i++) {
                                V[i] = memory[I+i];
                            }

                            break;
                        case 0x0A: {
                            bool key = false;
                            for (int i = 0; i < 16; i++) {
                                if (kbd[i] == 1) {
                                    key = true;
                                    V[X] = i;
                                }
                            }
                            if (!key) {
                                pc -= 2;
                            }
                            break;
                        }
                            
                        
                    }
                    break;
                case 0xC000:
                    V[X] = rand() & KK;
                    break;
                case 0xE000:
                    switch (KK) {
                        case 0x9E:
                            if (kbd[V[X]] == 1) {
                                pc+=2;
                            }
                            break;
                        case 0xA1:
                            if (kbd[V[X]] == 0) {
                                pc+=2;
                            }
                            break;
                    }
                    break;
                case 0xD000:
                    V[15] = 0;
                    draw_flag = false;
                    for (int row = 0; row < (opcode & 0x000F); row++) {
                        uint8_t sprite_byte = memory[I + row];
                        for (int col = 0; col < 8; col++) {
                            uint8_t symbol = (sprite_byte & (0x80 >> col));
                            if (symbol) {
                                
                                uint8_t x = (V[X] + col) % 64;
                                uint8_t y = (V[Y]+ row) % 32;
                                if (display[x + (y*64)] == 0xFFFFFFFF) {
                                    V[15] = 1;
                                }
                            display[x + (y*64)] ^= 0xFFFFFFFF;
                        }
                    }
            }       draw_flag = true;
                    break;
            
        }
    }

        Chip8() {
            init();
        }
    
};

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Укажите имя файла! " << argv[0] << " <rom_name.ch8>" << std::endl;
        return 1;
    }
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Ошибка инициализации SDL: " << SDL_GetError() << std::endl;
        return 1;
    }
    Chip8 emu;
    emu.init();
    if (!emu.loadROM(argv[1])) {
        std::cerr << "Произошла ошибка! Повторите попытку..." << std::endl;
        return 1;
    }
    bool running = true;
    
    SDL_Event event;
    SDL_Window* window = SDL_CreateWindow("CHIP-8", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_SHOWN);
    if (!window) {
        std::cout << "Не удалось создать окно! " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cout << "Не удалось создать ренлерер! " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_Rect rect;
    rect.w = 10;
    rect.h = 10;
    
    while (running) {
        
        while(SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_1: emu.kbd[0x1] = 1; break;
                    case SDLK_q: emu.kbd[0x4] = 1; break;
                    case SDLK_4: emu.kbd[0xC] = 1; break;
                    case SDLK_r: emu.kbd[0xD] = 1; break; 
                }
            }
            if (event.type == SDL_KEYUP) {
                switch (event.key.keysym.sym) {
                    case SDLK_1: emu.kbd[0x1] = 0; break;
                    case SDLK_q: emu.kbd[0x4] = 0; break;
                    case SDLK_4: emu.kbd[0xC] = 0; break;
                    case SDLK_r: emu.kbd[0xD] = 0; break;
                }
            }

        }
        emu.cycle();
        emu.updateTimers();
        
        if (emu.draw_flag) {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            for (int y = 0; y < 32; y++) {
                for (int x = 0; x < 64; x++) {
                    int shit = x+(y*64);
                    if (emu.display[shit] == 0xFFFFFFFF) {
                        rect.x = x*10;
                        rect.y = y*10;
                        SDL_RenderFillRect(renderer, &rect);
                    }
                }
            }
            SDL_RenderPresent(renderer);
            emu.draw_flag = false;
            
        }
        std::this_thread::sleep_for(std::chrono::microseconds(1500));
    }
    

    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();
    return 0;
}