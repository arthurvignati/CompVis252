//includes
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <string.h>   
#include <math.h> 

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

typedef enum { BTN_IDLE, BTN_HOVER, BTN_ACTIVE } ButtonState;

typedef struct {
  SDL_FRect rect;
  ButtonState state;  // pode manter pra hover visual (opcional)
} UIButton;

typedef struct {
  AppContext mainApp;
  AppContext sideApp;
  UIButton   eqButton;
  Uint32     hist[256];
} UIContext;

//constantes
#define SIDE_W  420
#define SIDE_H  340
#define SIDE_MARGIN 16
#define BUTTON_H 36
#define BUTTON_W 160

//declaração de função
static void  log_sdl_error(const char* msg);
static bool  img_load_rgba32(const char* path, ImageData* out);
static bool  is_surface_grayscale_rgba32(const SDL_Surface* surf);
static bool  convert_to_grayscale_inplace(SDL_Surface* surf);
static void  compute_histogram_gray_rgba32(const SDL_Surface* surf, Uint32 hist[256], float* out_mean, float* out_stddev);
static void  draw_histogram(SDL_Renderer* rr, const Uint32 hist[256], SDL_FRect area);
static void  draw_button(SDL_Renderer* rr, const UIButton* btn);
static bool  create_main_window(UIContext* ui, int imgw, int imgh);
static bool  create_side_window(UIContext* ui);
static void  cleanup_all(UIContext* ui, ImageData* img);
static void  render_main_window(UIContext* ui, ImageData* img);
static void  render_side_window(UIContext* ui);
static void  handle_events(UIContext* ui);
static void  render_loop(UIContext* ui, ImageData* img);
void shutdown(void);

