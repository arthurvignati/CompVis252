#include <stdio.h>
#include <stdbool.h>
#include<stdlib.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>


typedef struct {
  SDL_Window*   window;
  SDL_Renderer* renderer;
} AppContext;

typedef struct {
  SDL_Surface* surface_rgba;   
  SDL_Texture* texture;        
  int w, h;
} ImageData;


static void log_sdl_error(const char* msg);
static bool sdl_bootstrap(AppContext* app);
static void cleanup_resources(AppContext* app, ImageData* img);

static bool img_load_rgba32(const char* path, ImageData* out);

static bool preview_open(AppContext* app, ImageData* img);
static void preview_loop(AppContext* app, ImageData* img);


void shutdown(void);

static void log_sdl_error(const char* msg) {
  SDL_Log("*** %s: %s", msg, SDL_GetError());
}

static bool sdl_bootstrap(AppContext* app) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    log_sdl_error("Falha ao iniciar SDL");
    return false;
  }



  app->window = NULL;
  app->renderer = NULL;
  return true;
}

static void cleanup_resources(AppContext* app, ImageData* img) {
  if (img) {
    if (img->texture)      SDL_DestroyTexture(img->texture);
    if (img->surface_rgba) SDL_DestroySurface(img->surface_rgba);
    img->texture = NULL;
    img->surface_rgba = NULL;
    img->w = img->h = 0;
  }
  if (app) {
    if (app->renderer) SDL_DestroyRenderer(app->renderer);
    if (app->window)   SDL_DestroyWindow(app->window);
    app->renderer = NULL;
    app->window = NULL;
  }
}

void shutdown(void) {
  SDL_Log("shutdown()");
  SDL_Quit();
}

static bool img_load_rgba32(const char* path, ImageData* out) {
  SDL_Log("Carregando: %s", path);

  SDL_Surface* loaded = IMG_Load(path);
  if (!loaded) {
    log_sdl_error("IMG_Load falhou");
    return false;
  }

  SDL_Surface* rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
  SDL_DestroySurface(loaded);
  if (!rgba) {
    log_sdl_error("SDL_ConvertSurface para RGBA32 falhou");
    return false;
  }

  out->surface_rgba = rgba;
  out->texture = NULL;
  out->w = rgba->w;
  out->h = rgba->h;

  const char* fmt_name = SDL_GetPixelFormatName(rgba->format);
  SDL_Log("Imagem OK: %dx%d | pitch=%d bytes | formato=%s",
          rgba->w, rgba->h, rgba->pitch, fmt_name ? fmt_name : "(desconhecido)");
  return true;
}

static bool preview_open(AppContext* app, ImageData* img) {
  app->window = SDL_CreateWindow("Pré-visualização (carregamento)", img->w, img->h, 0);
  if (!app->window) { log_sdl_error("SDL_CreateWindow falhou"); return false; }

  app->renderer = SDL_CreateRenderer(app->window, NULL);
  if (!app->renderer) { log_sdl_error("SDL_CreateRenderer falhou"); return false; }

  img->texture = SDL_CreateTextureFromSurface(app->renderer, img->surface_rgba);
  if (!img->texture) { log_sdl_error("SDL_CreateTextureFromSurface falhou"); return false; }

  return true;
}

static void preview_loop(AppContext* app, ImageData* img) {
  bool running = true;
  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_EVENT_QUIT || e.type == SDL_EVENT_KEY_DOWN) {
        running = false;
      }
    }

    SDL_SetRenderDrawColor(app->renderer, 20, 20, 20, 255);
    SDL_RenderClear(app->renderer);

    SDL_FRect dst = {0, 0, (float)img->w, (float)img->h};
    SDL_RenderTexture(app->renderer, img->texture, NULL, &dst);
    SDL_RenderPresent(app->renderer);

    SDL_Delay(16);
  }
}


int main(int argc, char** argv) {
  atexit(shutdown); 

  if (argc != 2) {
    SDL_Log("Uso: %s <caminho_imagem>\nEx.: %s images/foto.png", argv[0], argv[0]);
    return 1;
  }

  const char* path = argv[1];

  AppContext app = {0};
  if (!sdl_bootstrap(&app)) return 1;

  ImageData img = {0};
  if (!img_load_rgba32(path, &img)) {
    cleanup_resources(&app, &img);
    return 1;
  }


  if (preview_open(&app, &img)) {
    preview_loop(&app, &img);
  }

  cleanup_resources(&app, &img);
  return 0;
}
