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
    SDL_Window* window;
    SDL_Renderer* renderer;
} AppContext;

typedef struct {
    SDL_Surface* surface_rgba; 
    SDL_Texture* texture;  
    int w, h;
} ImageData;

typedef struct {
    AppContext mainApp;
    AppContext sideApp;
} UIContext;

//constantes
#define SIDE_MARGIN 16

//declaração de função
static void   log_sdl_error(const char* msg);
static bool   img_load_rgba32(const char* path, ImageData* out);
static bool   is_surface_grayscale_rgba32(const SDL_Surface* surf);
static bool   convert_to_grayscale_inplace(SDL_Surface* surf);
static bool   create_main_window(UIContext* ui, int imgw, int imgh);
static bool   create_side_window(UIContext* ui, int imgw, int imgh);
static void   cleanup_all(UIContext* ui, ImageData* img, ImageData* sobel_img);
static void   render_main_window(UIContext* ui, ImageData* img);
static void   render_side_window(UIContext* ui, ImageData* sobel_img);
static void   handle_events(UIContext* ui);
static void   render_loop(UIContext* ui, ImageData* img, ImageData* sobel_img);
static bool   apply_sobel_filter(SDL_Surface* src_gray, SDL_Surface* dst_rgba);
static Uint8  get_pixel_gray_safe(const SDL_Surface* surf, int x, int y, const SDL_PixelFormatDetails* fmt, const SDL_Palette* pal);
void shutdown(void);

//funções
static void log_sdl_error(const char* msg) {
    SDL_Log("%s: %s", msg, SDL_GetError());
}

void shutdown(void) {
    SDL_Log("shutdown()");
    SDL_Quit();
}

