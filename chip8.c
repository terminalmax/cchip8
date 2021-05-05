#define _CRT_SECURE_NO_DEPRECATE //Visual Studio does not like fopen or scanf  > :|

#include<SDL.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>

//Resolution of CHIP-8
#define FRAME_RATE 10
#define SCREEN_HEIGHT 32
#define SCREEN_WIDTH 64


/*  -- CHIP-8 --
    & is used to 'mask' the bits and >> is used to set them in the right place.
*/
#define _X      ((opcode & 0x0F00) >> 8)
#define _Y      ((opcode & 0x00F0) >> 4)
#define _N      (opcode & 0x000F)
#define _NN     (opcode & 0x00FF)
#define _NNN    (opcode & 0x0FFF)
#define VX registers[_X]
#define VY registers[_Y]
#define VF registers[0xF]

//Chip8 execution starts from memory address 0x200
#define STARTING_ADDRESS 0x200
#define ENDING_ADDRESS   0xFFF

//Chip8 Variables
uint16_t opcode = 0;                         //Holds current opcode. Each instruction is exactly 2bytes (16bits) long
uint8_t Memory[0x1000] = { 0 };                //CHIP-8 has 4096 bytes of memory
uint16_t program_counter = STARTING_ADDRESS; //Holds the location in memory for the next instruction to be executed

uint8_t registers[16] = { 0 };    //16 8bit registers 
uint16_t index_register = 0;    //1 16bit index register

uint16_t stack[16] = { 0 };   //The stack holds on to the current location before making a jump.
uint8_t stack_pointer = 0;  //Holds the top location of stack

uint8_t delay_timer = 0;
uint8_t sound_timer = 0;

uint32_t display[SCREEN_HEIGHT * SCREEN_WIDTH] = { 0 }; //The display array is used to store pixel information
int keys[16]; //Chip-8 has 16 keys

void initializeChip()
{
    srand(time(0));

    opcode = 0;
    memset(Memory, 0, sizeof(Memory));
    program_counter = STARTING_ADDRESS;

    memset(registers, 0, sizeof(registers));
    index_register = 0;

    for (int i = 0; i < 16; ++i) stack[i] = 0;
    stack_pointer = 0;

    for (int i = 0; i < SCREEN_HEIGHT * SCREEN_WIDTH; ++i) display[0];
    for (int i = 0; i < 16; ++i) keys[0];

    //Loading fonts
    const uint32_t fonts[16] = { 0xF999F, 0x72262, 0xF8F1F, 0xF1F1F, 0x11F99, 0xF1F8F, 0xF9F8F, 0x4421F, 0xF9F9F, 0xF1F9F, 0x99F9F, 0xE9E9E, 0xF888F, 0xE999E, 0xF8F8F, 0x88F8F };
    for (int i = 0; i < 80; ++i)
    {
        Memory[i] = ((fonts[i / 5] >> (4 * (i % 5))) & 0xF) << 4;
        //To print font in binary
        for (int j = 7; j > -1; --j) printf("%u", (Memory[i] >> j) & 0x1);
        printf("\n");
        if ((i+1) % 5 == 0) printf("\n");
    }
}

int readRom(const char* filename)
{
    FILE* rom = fopen(filename, "rb");
    if (rom == NULL)
    {
        printf("ROM could not be opened\n");
        exit(1);
    }

    fseek(rom, 0, SEEK_END);
    unsigned int size = ftell(rom);
    rewind(rom);

    uint8_t* buffer = (uint8_t*)malloc(size * sizeof(uint8_t));

    if (buffer == NULL)
    {
        printf("Could not allocate memory for buffer!\n");
        exit(1);
    }

    fread(buffer, sizeof(uint8_t), size, rom);

    for (int i = 0; i < size; i++)
    {
        printf("%x ", buffer[i]);
    }


    if (size <= ENDING_ADDRESS - STARTING_ADDRESS)
    {
        for (int i = 0; i < size; ++i)
            Memory[STARTING_ADDRESS + i] = buffer[i];
    }
    else
    {
        printf("ERROR: ROM is too big!\n");
        exit(1);
    }


    fclose(rom);
    free(buffer);

    return 0;
}