//funções
static void log_sdl_error(const char* msg) {
  SDL_Log("*** %s: %s", msg, SDL_GetError());
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

static void render_main_window(UIContext* ui, ImageData* img) {
  SDL_SetRenderDrawColor(ui->mainApp.renderer, 20,20,20,255);
  SDL_RenderClear(ui->mainApp.renderer);

  int ww, wh;
  SDL_GetWindowSize(ui->mainApp.window, &ww, &wh);

  float iW = (float)img->w, iH = (float)img->h;
  float wW = (float)ww,     wH = (float)wh;
  float scale = (wW / iW < wH / iH) ? (wW / iW) : (wH / iH);
  float dw = iW * scale;
  float dh = iH * scale;

  SDL_FRect dst = { (wW - dw) * 0.5f, (wH - dh) * 0.5f, dw, dh };
  SDL_RenderTexture(ui->mainApp.renderer, img->texture, NULL, &dst);

  SDL_RenderPresent(ui->mainApp.renderer);
}

//verifica se todos os pixels têm R==G==B. Pressupõe surf->format == RGBA32
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


static void compute_histogram_gray_rgba32(const SDL_Surface* surf, Uint32 hist[256],
                                          float* out_mean, float* out_stddev) {
  memset(hist, 0, sizeof(Uint32) * 256);
  const int count = surf->w * surf->h;

  if (!SDL_LockSurface((SDL_Surface*)surf)) {
    SDL_Log("*** Lock falhou (hist): %s", SDL_GetError());
    return;
  }
  const Uint32* p = (const Uint32*)surf->pixels;
  const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(surf->format);

  double sum = 0.0, sum2 = 0.0;
  for (int i = 0; i < count; i++) {
    Uint8 r,g,b,a;
    SDL_GetRGBA(p[i], fmt, NULL, &r,&g,&b,&a);
    // imagem está em cinza -> r==g==b (r como intensidade)
    Uint8 Y = r;
    hist[Y]++;
    sum  += Y;
    sum2 += (double)Y * (double)Y;
  }
  SDL_UnlockSurface((SDL_Surface*)surf);

  double mean = sum / (double)count;
  double var  = (sum2 / (double)count) - (mean*mean);
  if (var < 0.0) var = 0.0;
  double stddev = sqrt(var);
  if (out_mean)   *out_mean = (float)mean;
  if (out_stddev) *out_stddev = (float)stddev;
}

static void draw_histogram(SDL_Renderer* rr, const Uint32 hist[256],
                           SDL_FRect area) {
  //fundo
  SDL_SetRenderDrawColor(rr, 30,30,30,255);
  SDL_FRect bg = area;
  SDL_RenderFillRect(rr, &bg);

  //encontra max
  Uint32 maxv = 1;
  for (int i = 0; i < 256; i++) if (hist[i] > maxv) maxv = hist[i];

  //barras
  float barw = area.w / 256.0f;
  for (int i = 0; i < 256; i++) {
    float h = (hist[i] / (float)maxv) * (area.h - 2.0f);
    SDL_FRect bar = { area.x + i * barw, area.y + (area.h - h), barw, h };
    SDL_SetRenderDrawColor(rr, 200,200,200,255);
    SDL_RenderFillRect(rr, &bar);
  }

  //moldura
  SDL_SetRenderDrawColor(rr, 80,80,80,255);
  SDL_RenderRect(rr, &area);
}

static void draw_button(SDL_Renderer* rr, const UIButton* btn) {
  SDL_SetRenderDrawColor(rr, 0,102,204,255); // azul fixo
  SDL_RenderFillRect(rr, &btn->rect);
  SDL_SetRenderDrawColor(rr, 20,20,20,255);
  SDL_RenderRect(rr, &btn->rect);
}



//window principal: adapta ao tamanho da imagem e centraliza
static bool create_main_window(UIContext* ui, int imgw, int imgh) {
  //limite um pouco o tamanho inicial pra não extrapolar telas muito pequenas
  int w = imgw, h = imgh;
  if (w > 1400) { float s = 1400.0f / (float)w; w = 1400; h = (int)(imgh * s); }
  if (h > 900)  { float s =  900.0f / (float)h; h = 900;  w = (int)(w * s); }

  ui->mainApp.window = SDL_CreateWindow("Imagem (principal)", w, h, SDL_WINDOW_RESIZABLE);
  if (!ui->mainApp.window) { log_sdl_error("Create main window"); return false; }
  SDL_SetWindowPosition(ui->mainApp.window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

  ui->mainApp.renderer = SDL_CreateRenderer(ui->mainApp.window, NULL);
  if (!ui->mainApp.renderer) { log_sdl_error("Create main renderer"); return false; }
  return true;
}

static bool create_side_window(UIContext* ui) {
  ui->sideApp.window = SDL_CreateWindow("Histograma (secundária)", SIDE_W, SIDE_H, 0);
  if (!ui->sideApp.window) { log_sdl_error("Create side window"); return false; }

  ui->sideApp.renderer = SDL_CreateRenderer(ui->sideApp.window, NULL);
  if (!ui->sideApp.renderer) { log_sdl_error("Create side renderer"); return false; }

  //posiciona ao lado direito da principal
  int x,y,w,h;
  SDL_GetWindowPosition(ui->mainApp.window, &x,&y);
  SDL_GetWindowSize(ui->mainApp.window, &w,&h);
  SDL_SetWindowPosition(ui->sideApp.window, x + w + SIDE_MARGIN, y);

  //botão embaixo do histograma
  ui->eqButton.rect.x = SIDE_MARGIN;
  ui->eqButton.rect.w = BUTTON_W;
  ui->eqButton.rect.h = BUTTON_H;
  ui->eqButton.rect.y = SIDE_H - SIDE_MARGIN - ui->eqButton.rect.h;
  ui->eqButton.state  = BTN_IDLE;
  return true;
}



static void cleanup_all(UIContext* ui, ImageData* img) {
  if (img) {
    if (img->texture)      SDL_DestroyTexture(img->texture);
    if (img->surface_rgba) SDL_DestroySurface(img->surface_rgba);
  }
  if (ui) {
    if (ui->mainApp.renderer) SDL_DestroyRenderer(ui->mainApp.renderer);
    if (ui->mainApp.window)   SDL_DestroyWindow(ui->mainApp.window);
    if (ui->sideApp.renderer) SDL_DestroyRenderer(ui->sideApp.renderer);
    if (ui->sideApp.window)   SDL_DestroyWindow(ui->sideApp.window);
  }
}



static void render_side_window(UIContext* ui) {
  SDL_SetRenderDrawColor(ui->sideApp.renderer, 15,15,15,255);
  SDL_RenderClear(ui->sideApp.renderer);

  // área do histograma
  SDL_FRect histArea = { SIDE_MARGIN, SIDE_MARGIN, SIDE_W - SIDE_MARGIN*2, SIDE_H - SIDE_MARGIN*3 - BUTTON_H };
  draw_histogram(ui->sideApp.renderer, ui->hist, histArea);

  // botão
  draw_button(ui->sideApp.renderer, &ui->eqButton);

  SDL_RenderPresent(ui->sideApp.renderer);
}


static void handle_events(UIContext* ui) {
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_EVENT_QUIT) {
      exit(0);
    }
    if (e.type == SDL_EVENT_WINDOW_RESIZED || e.type == SDL_EVENT_WINDOW_MOVED) {
      int x,y,w,h;
      SDL_GetWindowPosition(ui->mainApp.window, &x,&y);
      SDL_GetWindowSize(ui->mainApp.window, &w,&h);
      SDL_SetWindowPosition(ui->sideApp.window, x + w + SIDE_MARGIN, y);
    }
    if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) {
      exit(0);
    }
  }

}
  static void render_loop(UIContext* ui, ImageData* img) {
    compute_histogram_gray_rgba32(img->surface_rgba, ui->hist, NULL, NULL);
    for (;;) {
      handle_events(ui);
      render_main_window(ui, img);
      render_side_window(ui);
      SDL_Delay(16);
    }
  }

int main(int argc, char** argv) {
  atexit(shutdown);

  if (argc != 2) {
    SDL_Log("Uso: %s <caminho_imagem>", argv[0]);
    return 1;
  }

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    log_sdl_error("SDL_Init"); return 1;
  }

  ImageData img = {0};
  if (!img_load_rgba32(argv[1], &img)) {
    cleanup_all(NULL, &img); return 1;
  }

  if (!is_surface_grayscale_rgba32(img.surface_rgba)) {
    if (!convert_to_grayscale_inplace(img.surface_rgba)) {
      cleanup_all(NULL, &img); return 1;
    }
  }

  UIContext ui = {0};
  if (!create_main_window(&ui, img.w, img.h)) { cleanup_all(&ui, &img); return 1; }
  if (!create_side_window(&ui))               { cleanup_all(&ui, &img); return 1; }

  img.texture = SDL_CreateTextureFromSurface(ui.mainApp.renderer, img.surface_rgba);
  if (!img.texture) { log_sdl_error("CreateTextureFromSurface"); cleanup_all(&ui, &img); return 1; }

  render_loop(&ui, &img);

  cleanup_all(&ui, &img);
  return 0;
}