static bool img_load_rgba32(const char* path, ImageData* out) {
    SDL_Log("Carregando: %s", path);
    SDL_Surface* loaded = IMG_Load(path);
    if (!loaded) { log_sdl_error("IMG_Load falhou"); return false; }
    SDL_Surface* rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(loaded);
    if (!rgba) { log_sdl_error("SDL_ConvertSurface para RGBA32 falhou"); return false; }
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
static bool is_surface_grayscale_rgba32(const SDL_Surface* surf) {
    if (!surf) return false;
    if (!SDL_LockSurface((SDL_Surface*)surf)) { SDL_Log("Falha ao SDL_LockSurface (is_surface_grayscale): %s", SDL_GetError()); return false; }
    const Uint32* pixels = (const Uint32*)surf->pixels;
    const int count = surf->w * surf->h;
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
    if (!SDL_LockSurface(surf)) { SDL_Log("Falha ao SDL_LockSurface (convert_to_grayscale): %s", SDL_GetError()); return false; }
    Uint32* pixels = (Uint32*)surf->pixels;
    const int count = surf->w * surf->h;
    const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(surf->format);
    const SDL_Palette* pal = SDL_GetSurfacePalette(surf);
    for (int i = 0; i < count; i++) {
        Uint8 r, g, b, a;
        SDL_GetRGBA(pixels[i], fmt, pal, &r, &g, &b, &a);
        float Yf = 0.2125f * (float)r + 0.7154f * (float)g + 0.0721f * (float)b;
        Uint8 Y = (Uint8)(Yf + 0.5f);
        pixels[i] = SDL_MapRGBA(fmt, pal, Y, Y, Y, a);
    }
    SDL_UnlockSurface(surf);
    return true;
}

static bool create_main_window(UIContext* ui, int imgw, int imgh) {
    int w = imgw, h = imgh;
    if (w > 1400) { float s = 1400.0f / (float)w; w = 1400; h = (int)(imgh * s); }
    if (h > 900)  { float s =  900.0f / (float)h; h = 900;  w = (int)(w * s); }
    if (w < 320) w = 320;
    if (h < 240) h = 240;
    
    ui->mainApp.window = SDL_CreateWindow("Imagem (principal)", w, h, SDL_WINDOW_RESIZABLE);
    if (!ui->mainApp.window) { log_sdl_error("Create main window"); return false; }    
    SDL_DisplayID displayID = SDL_GetPrimaryDisplay();
    SDL_Rect displayBounds = {0};
    if (!SDL_GetDisplayBounds(displayID, &displayBounds)) {
         SDL_Log("Não foi possível obter os limites da tela: %s", SDL_GetError());
         SDL_SetWindowPosition(ui->mainApp.window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    } else {
        int total_width = (w * 2) + SIDE_MARGIN;
        int start_x = (displayBounds.w - total_width) / 2;
        int start_y = (displayBounds.h - h) / 2;
        SDL_SetWindowPosition(ui->mainApp.window, start_x, start_y);
    }

    ui->mainApp.renderer = SDL_CreateRenderer(ui->mainApp.window, NULL);
    if (!ui->mainApp.renderer) { log_sdl_error("Create main renderer"); return false; }
    return true;
}
static bool create_side_window(UIContext* ui, int imgw, int imgh) {
    int w = imgw, h = imgh;
    if (w > 1400) { float s = 1400.0f / (float)w; w = 1400; h = (int)(imgh * s); }
    if (h > 900)  { float s =  900.0f / (float)h; h = 900;  w = (int)(w * s); }
    if (w < 320) w = 320;
    if (h < 240) h = 240;

    ui->sideApp.window = SDL_CreateWindow("Detecção de Bordas (Sobel)", w, h, SDL_WINDOW_RESIZABLE); // --- MUDANÇA --- Título e tamanho
    if (!ui->sideApp.window) { log_sdl_error("Create side window"); return false; }
    ui->sideApp.renderer = SDL_CreateRenderer(ui->sideApp.window, NULL);
    if (!ui->sideApp.renderer) { log_sdl_error("Create side renderer"); return false; }
    int x,y,main_w,main_h;
    SDL_GetWindowPosition(ui->mainApp.window, &x,&y);
    SDL_GetWindowSize(ui->mainApp.window, &main_w,&main_h);
    SDL_SetWindowPosition(ui->sideApp.window, x + main_w + SIDE_MARGIN, y);
    
    return true;
}

static void cleanup_all(UIContext* ui, ImageData* img, ImageData* sobel_img) {
    if (ui) {
    }
    if (img) {
        if (img->texture)       SDL_DestroyTexture(img->texture);
        if (img->surface_rgba) SDL_DestroySurface(img->surface_rgba);
    }
    if (sobel_img) {
        if (sobel_img->texture)       SDL_DestroyTexture(sobel_img->texture);
        if (sobel_img->surface_rgba) SDL_DestroySurface(sobel_img->surface_rgba);
    }
    if (ui) {
        if (ui->mainApp.renderer) SDL_DestroyRenderer(ui->mainApp.renderer);
        if (ui->mainApp.window)   SDL_DestroyWindow(ui->mainApp.window);
        if (ui->sideApp.renderer) SDL_DestroyRenderer(ui->sideApp.renderer);
        if (ui->sideApp.window)   SDL_DestroyWindow(ui->sideApp.window);
    }
}

static void render_side_window(UIContext* ui, ImageData* sobel_img) {
    SDL_SetRenderDrawColor(ui->sideApp.renderer, 20,20,20,255);
    SDL_RenderClear(ui->sideApp.renderer);
    
    int ww, wh;
    SDL_GetWindowSize(ui->sideApp.window, &ww, &wh);
    float iW = (float)sobel_img->w, iH = (float)sobel_img->h;
    float wW = (float)ww,     wH = (float)wh;
    float scale = (wW / iW < wH / iH) ? (wW / iW) : (wH / iH);
    float dw = iW * scale;
    float dh = iH * scale;
    SDL_FRect dst = { (wW - dw) * 0.5f, (wH - dh) * 0.5f, dw, dh };
    
    SDL_RenderTexture(ui->sideApp.renderer, sobel_img->texture, NULL, &dst);
    SDL_RenderPresent(ui->sideApp.renderer);
}

static void handle_events(UIContext* ui) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) exit(0);
        
        if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                    SDL_Log("Recebido SDL_EVENT_WINDOW_CLOSE_REQUESTED para a janela ID %u. Encerrando.", e.window.windowID);
                    exit(0);
                }

        if (e.type == SDL_EVENT_WINDOW_MOVED && e.window.windowID == SDL_GetWindowID(ui->mainApp.window)) {
            int x,y,w,h;
            SDL_GetWindowPosition(ui->mainApp.window, &x,&y);
            SDL_GetWindowSize(ui->mainApp.window, &w,&h);
            SDL_SetWindowPosition(ui->sideApp.window, x + w + SIDE_MARGIN, y);
        }

        if (e.type == SDL_EVENT_WINDOW_RESIZED) {
             if (e.window.windowID == SDL_GetWindowID(ui->mainApp.window)) {
                int w, h;
                SDL_GetWindowSize(ui->mainApp.window, &w, &h);
                SDL_SetWindowSize(ui->sideApp.window, w, h);
                int x,y;
                SDL_GetWindowPosition(ui->mainApp.window, &x,&y);
                SDL_SetWindowPosition(ui->sideApp.window, x + w + SIDE_MARGIN, y);
             } else if (e.window.windowID == SDL_GetWindowID(ui->sideApp.window)) {
                int w, h;
                SDL_GetWindowSize(ui->sideApp.window, &w, &h);
                SDL_SetWindowSize(ui->mainApp.window, w, h);
             }
        }

        if (e.type == SDL_EVENT_KEY_DOWN) {
            if (e.key.key == SDLK_ESCAPE) exit(0);
            

        }
        
    }
}

