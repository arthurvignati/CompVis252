//includes
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string.h>   
#include <math.h> 
#define FONT_PATH "Roboto-Regular.ttf"

//variáveis e structs
typedef struct {
  SDL_Window*   window;
  SDL_Renderer* renderer;
} AppContext;

typedef struct {
  SDL_Surface* surface_rgba;   //surface de trabalho em RGBA32 
  SDL_Texture* texture;        //textura para renderizar 
  int w, h;
  SDL_Surface* original_gray;
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
  TTF_Font*  font;
  float      yzoom; 
  float      mean, stddev;
  char       meanLabel[64];
  char       stdLabel[64];
  bool is_equalized;
} UIContext;

//constantes
#define SIDE_W  320
#define SIDE_H  440
#define SIDE_MARGIN 16
#define BUTTON_H 36
#define BUTTON_W 160

//declaração de função
static void  log_sdl_error(const char* msg);
static bool  img_load_rgba32(const char* path, ImageData* out);
static bool  is_surface_grayscale_rgba32(const SDL_Surface* surf);
static bool  convert_to_grayscale_inplace(SDL_Surface* surf);
static void  compute_histogram_gray_rgba32(const SDL_Surface* surf, Uint32 hist[256], float* out_mean, float* out_stddev);
static void  draw_histogram(SDL_Renderer* rr, const Uint32 hist[256], SDL_FRect area, float yzoom);
static void  draw_button(SDL_Renderer* rr, const UIButton* btn, TTF_Font* font, bool is_equalized);
static bool  create_main_window(UIContext* ui, int imgw, int imgh);
static bool  create_side_window(UIContext* ui);
static void  cleanup_all(UIContext* ui, ImageData* img);
static void  render_main_window(UIContext* ui, ImageData* img);
static void  render_side_window(UIContext* ui);
static void  handle_events(UIContext* ui, ImageData* img);
static void  render_loop(UIContext* ui, ImageData* img);
static bool  equalize_histogram_inplace(SDL_Surface* surf);
void shutdown(void);

//funções
static void log_sdl_error(const char* msg) {
  SDL_Log("*** %s: %s", msg, SDL_GetError());
}

