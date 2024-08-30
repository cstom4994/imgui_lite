// Lite - A lightweight text editor written in Lua
// ImLite - An embeddable Lite for dear imgui (MIT license)
// modified from https://github.com/rxi/lite (MIT license)
//               https://github.com/r-lyeh/FWK (public domain)
// modified by KaoruXun(cstom4994) for NekoEngine

#ifndef NEKO_LITE_H
#define NEKO_LITE_H

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include <imgui.h>

// opengl
#include <GL/glew.h>

// glfw
#include <GLFW/glfw3.h>

#define NEKO_LITE

// ----------------------------------------------------------------------------
// lite editor, platform details, some things from neko engine

#define NEKO_MAX(A, B) ((A) > (B) ? (A) : (B))
#define NEKO_MIN(A, B) ((A) < (B) ? (A) : (B))

#if defined(_WIN32)
#define NEKO_IS_WIN32
#elif defined(__EMSCRIPTEN__)
#define NEKO_IS_WEB
#elif defined(__linux__) || defined(__unix__)
#define NEKO_IS_LINUX
#elif defined(__APPLE__) || defined(_APPLE)
#define NEKO_IS_APPLE
#elif defined(__ANDROID__)
#define NEKO_IS_ANDROID
#endif

#if defined MAX_PATH
#define DIR_MAX MAX_PATH
#elif defined PATH_MAX
#define DIR_MAX PATH_MAX
#else
#define DIR_MAX 260
#endif

#define LT_DATAPATH "./"

#define lt_assert(x) assert(x)

#define lt_realpath(p, q) file_pathabs(p)
#define lt_realpath_free(p)

#define lt_malloc(n) malloc(n)
#define lt_calloc(n, m) calloc(n, m)
#define lt_free(p) free(p)
#define lt_realloc(p, s) realloc(p, s)
#define lt_memcpy(d, s, c) memcpy(d, s, c)
#define lt_memset(p, ch, c) memset(p, ch, c)

#define lt_time_ms() lt_time_now()
#define lt_sleep_ms(ms) os_sleep(ms)

#define lt_getclipboard(w) window_clipboard()
#define lt_setclipboard(w, s) window_setclipboard(s)

#define lt_window() glfw_window
// #define lt_setwindowmode(m) window_fullscreen(m == 2), (m < 2 && (window_maximize(m), 1))  // 0:normal,1:maximized,2:fullscreen
#define lt_setwindowmode(m)

#define lt_setwindowtitle(t)  // window_title(t)
#define lt_haswindowfocus() window_has_focus()
// #define lt_setcursor(shape) window_cursor_shape(lt_events & (1 << 31) ? CURSOR_SW_AUTO : shape + 1)  // 0:arrow,1:ibeam,2:sizeh,3:sizev,4:hand
#define lt_setcursor(shape)

#define lt_prompt(msg, title) (MessageBoxA(0, msg, title, MB_YESNO | MB_ICONWARNING) == IDYES)

typedef struct LT_Texture {
    GLuint id;  // 如果未初始化或纹理错误 则为 0
    int width;
    int height;
    int components;
} LT_Texture;

typedef struct lt_surface {
    int w, h;
    void *pixels;
    LT_Texture t;
} lt_surface;

typedef struct lt_rect {
    int x, y, width, height;
} lt_rect;

extern int lt_mx, lt_my, lt_wx, lt_wy, lt_ww, lt_wh;

lt_surface *lt_getsurface(void *window);

int lt_resizesurface(lt_surface *s, int ww, int wh);

// ----------------------------------------------------------------------------

#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif

#ifndef S_ISREG
#define S_ISREG(mode) (((mode) & S_IFMT) == S_IFREG)
#endif

// ----------------------------------------------------------------------------
// lite/api.h

#define API_TYPE_FONT "Font"

// ----------------------------------------------------------------------------
// lite/renderer.h

typedef struct RenImage RenImage;
typedef struct RenFont RenFont;

typedef struct {
    uint8_t b, g, r, a;
} RenColor;
// typedef struct { int x, y, width, height; } RenRect;
typedef lt_rect RenRect;

void ren_init(void *win);
void ren_update_rects(RenRect *rects, int count);
void ren_set_clip_rect(RenRect rect);
void ren_get_size(int *x, int *y);

RenImage *ren_new_image(int width, int height);
void ren_free_image(RenImage *image);

RenFont *ren_load_font(const char *filename, float size);
void ren_free_font(RenFont *font);
void ren_set_font_tab_width(RenFont *font, int n);
int ren_get_font_tab_width(RenFont *font);
int ren_get_font_width(RenFont *font, const char *text);
int ren_get_font_height(RenFont *font);

void ren_draw_rect(RenRect rect, RenColor color);
void ren_draw_image(RenImage *image, RenRect *sub, int x, int y, RenColor color);
int ren_draw_text(RenFont *font, const char *text, int x, int y, RenColor color);

// ----------------------------------------------------------------------------
// lite/rencache.h

void rencache_show_debug(bool enable);
void rencache_free_font(RenFont *font);
void rencache_set_clip_rect(RenRect rect);
void rencache_draw_rect(RenRect rect, RenColor color);
int rencache_draw_text(RenFont *font, const char *text, int x, int y, RenColor color);
void rencache_invalidate(void);
void rencache_begin_frame(void);
void rencache_end_frame(void);