static void render_loop(UIContext* ui, ImageData* img, ImageData* sobel_img) {
    for (;;) {
        handle_events(ui); 
        render_main_window(ui, img);
        render_side_window(ui, sobel_img); 
        SDL_Delay(16);
    }
}

static Uint8 get_pixel_gray_safe(const SDL_Surface* surf, int x, int y, 
                                 const SDL_PixelFormatDetails* fmt, const SDL_Palette* pal) {
    if (x < 0 || x >= surf->w || y < 0 || y >= surf->h) {
        return 0;
    }
    
    const Uint32* pixels = (const Uint32*)surf->pixels;
    Uint32 p = pixels[y * surf->w + x];
    
    Uint8 r,g,b,a;
    SDL_GetRGBA(p, fmt, pal, &r, &g, &b, &a);
    return r;
}

static bool apply_sobel_filter(SDL_Surface* src_gray, SDL_Surface* dst_rgba) {
    if (!src_gray || !dst_rgba) return false;
        SDL_Surface* blurred_surf = SDL_CreateSurface(src_gray->w, src_gray->h, SDL_PIXELFORMAT_RGBA32);
    if (!blurred_surf) {
        log_sdl_error("Falha ao criar surface para blur");
        return false;
    }

    if (!SDL_LockSurface(src_gray) || !SDL_LockSurface(blurred_surf)) {
        log_sdl_error("Lock falhou (blur)");
        if(SDL_MUSTLOCK(src_gray)) SDL_UnlockSurface(src_gray);
        SDL_DestroySurface(blurred_surf);
        return false;
    }

    const SDL_PixelFormatDetails* fmt_src = SDL_GetPixelFormatDetails(src_gray->format);
    const SDL_Palette* pal_src = SDL_GetSurfacePalette(src_gray);
    const SDL_PixelFormatDetails* fmt_blur = SDL_GetPixelFormatDetails(blurred_surf->format);
    const SDL_Palette* pal_blur = SDL_GetSurfacePalette(blurred_surf);
    Uint32* blur_px = (Uint32*)blurred_surf->pixels;
    const int w = src_gray->w;
    const int h = src_gray->h;

    SDL_Log("Aplicando 3x3 Box Blur...");
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int sum = 0;
            for (int j = -1; j <= 1; j++) {
                for (int i = -1; i <= 1; i++) {
                    sum += get_pixel_gray_safe(src_gray, x + i, y + j, fmt_src, pal_src);
                }
            }
            Uint8 Y = (Uint8)(sum / 9);
            blur_px[y * w + x] = SDL_MapRGBA(fmt_blur, pal_blur, Y, Y, Y, 255);
        }
    }
    SDL_UnlockSurface(src_gray);
    if (!SDL_LockSurface(dst_rgba)) {
        log_sdl_error("Lock falhou (sobel write)");
        SDL_UnlockSurface(blurred_surf);
        SDL_DestroySurface(blurred_surf);
        return false;
    }
    
    Uint32* dst_px = (Uint32*)dst_rgba->pixels;
    const SDL_PixelFormatDetails* fmt_dst = SDL_GetPixelFormatDetails(dst_rgba->format);
    const SDL_Palette* pal_dst = SDL_GetSurfacePalette(dst_rgba);

    const int Gx[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
    const int Gy[3][3] = {{-1,-2,-1}, { 0, 0, 0}, { 1, 2, 1}};

    SDL_Log("Aplicando Filtro Sobel...");
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            long sumX = 0;
            long sumY = 0;

            for (int j = -1; j <= 1; j++) {
                for (int i = -1; i <= 1; i++) {
                    Uint8 val = get_pixel_gray_safe(blurred_surf, x + i, y + j, fmt_blur, pal_blur);
                    sumX += (long)val * Gx[j + 1][i + 1];
                    sumY += (long)val * Gy[j + 1][i + 1];
                }
            }
            double mag = sqrt((double)(sumX * sumX) + (double)(sumY * sumY));

            if (mag > 255.0) mag = 255.0;
            
            Uint8 Y = (Uint8)mag;
            dst_px[y * w + x] = SDL_MapRGBA(fmt_dst, pal_dst, Y, Y, Y, 255);
        }
    }

    SDL_UnlockSurface(blurred_surf);
    SDL_UnlockSurface(dst_rgba);
    SDL_DestroySurface(blurred_surf);

    SDL_Log("Detecção de bordas concluída.");
    return true;
}