void shutdown(void) {
  SDL_Log("shutdown()");
  TTF_Quit();
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


static const char* classify_mean(float mean) {
  if (mean < 85.f)   return "escura";
  if (mean < 170.f)  return "média";
  return "clara";
}

static const char* classify_stddev(float sd) {
  if (sd < 30.f)     return "baixo";
  if (sd < 60.f)     return "médio";
  return "alto";
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
  const SDL_Palette* pal = SDL_GetSurfacePalette((SDL_Surface*)surf);

  bool is_gray = true;
  for (int i = 0; i < count; i++) {
    Uint8 r, g, b, a;
    SDL_GetRGBA(pixels[i], fmt, pal, &r, &g, &b, &a);
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
   const SDL_Palette* pal = SDL_GetSurfacePalette(surf);
  for (int i = 0; i < count; i++) {
    Uint8 r, g, b, a;
    SDL_GetRGBA(pixels[i], fmt, pal, &r, &g, &b, &a);

    // Y = 0.2125R + 0.7154G + 0.0721B 
    float Yf = 0.2125f * (float)r + 0.7154f * (float)g + 0.0721f * (float)b;
    Uint8 Y = (Uint8)(Yf + 0.5f);

    pixels[i] = SDL_MapRGBA(fmt, pal, Y, Y, Y, a);
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
  const SDL_Palette* pal = SDL_GetSurfacePalette((SDL_Surface*)surf);

  double sum = 0.0, sum2 = 0.0;
  for (int i = 0; i < count; i++) {
    Uint8 r,g,b,a;
    SDL_GetRGBA(p[i], fmt, pal, &r,&g,&b,&a);
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
                           SDL_FRect area, float yzoom)
{
  // fundo
  SDL_SetRenderDrawColor(rr, 30,30,30,255);
  SDL_RenderFillRect(rr, &area);
  SDL_SetRenderDrawColor(rr, 60,60,60,255);
  for (int i = 1; i <= 4; i++) {
    float y = area.y + (area.h * i) / 5.0f;
    SDL_RenderLine(rr, area.x, y, area.x + area.w, y);
  }

  // max
  Uint32 maxv = 1;
  for (int i = 0; i < 256; i++) if (hist[i] > maxv) maxv = hist[i];

  // barras
  float barw = area.w / 256.0f;
  if (barw < 2.0f) barw = 2.0f; // deixa as barras mais grossas
  SDL_SetRenderDrawColor(rr, 220,220,220,255);

  float usableH = area.h - 2.0f;
  for (int i = 0; i < 256; i++) {
    float h = ((float)hist[i] / (float)maxv) * usableH * yzoom;
    if (h > usableH) h = usableH; // clampa
    SDL_FRect bar = { area.x + i * (area.w/256.0f), area.y + (area.h - h), barw, h };
    SDL_RenderFillRect(rr, &bar);
  }

  // moldura
  SDL_SetRenderDrawColor(rr, 100,100,100,255);
  SDL_RenderRect(rr, &area);
}

static void draw_text(SDL_Renderer* rr, TTF_Font* font,
                      const char* msg, int x, int y)
{
  if (!font || !msg || !*msg) return;

  SDL_Color white = (SDL_Color){ 230, 230, 230, 255 };

  // length = 0  => string null-terminated (UTF-8)
  SDL_Surface* surf = TTF_RenderText_Blended(font, msg, 0, white);
  if (!surf) {
    SDL_Log("TTF_RenderText_Blended falhou: %s", SDL_GetError());
    return;
  }

  SDL_Texture* tex = SDL_CreateTextureFromSurface(rr, surf);
  if (!tex) {
    SDL_Log("CreateTextureFromSurface (texto) falhou: %s", SDL_GetError());
    SDL_DestroySurface(surf);
    return;
  }

  SDL_FRect dst = { (float)x, (float)y, (float)surf->w, (float)surf->h };
  SDL_RenderTexture(rr, tex, NULL, &dst);

  SDL_DestroyTexture(tex);
  SDL_DestroySurface(surf);
}


static void draw_button(SDL_Renderer* rr, const UIButton* btn, TTF_Font* font, bool is_equalized) {
  // cores por estado
  SDL_Color fill;
  switch (btn->state) {
    case BTN_IDLE:   fill = (SDL_Color){  0,102,204,255}; break; // azul
    case BTN_HOVER:  fill = (SDL_Color){ 30,144,255,255}; break; // azul claro
    case BTN_ACTIVE: fill = (SDL_Color){  0, 70,160,255}; break; // azul escuro
    default:         fill = (SDL_Color){  0,102,204,255}; break;
  }
  SDL_SetRenderDrawColor(rr, fill.r, fill.g, fill.b, fill.a);
  SDL_RenderFillRect(rr, &btn->rect);

  // borda
  SDL_SetRenderDrawColor(rr, 20,20,20,255);
  SDL_RenderRect(rr, &btn->rect);

  // rótulo
  const char* label = is_equalized ? "Original" : "Equalizar";
  SDL_Surface* s = TTF_RenderText_Blended(font, label, 0, (SDL_Color){240,240,240,255});
  if (!s) return;
  SDL_Texture* t = SDL_CreateTextureFromSurface(rr, s);
  if (!t) { SDL_DestroySurface(s); return; }

  SDL_FRect dst = {
    btn->rect.x + (btn->rect.w - s->w) * 0.5f,
    btn->rect.y + (btn->rect.h - s->h) * 0.5f,
    (float)s->w, (float)s->h
  };
  SDL_RenderTexture(rr, t, NULL, &dst);
  SDL_DestroyTexture(t);
  SDL_DestroySurface(s);
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
  ui->eqButton.rect.y = 0;
  ui->eqButton.state  = BTN_IDLE;
  return true;
}



static void cleanup_all(UIContext* ui, ImageData* img) {
  if (ui) {
    if (ui->font) { TTF_CloseFont(ui->font); ui->font = NULL; }
  }
  if (img) {
    if (img->texture)      SDL_DestroyTexture(img->texture);
    if (img->surface_rgba) SDL_DestroySurface(img->surface_rgba);
    if (img->original_gray) SDL_DestroySurface(img->original_gray);
  }
  if (ui) {
    if (ui->mainApp.renderer) SDL_DestroyRenderer(ui->mainApp.renderer);
    if (ui->mainApp.window)   SDL_DestroyWindow(ui->mainApp.window);
    if (ui->sideApp.renderer) SDL_DestroyRenderer(ui->sideApp.renderer);
    if (ui->sideApp.window)   SDL_DestroyWindow(ui->sideApp.window);
  }
}

static bool point_in_rect(float x, float y, SDL_FRect r) {
  return (x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h);
}

static void rebuild_texture(ImageData* img, SDL_Renderer* rr) {
  if (img->texture) { SDL_DestroyTexture(img->texture); img->texture = NULL; }
  img->texture = SDL_CreateTextureFromSurface(rr, img->surface_rgba);
  if (!img->texture) SDL_Log("*** CreateTextureFromSurface falhou: %s", SDL_GetError());
}

static void recompute_stats(UIContext* ui, ImageData* img) {
  compute_histogram_gray_rgba32(img->surface_rgba, ui->hist, &ui->mean, &ui->stddev);
  snprintf(ui->meanLabel, sizeof(ui->meanLabel),
           "Média de intensidade: %.1f (%s)", ui->mean, classify_mean(ui->mean));
  snprintf(ui->stdLabel, sizeof(ui->stdLabel),
           "Desvio padrão: %.1f (contraste %s)", ui->stddev, classify_stddev(ui->stddev));
}

static void render_side_window(UIContext* ui) {
  SDL_SetRenderDrawColor(ui->sideApp.renderer, 15,15,15,255);
  SDL_RenderClear(ui->sideApp.renderer);

  // medidas de texto
  int line_h = TTF_GetFontLineSkip(ui->font);
  if (line_h <= 0) line_h = 18; // fallback
  const float gap = 6.0f;
  const float labels_h = (float)(line_h * 2) + gap; // duas linhas + respiro

  // área do histograma agora reserva espaço p/ textos E botão
  SDL_FRect histArea = {
    SIDE_MARGIN,
    SIDE_MARGIN,
    SIDE_W - SIDE_MARGIN * 2.0f,
    SIDE_H - SIDE_MARGIN * 3.0f - BUTTON_H - labels_h
  };
  if (histArea.h < 80.0f) histArea.h = 80.0f; // evita ficar negativo/pequeno demais

  // histograma
  draw_histogram(ui->sideApp.renderer, ui->hist, histArea, ui->yzoom);

  // textos logo abaixo do histograma
  int textX = (int)histArea.x + 6;
  int textY = (int)(histArea.y + histArea.h) + (int)gap;
  draw_text(ui->sideApp.renderer, ui->font, ui->meanLabel, textX, textY);
  textY += line_h; // próxima linha
  draw_text(ui->sideApp.renderer, ui->font, ui->stdLabel,  textX, textY);

  // botão abaixo dos textos
  ui->eqButton.rect.y = (float)( (int)(histArea.y + histArea.h) + (int)labels_h );
  draw_button(ui->sideApp.renderer, &ui->eqButton, ui->font, ui->is_equalized);

  SDL_RenderPresent(ui->sideApp.renderer);
}


static void handle_events(UIContext* ui, ImageData* img) {
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_EVENT_QUIT) exit(0);

    if (e.type == SDL_EVENT_WINDOW_RESIZED || e.type == SDL_EVENT_WINDOW_MOVED) {
      int x,y,w,h;
      SDL_GetWindowPosition(ui->mainApp.window, &x,&y);
      SDL_GetWindowSize(ui->mainApp.window, &w,&h);
      SDL_SetWindowPosition(ui->sideApp.window, x + w + SIDE_MARGIN, y);
    }

    if (e.type == SDL_EVENT_KEY_DOWN) {
      if (e.key.key == SDLK_ESCAPE) exit(0);
      if (e.key.key == SDLK_EQUALS || e.key.key == SDLK_PLUS)
        ui->yzoom = ui->yzoom < 4.0f ? ui->yzoom + 0.25f : 4.0f;
      if (e.key.key == SDLK_MINUS)
        ui->yzoom = ui->yzoom > 0.25f ? ui->yzoom - 0.25f : 0.25f;
    }

    // eventos do botão (apenas quando mouse está na janela secundária)
    if (e.type == SDL_EVENT_MOUSE_MOTION || e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_BUTTON_UP) {
      SDL_Window* focus = SDL_GetMouseFocus();
      if (focus == ui->sideApp.window) {
        float mx = (float)e.motion.x;
        float my = (float)e.motion.y;
        bool inside = point_in_rect(mx, my, ui->eqButton.rect);

        if (e.type == SDL_EVENT_MOUSE_MOTION) {
          ui->eqButton.state = inside ? BTN_HOVER : BTN_IDLE;
        }

        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && inside && e.button.button == SDL_BUTTON_LEFT) {
          ui->eqButton.state = BTN_ACTIVE;
        }

        if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT) {
          if (inside && ui->eqButton.state == BTN_ACTIVE) {
            // equalização
            ui->is_equalized = !ui->is_equalized;

            if (ui->is_equalized) {
              // aplica equalização na imagem atual (que está em cinza)
              if (!equalize_histogram_inplace(img->surface_rgba)) {
                SDL_Log("*** equalize_histogram_inplace falhou");
                ui->is_equalized = false;
              }
            } else {
              // reverte a partir do backup
              if (!SDL_LockSurface(img->surface_rgba) || !SDL_LockSurface(img->original_gray)) {
                SDL_Log("*** Lock para reverter falhou: %s", SDL_GetError());
                if (SDL_MUSTLOCK(img->original_gray)) SDL_UnlockSurface(img->original_gray);
                if (SDL_MUSTLOCK(img->surface_rgba))  SDL_UnlockSurface(img->surface_rgba);
              } else {
                memcpy(img->surface_rgba->pixels, img->original_gray->pixels, (size_t)img->h * img->original_gray->pitch);
                SDL_UnlockSurface(img->original_gray);
                SDL_UnlockSurface(img->surface_rgba);
              }
            }

            // atualizar textura e estatísticas
            rebuild_texture(img, ui->mainApp.renderer);
            recompute_stats(ui, img);
          }
          // estado visual pós-click
          ui->eqButton.state = inside ? BTN_HOVER : BTN_IDLE;
        }
      }
    }
  }
}