void chip_cycle()
{

    //FETCH and DECODE
    opcode = (Memory[program_counter] << 8) | Memory[program_counter + 1];
    program_counter += 2;

    //EXECUTE
    switch ((opcode & 0xF000) >> 12)
    {
    case 0x0:
        switch (opcode & 0x00FF)
        {
        case 0xE0:  for(int i = 0; i < SCREEN_HEIGHT * SCREEN_WIDTH; ++i)display[i] = 0; break;
        case 0xEE:  program_counter = stack[stack_pointer--]; break;
        default:    printf("Invalid OPCODE at Branch 0x0 : %u", opcode); break;
        }break;
    case 0x2:   stack[++stack_pointer] = program_counter;
    case 0x1:   program_counter = _NNN;             break;
    case 0x3:   if(VX == _NN)   program_counter+=2; break;
    case 0x4:   if(VX != _NN)   program_counter+=2; break;
    case 0x5:   if(VX == VY)    program_counter+=2; break;
    case 0x6:   VX = _NN;                           break;
    case 0x7:   VX += _NN;                          break;
    case 0x8:
        switch (opcode & 0x000F)
        {
        case 0x0:   VX = VY;  break;
        case 0x1:   VX |= VY; break;
        case 0x2:   VX &= VY; break;
        case 0x3:   VX ^= VY; break;
        case 0x4: {
            VF = 0;
            if (VX + VY > 255) VF = 1;
            VX = (VX + VF) & 0xFF;
        }  break;
        case 0x5: {
            VF = 0;
            if (VX > VY) VF = 1;
            VX -= VF;
        }  break;
        case 0x6: {
            VF = VX & 0x1;
            VX >>= 1;
        } break;
        case 0x7: {
            VF = 0;
            if (VY > VX) VF = 1;
            VX = VY - VX;
        } break;
        case 0xE: {
            VF = (VX & 0x80) >> 7;
            VX <<= 1;
        } break;
        default:  printf("Invalid OPCODE at Branch 0x8 : %u", opcode); break;
        }break;
    case 0x9:if(VX != VY)program_counter+=2; break;
    case 0xA: index_register = _NNN; break;
    case 0xB: program_counter = registers[0] + _NNN; break;
    case 0xC: VX = (rand()%256) & _NN; break;
    case 0xD: {

        unsigned int xpos = VX % 64;
        unsigned int ypos = VY % 32;

        //VF is used to check for collision
        VF = 0;
        //Loops till sprite height N
        for (int i = 0; i < _N; ++i)
            for (int j = 0; j < 8; ++j)
            {
                uint32_t* screenpixel = &display[((ypos + i) * SCREEN_WIDTH) + (xpos + j)];
                if (Memory[index_register + i] & (0x80 >> j))
                {
                    if (*screenpixel == 0xFFFFFFFF) VF = 1;
                    *screenpixel ^= 0xFFFFFFFF;
                }
            }

    }break;
    case 0xE:
        switch (opcode & 0x00FF)
        {
        case 0x9E:if(keys[VX] == 1)program_counter+=2; break;
        case 0xA1:if(keys[VX] == 0)program_counter+=2; break;
        default: printf("Invalid OPCODE at Branch 0xE : %u", opcode); break;
        }
    case 0xF:
        switch (opcode & 0x00FF)
        {
        case 0x07:  VX = delay_timer;           break;
        case 0x0A: {
            program_counter -= 2;
            for (int i = 0; i < 16; ++i)
                if (keys[i])
                {
                    VX = 1;
                    program_counter += 2;
                    break;
                }
        }                    break;
        case 0x15:  delay_timer = VX;           break;
        case 0x18:  sound_timer = VX;           break;
        case 0x1E:  index_register += VX;       break;
        case 0x29:  index_register = VX * 5;    break;  //Select VX font
        case 0x33: {
            uint8_t temp = VX;
            Memory[index_register + 2] = temp % 10;
            temp /= 10;
            Memory[index_register + 1] = temp % 10;
            temp /= 10;
            Memory[index_register] = temp % 10;
        } break;
        case 0x55:  for (int i = 0; i <= VX; ++i) Memory[index_register + i] = registers[i]; break;
        case 0x65:  for (int i = 0; i <= VX; ++i) registers[i] = Memory[index_register + i]; break;
        default:    printf("Invalid OPCODE at Branch 0xF : %u", opcode); break;
        }break;
    default:  printf("Invalid OPCODE at Branch Main : %u", opcode); break;
    }

    //Change timers
    if (delay_timer > 0) --delay_timer; 
    if (sound_timer > 0) --sound_timer; //Play sound when soundtimer > 0

}