// neko lite
void lt_init(lua_State *L, void *handle, const char *pathdata, int argc, char **argv, float scale, const char *platform);
void lt_tick(struct lua_State *L);
void lt_fini();

typedef enum {
    INPUT_WRAP_NONE = 0,
    INPUT_WRAP_WINDOW_MOVED = 1 << 1,
    INPUT_WRAP_WINDOW_RESIZED = 1 << 2,
    INPUT_WRAP_WINDOW_CLOSED = 1 << 3,
    INPUT_WRAP_WINDOW_REFRESH = 1 << 4,
    INPUT_WRAP_WINDOW_FOCUSED = 1 << 5,
    INPUT_WRAP_WINDOW_DEFOCUSED = 1 << 6,
    INPUT_WRAP_WINDOW_ICONIFIED = 1 << 7,
    INPUT_WRAP_WINDOW_UNICONIFIED = 1 << 8,
    INPUT_WRAP_FRAMEBUFFER_RESIZED = 1 << 9,
    INPUT_WRAP_BUTTON_PRESSED = 1 << 10,
    INPUT_WRAP_BUTTON_RELEASED = 1 << 11,
    INPUT_WRAP_CURSOR_MOVED = 1 << 12,
    INPUT_WRAP_CURSOR_ENTERED = 1 << 13,
    INPUT_WRAP_CURSOR_LEFT = 1 << 14,
    INPUT_WRAP_SCROLLED = 1 << 15,
    INPUT_WRAP_KEY_PRESSED = 1 << 16,
    INPUT_WRAP_KEY_REPEATED = 1 << 17,
    INPUT_WRAP_KEY_RELEASED = 1 << 18,
    INPUT_WRAP_CODEPOINT_INPUT = 1 << 19
} INPUT_WRAP_type;

typedef struct INPUT_WRAP_event {
    INPUT_WRAP_type type;
    union {
        struct {
            int x;
            int y;
        } pos;
        struct {
            int width;
            int height;
        } size;
        struct {
            double x;
            double y;
        } scroll;
        struct {
            int key;
            int scancode;
            int mods;
        } keyboard;
        struct {
            int button;
            int mods;
        } mouse;
        unsigned int codepoint;
    };
} INPUT_WRAP_event;

#ifndef INPUT_WRAP_CAPACITY
#define INPUT_WRAP_CAPACITY 256
#endif

typedef struct event_queue {
    INPUT_WRAP_event events[INPUT_WRAP_CAPACITY];
    size_t head;
    size_t tail;
} event_queue;

INPUT_WRAP_event *input_wrap_new_event(event_queue *equeue);
int input_wrap_next_e(event_queue *equeue, INPUT_WRAP_event *event);
void input_wrap_free_e(INPUT_WRAP_event *event);

#define INPUT_WRAP_DEFINE(NAME)                                              \
    static event_queue NAME##_input_queue = {{INPUT_WRAP_NONE}, 0, 0};       \
    static void NAME##_char_down(unsigned int c) {                           \
        INPUT_WRAP_event *event = input_wrap_new_event(&NAME##_input_queue); \
        event->type = INPUT_WRAP_CODEPOINT_INPUT;                            \
        event->codepoint = c;                                                \
    }                                                                        \
    static void NAME##_key_down(int key, int scancode, int mods) {           \
        INPUT_WRAP_event *event = input_wrap_new_event(&NAME##_input_queue); \
        event->keyboard.key = key;                                           \
        event->keyboard.scancode = scancode;                                 \
        event->keyboard.mods = mods;                                         \
        event->type = INPUT_WRAP_KEY_PRESSED;                                \
    }                                                                        \
    static void NAME##_key_up(int key, int scancode, int mods) {             \
        INPUT_WRAP_event *event = input_wrap_new_event(&NAME##_input_queue); \
        event->keyboard.key = key;                                           \
        event->keyboard.scancode = scancode;                                 \
        event->keyboard.mods = mods;                                         \
        event->type = INPUT_WRAP_KEY_RELEASED;                               \
    }                                                                        \
    static void NAME##_mouse_down(int mouse) {                               \
        INPUT_WRAP_event *event = input_wrap_new_event(&NAME##_input_queue); \
        event->mouse.button = mouse;                                         \
        event->type = INPUT_WRAP_BUTTON_PRESSED;                             \
    }                                                                        \
    static void NAME##_mouse_up(int mouse) {                                 \
        INPUT_WRAP_event *event = input_wrap_new_event(&NAME##_input_queue); \
        event->mouse.button = mouse;                                         \
        event->type = INPUT_WRAP_BUTTON_RELEASED;                            \
    }                                                                        \
    static void NAME##_mouse_move(ImVec2 pos) {                              \
        INPUT_WRAP_event *event = input_wrap_new_event(&NAME##_input_queue); \
        event->type = INPUT_WRAP_CURSOR_MOVED;                               \
        event->pos.x = (int)pos.x;                                           \
        event->pos.y = (int)pos.y;                                           \
    }                                                                        \
    static void NAME##_scroll(ImVec2 scroll) {                               \
        INPUT_WRAP_event *event = input_wrap_new_event(&NAME##_input_queue); \
        event->type = INPUT_WRAP_SCROLLED;                                   \
        event->scroll.x = scroll.x;                                          \
        event->scroll.y = scroll.y;                                          \
    }

#endif