static void render_loop(UIContext* ui, ImageData* img) {
  recompute_stats(ui, img);

  for (;;) {
    handle_events(ui, img);
    render_main_window(ui, img);
    render_side_window(ui);
    SDL_Delay(16);
  }
}


// equaliza in-place assumindo imagem em GRAYSCALE já (R==G==B) em RGBA32
static bool equalize_histogram_inplace(SDL_Surface* surf) {
  if (!surf) return false;
  if (!SDL_LockSurface(surf)) return false;

  const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(surf->format);
  const SDL_Palette* pal = SDL_GetSurfacePalette(surf);
  Uint32* px = (Uint32*)surf->pixels;
  const int n = surf->w * surf->h;

  // 1) histograma
  Uint32 hist[256] = {0};
  for (int i = 0; i < n; i++) {
    Uint8 r,g,b,a; SDL_GetRGBA(px[i], fmt, pal, &r,&g,&b,&a);
    hist[r]++; // r==g==b
  }

  // 2) CDF normalizada
  Uint32 cum = 0; double cdf[256];
  // Uint32 min_nonzero = 0;
  // bool found = false;
  for (int i = 0; i < 256; i++) {
    // if (!found && hist[i] != 0) { min_nonzero = hist[i]; found = true; }
    cum += hist[i];
    cdf[i] = (double)cum / (double)n;   // [0..1]
  }

  // 3) mapeia intensidade antiga -> nova (0..255)
  Uint8 lut[256];
  for (int i = 0; i < 256; i++) {
    int v = (int)round(cdf[i] * 255.0);
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    lut[i] = (Uint8)v;
  }

  // 4) aplica
  for (int i = 0; i < n; i++) {
    Uint8 r,g,b,a; SDL_GetRGBA(px[i], fmt, pal, &r,&g,&b,&a);
    Uint8 y = lut[r];
    px[i] = SDL_MapRGBA(fmt, pal, y, y, y, a);
  }

  SDL_UnlockSurface(surf);
  return true;
}