#undef _X
#undef _Y 
#undef _N 
#undef _NN 
#undef _NNN 
#undef VX 
#undef VY 
#undef STARTING_ADDRESS


/*  --EMULATOR PLATFORM LAYER--
A platform layer
*/

SDL_Window* window;
SDL_Renderer* renderer;
SDL_Texture* texture;

//Initialises all SDL componenets
void initializeSDL(const char* title, int scale)
{
    if (SDL_Init(SDL_INIT_EVERYTHING))
    {
        printf("Could not Initialize SDL!");
        exit(1);
    }
    window = SDL_CreateWindow("Chip-8", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, (SCREEN_WIDTH * scale), (SCREEN_HEIGHT * scale), SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);
}

//Taking input
int poll_events()
{
    static SDL_Event e;

    while (SDL_PollEvent(&e))
    {
        switch (e.type)
        {
        case SDL_QUIT: return 0; break;
        case SDL_KEYDOWN:
            switch (e.key.keysym.sym)
            {
            case SDLK_1:keys[0] = 1; break;
            case SDLK_2:keys[1] = 1;  break;
            case SDLK_3:keys[2] = 1; break;
            case SDLK_4:keys[3] = 1; break;
            case SDLK_q:keys[4] = 1; break;
            case SDLK_w:keys[5] = 1; break;
            case SDLK_e:keys[6] = 1; break;
            case SDLK_r:keys[7] = 1; break;
            case SDLK_a:keys[8] = 1; break;
            case SDLK_s:keys[9] = 1; break;
            case SDLK_d:keys[10] = 1; break;
            case SDLK_f:keys[11] = 1; break;
            case SDLK_z:keys[12] = 1; break;
            case SDLK_x:keys[13] = 1; break;
            case SDLK_c:keys[14] = 1; break;
            case SDLK_v:keys[15] = 1; break;
            default:break;
            }break;
        case SDL_KEYUP:
            switch (e.key.keysym.sym)
            {
            case SDLK_1:keys[0] = 0; break;
            case SDLK_2:keys[1] = 0;  break;
            case SDLK_3:keys[2] = 0; break;
            case SDLK_4:keys[3] = 0; break;
            case SDLK_q:keys[4] = 0; break;
            case SDLK_w:keys[5] = 0; break;
            case SDLK_e:keys[6] = 0; break;
            case SDLK_r:keys[7] = 0; break;
            case SDLK_a:keys[8] = 0; break;
            case SDLK_s:keys[9] = 0; break;
            case SDLK_d:keys[10] = 0; break;
            case SDLK_f:keys[11] = 0; break;
            case SDLK_z:keys[12] = 0; break;
            case SDLK_x:keys[13] = 0; break;
            case SDLK_c:keys[14] = 0; break;
            case SDLK_v:keys[15] = 0; break;
            default:break;
            }break;
        default:break;
        }
    }
    return 1;
}

//Called every time the screen needs to be updated
void update_screen()
{
    SDL_UpdateTexture(texture, NULL, (const void*)display, sizeof(display[0])*SCREEN_WIDTH);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

//Destroys all SDL entities and quits SDL
void cleanSDL()
{
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}


int main(int argc, char* argv[])
{
    //Initializing CHIP
    initializeChip();

    //Taking rom input
    char path[30];
    printf("Enter the path:");
    fgets(path, 30, stdin);
    path[strlen(path) - 1] = '\0';
    readRom(path);

    initializeSDL("Chip8", 10);

    time_t time1 = time(0);

    //Main Loop
    int loop = 1;
    while (loop)
    {
        time_t time2 = time(0) - time1;
        if (time2 > FRAME_RATE)
        {
            time1 = time2;
            loop = poll_events();
            chip_cycle();
            update_screen();
        }
        
    }

    cleanSDL();
    return 0;
}