int main(int argc, char** argv) {
    atexit(shutdown);
    if (argc != 2) { SDL_Log("Uso: %s <caminho_imagem>", argv[0]); return 1; }
    if (!SDL_Init(SDL_INIT_VIDEO)) { log_sdl_error("SDL_Init: Erro ao inicializar"); return 1; }
    ImageData img = {0};
    if (!img_load_rgba32(argv[1], &img)) { cleanup_all(NULL, &img, NULL); return 1; }
    if (!is_surface_grayscale_rgba32(img.surface_rgba)) {
        if (!convert_to_grayscale_inplace(img.surface_rgba)) {
            cleanup_all(NULL, &img, NULL); return 1;
        }
    }
    ImageData sobel_img = {0};
    sobel_img.w = img.w;
    sobel_img.h = img.h;
    sobel_img.surface_rgba = SDL_CreateSurface(img.w, img.h, SDL_PIXELFORMAT_RGBA32);
    if (!sobel_img.surface_rgba) {
        log_sdl_error("Falha ao criar surface para sobel_img");
        cleanup_all(NULL, &img, &sobel_img);
        return 1;
    }
    SDL_Log("Processando filtro Sobel na inicialização...");
    if (!apply_sobel_filter(img.surface_rgba, sobel_img.surface_rgba)) {
        SDL_Log("Falha ao aplicar filtro Sobel na inicialização");
        cleanup_all(NULL, &img, &sobel_img);
        return 1;
    }

    UIContext ui = {0};
    
    if (!create_main_window(&ui, img.w, img.h)) { cleanup_all(&ui, &img, &sobel_img); return 1; }
    if (!create_side_window(&ui, img.w, img.h)) { cleanup_all(&ui, &img, &sobel_img); return 1; }
    img.texture = SDL_CreateTextureFromSurface(ui.mainApp.renderer, img.surface_rgba);
    if (!img.texture) { log_sdl_error("CreateTextureFromSurface (main)"); cleanup_all(&ui, &img, &sobel_img); return 1; }
    
    sobel_img.texture = SDL_CreateTextureFromSurface(ui.sideApp.renderer, sobel_img.surface_rgba);
    if (!sobel_img.texture) { log_sdl_error("CreateTextureFromSurface (side)"); cleanup_all(&ui, &img, &sobel_img); return 1; }


    render_loop(&ui, &img, &sobel_img); 
    
    cleanup_all(&ui, &img, &sobel_img); 
    return 0;
}