int main(int argc, char** argv) {
  atexit(shutdown);

  if (argc != 2) {
    SDL_Log("Uso: %s <caminho_imagem>", argv[0]);
    return 1;
  }

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    log_sdl_error("SDL_Init: Erro ao inicializar"); return 1;
  }
  if (!TTF_Init()) {
    SDL_Log("*** TTF_Init falhou: %s", SDL_GetError());
    return 1;

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


  // cria backup da imagem em cinza para poder reverter
  img.original_gray = SDL_CreateSurface(img.w, img.h, SDL_PIXELFORMAT_RGBA32);
  if (!img.original_gray) {
    SDL_Log("*** Falha ao criar surface original_gray: %s", SDL_GetError());
    cleanup_all(NULL, &img); return 1;
  }
  if (!SDL_LockSurface(img.surface_rgba) || !SDL_LockSurface(img.original_gray)) {
    SDL_Log("*** Falha ao lockar para copiar original_gray: %s", SDL_GetError());
    if (SDL_MUSTLOCK(img.original_gray)) SDL_UnlockSurface(img.original_gray);
    if (SDL_MUSTLOCK(img.surface_rgba))  SDL_UnlockSurface(img.surface_rgba);
    cleanup_all(NULL, &img); return 1;
  }
  memcpy(img.original_gray->pixels, img.surface_rgba->pixels,
        (size_t)img.h * img.surface_rgba->pitch);
  SDL_UnlockSurface(img.original_gray);
  SDL_UnlockSurface(img.surface_rgba);


  UIContext ui = {0};
  ui.is_equalized = false;
  ui.yzoom = 1.5f;
  if (!create_main_window(&ui, img.w, img.h)) { cleanup_all(&ui, &img); return 1; }
  if (!create_side_window(&ui))               { cleanup_all(&ui, &img); return 1; }

  ui.font = TTF_OpenFont(FONT_PATH, 16);  // tamanho 16 px
  if (!ui.font) {
    SDL_Log("*** Falha ao abrir fonte '%s': %s", FONT_PATH, SDL_GetError());
    cleanup_all(&ui, &img);
    return 1;
  }

  img.texture = SDL_CreateTextureFromSurface(ui.mainApp.renderer, img.surface_rgba);
  if (!img.texture) { log_sdl_error("CreateTextureFromSurface"); cleanup_all(&ui, &img); return 1; }

  render_loop(&ui, &img);

  cleanup_all(&ui, &img);
  return 0;
}
