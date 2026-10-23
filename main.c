#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <SDL2/SDL.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define CONFIG_FILE "config.lua"
#define MAX_DELTA_TIME 0.05f /* evita "saltos" se a janela travar ou for arrastada */

typedef struct {
    SDL_Rect rect;   /* usado para desenhar */
    SDL_Color color;
    float x, y;      /* posição real, em float (o rect só recebe o valor arredondado) */
    float speed;
    float dx;
    float dy;
} Player;

SDL_Window *gWindow = NULL;
SDL_Renderer *gRenderer = NULL;
lua_State *gLua = NULL; /* antes se chamava L e era "sombreado" pelos parâmetros L das funções */
Player gPlayer;
bool gGameIsRunning = true;

/* Garante que o valor cabe em um Uint8 (0 a 255) em vez de "dar a volta" */
static Uint8 clampToUint8(lua_Number value){
    if (value < 0) return 0;
    if (value > 255) return 255;
    return (Uint8)value;
}

/* Lê um campo numérico da tabela que está no topo da pilha */
int get_lua_int_field(lua_State *L, const char *key, int default_value){
    int result = default_value;
    lua_getfield(L, -1, key);
    if (lua_isnumber(L, -1)){
        result = (int)lua_tonumber(L, -1);
    } else {
        fprintf(stderr, "Warning: Lua config: '%s' not found or not a number. Using default %d.\n", key, default_value);
    }
    lua_pop(L, 1);
    return result;
}

float get_lua_float_field(lua_State *L, const char *key, float default_value){
    float result = default_value;
    lua_getfield(L, -1, key);
    if (lua_isnumber(L, -1)){
        result = (float)lua_tonumber(L, -1);
    } else {
        fprintf(stderr, "Warning: Lua config: '%s' not found or not a number. Using default %.2f.\n", key, default_value);
    }
    lua_pop(L, 1);
    return result;
}

/* Lê um canal (r, g, b ou a) da tabela de cor que está no topo da pilha */
static void get_color_channel(lua_State *L, const char *name, Uint8 *channel){
    lua_getfield(L, -1, name);
    if (lua_isnumber(L, -1)){
        *channel = clampToUint8(lua_tonumber(L, -1));
    } else {
        fprintf(stderr, "Warning: Lua config: color field '%s' missing or not a number.\n", name);
    }
    lua_pop(L, 1);
}

void get_lua_color_field(lua_State *L, const char *key, SDL_Color *color){
    lua_getfield(L, -1, key);
    if (lua_istable(L, -1)){
        get_color_channel(L, "r", &color->r);
        get_color_channel(L, "g", &color->g);
        get_color_channel(L, "b", &color->b);
        get_color_channel(L, "a", &color->a);
    } else {
        fprintf(stderr, "Warning: Lua config: '%s' not found or not a table. Using default color.\n", key);
    }
    lua_pop(L, 1);
}

static void setDefaultPlayer(void){
    gPlayer.rect = (SDL_Rect){50, 50, 100, 100};
    gPlayer.color = (SDL_Color){255, 0, 0, 255};
    gPlayer.speed = 300.0f;
}

/* Mantém o jogador dentro da janela */
static void clampPlayerToScreen(void){
    if (gPlayer.x + gPlayer.rect.w > SCREEN_WIDTH)  gPlayer.x = (float)(SCREEN_WIDTH - gPlayer.rect.w);
    if (gPlayer.y + gPlayer.rect.h > SCREEN_HEIGHT) gPlayer.y = (float)(SCREEN_HEIGHT - gPlayer.rect.h);
    if (gPlayer.x < 0) gPlayer.x = 0;
    if (gPlayer.y < 0) gPlayer.y = 0;
}

