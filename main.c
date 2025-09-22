//includes
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

//variáveis e structs
typedef struct {
  SDL_Window*   window;
  SDL_Renderer* renderer;
} AppContext;

typedef struct {
  SDL_Surface* surface_rgba;   //surface de trabalho em RGBA32 
  SDL_Texture* texture;        //textura para renderizar 
  int w, h;
} ImageData;

//declaração de função
static void log_sdl_error(const char* msg);
static bool sdl_bootstrap(AppContext* app);
static bool start_sdl_window(AppContext* app, const char* title, int w, int h, Uint32 window_flags);
static void cleanup_resources(AppContext* app, ImageData* img);
static bool img_load_rgba32(const char* path, ImageData* out);
static void render_loop(AppContext* app, ImageData* img);
static bool is_surface_grayscale_rgba32(const SDL_Surface* surf);
static bool convert_to_grayscale_inplace(SDL_Surface* surf); 
void shutdown(void);

//funções
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

// cria janela + renderer 
static bool start_sdl_window(AppContext* app, const char* title, int w, int h, Uint32 window_flags) {
  app->window = SDL_CreateWindow(title ? title : "Visualizador", w, h, window_flags);
  if (!app->window) {
    log_sdl_error("SDL_CreateWindow falhou");
    return false;
  }

  app->renderer = SDL_CreateRenderer(app->window, NULL);
  if (!app->renderer) {
    log_sdl_error("SDL_CreateRenderer falhou");
    SDL_DestroyWindow(app->window);
    app->window = NULL;
    return false;
  }
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

//carrega a imagem do disco e converte para RGBA32
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

// Loop de renderização simples usando a janela criada antes do load 
static void render_loop(AppContext* app, ImageData* img) {
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

    //manter a proporção da imagem para não distorcer
    int ww, wh;
    SDL_GetWindowSize(app->window, &ww, &wh);

    float iW = (float)img->w, iH = (float)img->h;
    float wW = (float)ww,     wH = (float)wh;

    float scale = (wW / iW < wH / iH) ? (wW / iW) : (wH / iH);
    float dw = iW * scale;
    float dh = iH * scale;

    SDL_FRect dst = { (wW - dw) * 0.5f, (wH - dh) * 0.5f, dw, dh };
    SDL_RenderTexture(app->renderer, img->texture, NULL, &dst);

    SDL_RenderPresent(app->renderer);
    SDL_Delay(16);
  }
}

// Verifica se TODOS os pixels têm R==G==B. Pressupõe surf->format == RGBA32
static bool is_surface_grayscale_rgba32(const SDL_Surface* surf) {
  if (!surf) return false;

  if (!SDL_LockSurface((SDL_Surface*)surf)) { 
    SDL_Log("*** Falha ao SDL_LockSurface (is_surface_grayscale): %s", SDL_GetError());
    return false;
  }

  const Uint32* pixels = (const Uint32*)surf->pixels;
  const int w = surf->w, h = surf->h;
  const int count = w * h;
  const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(surf->format);

  bool is_gray = true;
  for (int i = 0; i < count; i++) {
    Uint8 r, g, b, a;
    SDL_GetRGBA(pixels[i], fmt, NULL, &r, &g, &b, &a);
    if (!(r == g && g == b)) { is_gray = false; break; }
  }

  SDL_UnlockSurface((SDL_Surface*)surf);
  return is_gray;
}

static bool convert_to_grayscale_inplace(SDL_Surface* surf) {
  if (!surf) return false;
  if (!SDL_LockSurface(surf)) {
    SDL_Log("*** Falha ao SDL_LockSurface (convert_to_grayscale): %s", SDL_GetError());
    return false;
  }

  Uint32* pixels = (Uint32*)surf->pixels;
  const int count = surf->w * surf->h;
  const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(surf->format);

  for (int i = 0; i < count; i++) {
    Uint8 r, g, b, a;
    SDL_GetRGBA(pixels[i], fmt, NULL, &r, &g, &b, &a);

    // Y = 0.2125R + 0.7154G + 0.0721B 
    float Yf = 0.2125f * (float)r + 0.7154f * (float)g + 0.0721f * (float)b;
    Uint8 Y = (Uint8)(Yf + 0.5f);

    pixels[i] = SDL_MapRGBA(fmt, NULL, Y, Y, Y, a);
  }

  SDL_UnlockSurface(surf);
  return true;
}

int main(int argc, char** argv) {
  atexit(shutdown);  // SDL_Quit() no fim 

  if (argc != 2) {
    SDL_Log("Uso: %s <caminho_imagem>\nEx.: %s images/foto.png", argv[0], argv[0]);
    return 1;
  }

  const char* path = argv[1];

  AppContext app = {0};
  if (!sdl_bootstrap(&app)) return 1;

  // criação da janela antes de carregar a imagem 
  if (!start_sdl_window(&app, "Pré-visualização (carregamento)", 800, 600, SDL_WINDOW_RESIZABLE)) {
    cleanup_resources(&app, NULL);
    return 1;
  }

  // 2) carregamento da imagem 
  ImageData img = {0};
  if (!img_load_rgba32(path, &img)) {
    cleanup_resources(&app, &img);
    return 1;
  }

  if (!is_surface_grayscale_rgba32(img.surface_rgba)) {
    SDL_Log("Imagem colorida detectada -> convertendo para escala de cinza...");
    if (!convert_to_grayscale_inplace(img.surface_rgba)) {
      cleanup_resources(&app, &img);
      return 1;
    }
  } else {
    SDL_Log("Imagem já está em escala de cinza.");
  }

  // 3) cria a textura agora que já há renderer 
  img.texture = SDL_CreateTextureFromSurface(app.renderer, img.surface_rgba);
  if (!img.texture) {
    log_sdl_error("SDL_CreateTextureFromSurface falhou");
    cleanup_resources(&app, &img);
    return 1;
  }

  //loop de exibição 
  render_loop(&app, &img);

  cleanup_resources(&app, &img);
  return 0; //atexit(shutdown) chamará SDL_Quit() 
}