bool initialize(void){

    if (SDL_Init(SDL_INIT_VIDEO) < 0){
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return false;
    }

    gWindow = SDL_CreateWindow("SDL Lua Demo",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH,
        SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (gWindow == NULL){
        fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return false;
    }

    gRenderer = SDL_CreateRenderer(gWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (gRenderer == NULL){
        fprintf(stderr, "Accelerated renderer failed (%s). Trying software renderer.\n", SDL_GetError());
        gRenderer = SDL_CreateRenderer(gWindow, -1, SDL_RENDERER_SOFTWARE);
    }
    if (gRenderer == NULL){
        fprintf(stderr, "Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return false;
    }

    gLua = luaL_newstate();
    if (gLua == NULL){
        fprintf(stderr, "Failed to create Lua state.\n");
        return false;
    }
    luaL_openlibs(gLua);

    printf("Loading config from %s...\n", CONFIG_FILE);
    if (luaL_dofile(gLua, CONFIG_FILE) != LUA_OK){
        fprintf(stderr, "Error loading Lua script %s: %s\n", CONFIG_FILE, lua_tostring(gLua, -1));
        lua_pop(gLua, 1); /* remove a mensagem de erro da pilha */
        setDefaultPlayer();
    }
    else {
        printf("Lua script executed successfully.\n");
        lua_getglobal(gLua, "player");
        if (lua_istable(gLua, -1)){
            printf("Found 'player' table in Lua.\n");

            gPlayer.rect.x = get_lua_int_field(gLua, "x", 50);
            gPlayer.rect.y = get_lua_int_field(gLua, "y", 50);
            gPlayer.rect.w = get_lua_int_field(gLua, "width", 100);
            gPlayer.rect.h = get_lua_int_field(gLua, "height", 100);
            gPlayer.speed = get_lua_float_field(gLua, "speed", 300.0f);

            gPlayer.color = (SDL_Color){255, 0, 0, 255};
            get_lua_color_field(gLua, "color", &gPlayer.color);

            /* valida valores absurdos vindos do config.lua */
            if (gPlayer.rect.w <= 0 || gPlayer.rect.w > SCREEN_WIDTH)  gPlayer.rect.w = 100;
            if (gPlayer.rect.h <= 0 || gPlayer.rect.h > SCREEN_HEIGHT) gPlayer.rect.h = 100;

            printf("Loaded player config: pos=(%d, %d), size=(%d, %d), speed=%.1f, color=(%d, %d, %d, %d)\n",
                gPlayer.rect.x, gPlayer.rect.y, gPlayer.rect.w, gPlayer.rect.h,
                gPlayer.speed, gPlayer.color.r, gPlayer.color.g, gPlayer.color.b, gPlayer.color.a);
        } else {
            fprintf(stderr, "Warning: Lua global 'player' not found or not a table. Using default values.\n");
            setDefaultPlayer();
        }
        lua_pop(gLua, 1);
    }

    gPlayer.x = (float)gPlayer.rect.x;
    gPlayer.y = (float)gPlayer.rect.y;
    clampPlayerToScreen();
    gPlayer.rect.x = (int)gPlayer.x;
    gPlayer.rect.y = (int)gPlayer.y;
    gPlayer.dx = 0.0f;
    gPlayer.dy = 0.0f;
    return true;
}

void cleanup(void){
    printf("Cleaning up...\n");
    if (gLua){
        lua_close(gLua);
        gLua = NULL;
        printf("Lua state closed.\n");
    }
    if (gRenderer){
        SDL_DestroyRenderer(gRenderer);
        gRenderer = NULL;
    }
    if (gWindow){
        SDL_DestroyWindow(gWindow);
        gWindow = NULL;
    }
    SDL_Quit();
    printf("SDL quit.\n");
}

void handleInput(void){
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0){
        if (e.type == SDL_QUIT){
            gGameIsRunning = false;
        }
    }

    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    gPlayer.dx = 0;
    gPlayer.dy = 0;

    /* com += e -=, teclas opostas apertadas juntas se cancelam */
    if (keys[SDL_SCANCODE_UP]    || keys[SDL_SCANCODE_W]) gPlayer.dy -= 1.0f;
    if (keys[SDL_SCANCODE_DOWN]  || keys[SDL_SCANCODE_S]) gPlayer.dy += 1.0f;
    if (keys[SDL_SCANCODE_LEFT]  || keys[SDL_SCANCODE_A]) gPlayer.dx -= 1.0f;
    if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) gPlayer.dx += 1.0f;

    /* normaliza para a diagonal não ser mais rápida (1.41x) que andar reto */
    if (gPlayer.dx != 0.0f && gPlayer.dy != 0.0f){
        const float inv = 1.0f / sqrtf(2.0f);
        gPlayer.dx *= inv;
        gPlayer.dy *= inv;
    }
}

void update(float deltaTime){
    /* a posição fica em float: com (int) direto, em FPS alto o movimento por quadro
       era arredondado para 0 e o jogador andava mais devagar ou parava */
    gPlayer.x += gPlayer.dx * gPlayer.speed * deltaTime;
    gPlayer.y += gPlayer.dy * gPlayer.speed * deltaTime;

    clampPlayerToScreen();

    gPlayer.rect.x = (int)roundf(gPlayer.x);
    gPlayer.rect.y = (int)roundf(gPlayer.y);
}

void render(void){
    SDL_SetRenderDrawColor(gRenderer, 0x33, 0x33, 0x33, 0xFF);
    SDL_RenderClear(gRenderer);

    SDL_SetRenderDrawColor(gRenderer, gPlayer.color.r, gPlayer.color.g, gPlayer.color.b, gPlayer.color.a);
    SDL_RenderFillRect(gRenderer, &gPlayer.rect);

    SDL_RenderPresent(gRenderer);
}

int main(int argc, char const *argv[]) {
    (void)argc;
    (void)argv;

    if (!initialize()) {
        printf("Failed to initialize!\n");
        cleanup();
        return 1;
    }

    Uint32 lastTime = SDL_GetTicks();

    while (gGameIsRunning) {
        Uint32 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;
        if (deltaTime > MAX_DELTA_TIME) deltaTime = MAX_DELTA_TIME;

        handleInput();
        update(deltaTime);
        render();
    }

    cleanup();
    return 0;
}
