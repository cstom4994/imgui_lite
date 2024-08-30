// Lite - A lightweight text editor written in Lua
// ImLite - An embeddable Lite for dear imgui (MIT license)
// modified from https://github.com/rxi/lite (MIT license)
//               https://github.com/r-lyeh/FWK (public domain)
// modified by KaoruXun(cstom4994) for NekoEngine

#include "lite.h"

#include <direct.h>

#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

#if (defined NEKO_IS_WIN32)
#include <Windows.h>
#endif

#include <imgui_impl_glfw.h>

// deps
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

// ----------------------------------------------------------------------------

extern GLFWwindow *glfw_window;

int lt_mx = 0, lt_my = 0, lt_wx = 0, lt_wy = 0, lt_ww = 0, lt_wh = 0;

int neko_os_chdir(const char *path) {
#if (defined NEKO_IS_WIN32)
    return _chdir(path);
#elif (defined NEKO_IS_LINUX || defined NEKO_IS_APPLE || defined NEKO_IS_ANDROID)
    return chdir(path);
#endif
    return 1;
}

#if (defined NEKO_IS_WIN32)
void os_sleep(uint32_t ms) { Sleep(ms); }
#elif (defined NEKO_IS_LINUX || defined NEKO_IS_APPLE || defined NEKO_IS_ANDROID)
void os_sleep(uint32_t ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, &ts);
}
#endif

#ifdef NEKO_IS_WIN32
#include <windows.h>
#elif NEKO_IS_APPLE
#include <mach-o/dyld.h>
#elif NEKO_IS_LINUX
#include <limits.h>
#include <unistd.h>
#endif

std::string os_program_dir() {
    char path[1024];
    std::string dir;

#ifdef NEKO_IS_WIN32
    GetModuleFileNameA(NULL, path, sizeof(path));
#elif NEKO_IS_APPLE
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        // success
    } else {
        // buffer was too small; handle error
        return "";
    }
#elif NEKO_IS_LINUX
    ssize_t count = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (count != -1) {
        path[count] = '\0';
    } else {
        // error occurred
        return "";
    }
#endif

    dir = std::string(path);
    // Find the last directory separator '/' or '\'
    size_t pos = dir.find_last_of("/\\");
    if (pos != std::string::npos) {
        dir = dir.substr(0, pos);  // Keep only the directory part
    }

    return dir;
}

const char *window_clipboard() { return glfwGetClipboardString(glfw_window); }

void window_setclipboard(const char *text) { glfwSetClipboardString(glfw_window, text); }

void window_focus() { glfwFocusWindow(glfw_window); }

int window_has_focus() { return !!glfwGetWindowAttrib(glfw_window, GLFW_FOCUSED); }

char *tmp_fmt(const char *fmt, ...) {
    static char s_buf[1024] = {};

    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(s_buf, sizeof(s_buf), fmt, args);
    va_end(args);
    return s_buf;
}

std::string read_file_to_string(const std::string &filename) {
    std::ifstream file(filename, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }
    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    if (size <= 0) {
        throw std::runtime_error("File is empty or an error occurred while reading: " + filename);
    }
    file.seekg(0, std::ios::beg);
    std::string contents(size, '\0');
    if (!file.read(&contents[0], size)) {
        throw std::runtime_error("Error occurred while reading file: " + filename);
    }
    return contents;
}

char *file_pathabs(const char *pathfile) {
    char *out = tmp_fmt("%*.s", DIR_MAX + 1, "");
#ifdef NEKO_IS_WIN32
    _fullpath(out, pathfile, DIR_MAX);
#else
    realpath(pathfile, out);
#endif
    return out;
}

uint64_t lt_time_now() {
    auto now = std::chrono::steady_clock::now();
    return duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

void destroy_texture(LT_Texture *tex) {
    GLuint id = (GLuint)tex->id;
    glDeleteTextures(1, &id);
}

bool texture_update_data(LT_Texture *tex, uint8_t *data) {

    // LockGuard lock{&g_app->gpu_mtx};

    // 如果存在 则释放旧的 GL 纹理
    if (tex->id != 0) glDeleteTextures(1, &tex->id);

    // 生成 GL 纹理
    glGenTextures(1, &tex->id);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex->id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // 将纹理数据复制到 GL
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, tex->width, tex->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

lt_surface *lt_getsurface(void *window) {
    static lt_surface s = {0};
    return &s;
}

void lt_updatesurfacerects(lt_surface *s, lt_rect *rects, unsigned count) {
    if (0)
        for (int i = 0; i < count; ++i) {
            memset((unsigned *)s->pixels + (rects[i].x + rects[i].y * s->w), 0xFF, rects[i].width * 4);
            memset((unsigned *)s->pixels + (rects[i].x + (rects[i].y + (rects[i].height - 1)) * s->w), 0xFF, rects[i].width * 4);
            for (int y = 1; y < (rects[i].height - 1); ++y) {
                ((unsigned *)s->pixels)[rects[i].x + y * s->w] = ((unsigned *)s->pixels)[rects[i].x + (rects[i].width - 1) + y * s->w] = 0xFFFFFFFF;
            }
        }

    // update contents
    // texture_update(&s->t, s->w, s->h, 4, s->pixels, TEXTURE_LINEAR | TEXTURE_BGRA); // TODO
    s->t.width = s->w;
    s->t.height = s->h;
    texture_update_data(&s->t, (uint8_t *)s->pixels);
}

void ren_set_clip_rect(struct lt_rect rect);
void rencache_invalidate(void);
int lt_resizesurface(lt_surface *s, int ww, int wh) {
    s->w = ww, s->h = wh;
    // TODO
    if (s->w * s->h <= 0) {
        s->w = 0;
        s->h = 0;
    }
    if (s->t.id == 0 || s->w != s->t.width || s->h != s->t.height) {
        // invalidate tiles
        ren_set_clip_rect(lt_rect{0, 0, s->w, s->h});
        rencache_invalidate();

        // texture clear
        // if (!s->t.id) s->t = texture_create(1, 1, 4, "    ", TEXTURE_LINEAR | TEXTURE_RGBA | TEXTURE_BYTE);
        if (!s->t.id) {
            LT_Texture t = {.width = 1, .height = 1};
            bool ok = texture_update_data(&t, (uint8_t *)"    ");
            s->t = t;
        }
        s->pixels = lt_realloc(s->pixels, s->w * s->h * 4);
        memset(s->pixels, 0, s->w * s->h * 4);

        // texture update
        lt_updatesurfacerects(s, 0, 0);
        return 1;  // resized
    }
    return 0;  // unchanged
}

void *lt_load_file(const char *filename, int *size) {
    static std::string contents = read_file_to_string(filename);
    if (size) *size = 0;
    if (contents.empty()) {
        return NULL;
    }
    if (size) *size = contents.length();
    return (void *)contents.c_str();
}

const char *lt_button_name(int button) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) return "left";
    if (button == GLFW_MOUSE_BUTTON_RIGHT) return "right";
    if (button == GLFW_MOUSE_BUTTON_MIDDLE) return "middle";
    return "?";
}

char *lt_key_name(char *dst, int key, int vk, int mods) {

    if (key == GLFW_KEY_UP) return "up";
    if (key == GLFW_KEY_DOWN) return "down";
    if (key == GLFW_KEY_LEFT) return "left";
    if (key == GLFW_KEY_RIGHT) return "right";
    if (key == GLFW_KEY_LEFT_ALT) return "left alt";
    if (key == GLFW_KEY_RIGHT_ALT) return "right alt";
    if (key == GLFW_KEY_LEFT_SHIFT) return "left shift";
    if (key == GLFW_KEY_RIGHT_SHIFT) return "right shift";
    if (key == GLFW_KEY_LEFT_CONTROL) return "left ctrl";
    if (key == GLFW_KEY_RIGHT_CONTROL) return "right ctrl";
    if (key == GLFW_KEY_LEFT_SUPER) return "left windows";
    if (key == GLFW_KEY_RIGHT_SUPER) return "left windows";
    if (key == GLFW_KEY_MENU) return "menu";

    if (key == GLFW_KEY_ESCAPE) return "escape";
    if (key == GLFW_KEY_BACKSPACE) return "backspace";
    if (key == GLFW_KEY_ENTER) return "return";
    if (key == GLFW_KEY_KP_ENTER) return "keypad enter";
    if (key == GLFW_KEY_TAB) return "tab";
    if (key == GLFW_KEY_CAPS_LOCK) return "capslock";

    if (key == GLFW_KEY_HOME) return "home";
    if (key == GLFW_KEY_END) return "end";
    if (key == GLFW_KEY_INSERT) return "insert";
    if (key == GLFW_KEY_DELETE) return "delete";
    if (key == GLFW_KEY_PAGE_UP) return "pageup";
    if (key == GLFW_KEY_PAGE_DOWN) return "pagedown";

    if (key == GLFW_KEY_F1) return "f1";
    if (key == GLFW_KEY_F2) return "f2";
    if (key == GLFW_KEY_F3) return "f3";
    if (key == GLFW_KEY_F4) return "f4";
    if (key == GLFW_KEY_F5) return "f5";
    if (key == GLFW_KEY_F6) return "f6";
    if (key == GLFW_KEY_F7) return "f7";
    if (key == GLFW_KEY_F8) return "f8";
    if (key == GLFW_KEY_F9) return "f9";
    if (key == GLFW_KEY_F10) return "f10";
    if (key == GLFW_KEY_F11) return "f11";
    if (key == GLFW_KEY_F12) return "f12";

    const char *name = glfwGetKeyName(key, vk);
    strcpy(dst, name ? name : "");
    char *p = dst;
    while (*p) {
        *p = tolower(*p);
        p++;
    }
    return dst;
}

void lt_globpath(struct lua_State *L, const char *path) {
    unsigned j = 0;

    namespace fs = std::filesystem;
    std::string basePath = path;

    if (basePath.back() != '/') {
        basePath += '/';
    }

    // directory iteration using std::filesystem
    for (const auto &entry : fs::directory_iterator(basePath)) {
        if (entry.is_regular_file() || entry.is_directory()) {
            std::string name = entry.path().filename().string();
            lua_pushstring(L, name.c_str());
            lua_rawseti(L, -2, ++j);
        }
    }

    // handling the LT_DATAPATH section
    const std::string _LT_DATAPATH = LT_DATAPATH;  // Define LT_DATAPATH appropriately
    size_t sectionPos = basePath.find(_LT_DATAPATH);
    if (sectionPos != std::string::npos && basePath[sectionPos + _LT_DATAPATH.size()] == '/') {
        std::vector<std::string> list;
        for (const auto &entry : fs::recursive_directory_iterator(".")) {
            if (entry.is_regular_file() || entry.is_directory()) {
                list.push_back(entry.path().string());
            }
        }

        for (const auto &name : list) {
            if (name.find(basePath.substr(sectionPos + 1)) != std::string::npos) {
                lua_pushstring(L, fs::path(name).filename().string().c_str());
                lua_rawseti(L, -2, ++j);
            }
        }

        if (!list.empty()) return;
    }
}

int lt_emit_event(lua_State *L, const char *event_name, const char *event_fmt, ...) {
    int count = 0;
    lua_pushstring(L, event_name);
    if (event_fmt) {
        va_list va;
        va_start(va, event_fmt);
        for (; event_fmt[count]; ++count) {
            if (event_fmt[count] == 'd') {
                int d = va_arg(va, int);
                lua_pushnumber(L, d);
            } else if (event_fmt[count] == 'f') {
                double f = va_arg(va, double);
                lua_pushnumber(L, f);
            } else if (event_fmt[count] == 's') {
                const char *s = va_arg(va, const char *);
                lua_pushstring(L, s);
            }
        }
        va_end(va);
    }
    return 1 + count;
}

int printi(int i) {
    // printf("clicks: %d\n", i);
    return i;
}

INPUT_WRAP_DEFINE(lt);

static const char *codepoint_to_utf8_(unsigned c);
int lt_poll_event(lua_State *L) {  // init.lua > core.step() wakes on mousemoved || inputtext
    int rc = 0;
    char buf[16];
    static int prevx = 0, prevy = 0;

    static unsigned clicks_time = 0, clicks = 0;
    if ((lt_time_ms() - clicks_time) > 400) clicks = 0;

    for (INPUT_WRAP_event e; input_wrap_next_e(&lt_input_queue, &e); input_wrap_free_e(&e))
        if (e.type) switch (e.type) {
                default:
                    break;
                case INPUT_WRAP_WINDOW_CLOSED:  // it used to be ok. depends on window_swap() flow
                    rc += lt_emit_event(L, "quit", NULL);
                    return rc;

                    break;
                case INPUT_WRAP_WINDOW_MOVED:
                    lt_wx = e.pos.x;
                    lt_wy = e.pos.y;

                    break;
                case INPUT_WRAP_WINDOW_RESIZED:
                    rc += lt_emit_event(L, "resized", "dd", lt_ww = e.size.width, lt_wh = e.size.height);
                    lt_resizesurface(lt_getsurface(lt_window()), lt_ww, lt_wh);

                    break;
                case INPUT_WRAP_WINDOW_REFRESH:
                    rc += lt_emit_event(L, "exposed", NULL);
                    rencache_invalidate();

                    break;
                // case INPUT_WRAP_FILE_DROPPED:
                //     rc += lt_emit_event(L, "filedropped", "sdd", e.file.paths[0], lt_mx, lt_my);
                //     break;
                case INPUT_WRAP_KEY_PRESSED:
                case INPUT_WRAP_KEY_REPEATED:
                    rc += lt_emit_event(L, "keypressed", "s", lt_key_name(buf, e.keyboard.key, e.keyboard.scancode, e.keyboard.mods));
                    goto bottom;

                    break;
                case INPUT_WRAP_KEY_RELEASED:
                    rc += lt_emit_event(L, "keyreleased", "s", lt_key_name(buf, e.keyboard.key, e.keyboard.scancode, e.keyboard.mods));
                    goto bottom;

                    break;
                case INPUT_WRAP_CODEPOINT_INPUT:
                    rc += lt_emit_event(L, "textinput", "s", codepoint_to_utf8_(e.codepoint));

                    break;
                case INPUT_WRAP_BUTTON_PRESSED:
                    rc += lt_emit_event(L, "mousepressed", "sddd", lt_button_name(e.mouse.button), lt_mx, lt_my, printi(1 + clicks));

                    break;
                case INPUT_WRAP_BUTTON_RELEASED:
                    clicks += e.mouse.button == GLFW_MOUSE_BUTTON_1;
                    clicks_time = lt_time_ms();
                    rc += lt_emit_event(L, "mousereleased", "sdd", lt_button_name(e.mouse.button), lt_mx, lt_my);

                    break;
                case INPUT_WRAP_CURSOR_MOVED:
                    lt_mx = e.pos.x - lt_wx, lt_my = e.pos.y - lt_wy;
                    rc += lt_emit_event(L, "mousemoved", "dddd", lt_mx, lt_my, lt_mx - prevx, lt_my - prevy);
                    prevx = lt_mx, prevy = lt_my;

                    break;
                case INPUT_WRAP_SCROLLED:
                    rc += lt_emit_event(L, "mousewheel", "f", e.scroll.y);
            }

bottom:;

    return rc;
}

// ----------------------------------------------------------------------------
// lite/renderer.c

#define MAX_GLYPHSET 256

struct RenImage {
    RenColor *pixels;
    int width, height;
};

typedef struct {
    RenImage *image;
    stbtt_bakedchar glyphs[256];
} GlyphSet;

struct RenFont {
    void *data;
    stbtt_fontinfo stbfont;
    GlyphSet *sets[MAX_GLYPHSET];
    float size;
    int height;
};

static struct {
    int left, top, right, bottom;
} lt_clip;

static const char *codepoint_to_utf8_(unsigned c) {
    static char s[4 + 1];
    lt_memset(s, 0, 5);
    if (c < 0x80)
        s[0] = c, s[1] = 0;
    else if (c < 0x800)
        s[0] = 0xC0 | ((c >> 6) & 0x1F), s[1] = 0x80 | (c & 0x3F), s[2] = 0;
    else if (c < 0x10000)
        s[0] = 0xE0 | ((c >> 12) & 0x0F), s[1] = 0x80 | ((c >> 6) & 0x3F), s[2] = 0x80 | (c & 0x3F), s[3] = 0;
    else if (c < 0x110000)
        s[0] = 0xF0 | ((c >> 18) & 0x07), s[1] = 0x80 | ((c >> 12) & 0x3F), s[2] = 0x80 | ((c >> 6) & 0x3F), s[3] = 0x80 | (c & 0x3F), s[4] = 0;
    return s;
}
static const char *utf8_to_codepoint_(const char *p, unsigned *dst) {
    unsigned res, n;
    switch (*p & 0xf0) {
        case 0xf0:
            res = *p & 0x07;
            n = 3;
            break;
        case 0xe0:
            res = *p & 0x0f;
            n = 2;
            break;
        case 0xd0:
        case 0xc0:
            res = *p & 0x1f;
            n = 1;
            break;
        default:
            res = *p;
            n = 0;
            break;
    }
    while (n-- && *p++) {                //< https://github.com/rxi/lite/issues/262
        res = (res << 6) | (*p & 0x3f);  //< https://github.com/rxi/lite/issues/262
    }
    *dst = res;
    return p + 1;
}

void ren_init(void *win) {
    lt_surface *surf = lt_getsurface(lt_window());
    ren_set_clip_rect(RenRect{0, 0, surf->w, surf->h});
}

void ren_update_rects(RenRect *rects, int count) { lt_updatesurfacerects(lt_getsurface(lt_window()), (lt_rect *)rects, count); }

void ren_set_clip_rect(RenRect rect) {
    lt_clip.left = rect.x;
    lt_clip.top = rect.y;
    lt_clip.right = rect.x + rect.width;
    lt_clip.bottom = rect.y + rect.height;
}

void ren_get_size(int *x, int *y) {
    lt_surface *surf = lt_getsurface(lt_window());
    *x = surf->w;
    *y = surf->h;
}

RenImage *ren_new_image(int width, int height) {
    lt_assert(width > 0 && height > 0);
    RenImage *image = (RenImage *)lt_malloc(sizeof(RenImage) + width * height * sizeof(RenColor));
    image->pixels = (RenColor *)(image + 1);
    image->width = width;
    image->height = height;
    return image;
}

void ren_free_image(RenImage *image) { lt_free(image); }

static GlyphSet *load_glyphset(RenFont *font, int idx) {
    GlyphSet *set = (GlyphSet *)lt_calloc(1, sizeof(GlyphSet));

    // init image
    int width = 128;
    int height = 128;
retry:
    set->image = ren_new_image(width, height);

    // load glyphs
    float s = stbtt_ScaleForMappingEmToPixels(&font->stbfont, 1) / stbtt_ScaleForPixelHeight(&font->stbfont, 1);
    int res = stbtt_BakeFontBitmap((unsigned char *)font->data, 0, font->size * s, (unsigned char *)set->image->pixels, width, height, idx * 256, 256, set->glyphs);

    // retry with a larger image buffer if the buffer wasn't large enough
    if (res < 0) {
        width *= 2;
        height *= 2;
        ren_free_image(set->image);
        goto retry;
    }

    // adjust glyph yoffsets and xadvance
    int ascent, descent, linegap;
    stbtt_GetFontVMetrics(&font->stbfont, &ascent, &descent, &linegap);
    float scale = stbtt_ScaleForMappingEmToPixels(&font->stbfont, font->size);
    int scaled_ascent = ascent * scale + 0.5;
    for (int i = 0; i < 256; i++) {
        set->glyphs[i].yoff += scaled_ascent;
        set->glyphs[i].xadvance = floor(set->glyphs[i].xadvance);
    }

    // convert 8bit data to 32bit
    for (int i = width * height - 1; i >= 0; i--) {
        uint8_t n = *((uint8_t *)set->image->pixels + i);
        set->image->pixels[i] = RenColor{.b = 255, .g = 255, .r = 255, .a = n};
    }

    return set;
}

static GlyphSet *get_glyphset(RenFont *font, int codepoint) {
    int idx = (codepoint >> 8) % MAX_GLYPHSET;
    if (!font->sets[idx]) {
        font->sets[idx] = load_glyphset(font, idx);
    }
    return font->sets[idx];
}

RenFont *ren_load_font(const char *filename, float size) {
    // load font into buffer
    char *fontdata = (char *)lt_load_file(filename, NULL);
    if (!fontdata) return NULL;

    RenFont *font = NULL;

    // init font
    font = (RenFont *)lt_calloc(1, sizeof(RenFont));
    font->size = size;
    font->data = fontdata;

    // init stbfont
    int ok = stbtt_InitFont(&font->stbfont, (unsigned char *)font->data, 0);
    if (!ok) {
        if (font) {
            lt_free(font->data);
        }
        lt_free(font);
        return NULL;
    }

    // get height and scale
    int ascent, descent, linegap;
    stbtt_GetFontVMetrics(&font->stbfont, &ascent, &descent, &linegap);
    float scale = stbtt_ScaleForMappingEmToPixels(&font->stbfont, size);
    font->height = (ascent - descent + linegap) * scale + 0.5;

    // make tab and newline glyphs invisible
    stbtt_bakedchar *g = get_glyphset(font, '\n')->glyphs;
    g['\t'].x1 = g['\t'].x0;
    g['\n'].x1 = g['\n'].x0;

    return font;
}

void ren_free_font(RenFont *font) {
    for (int i = 0; i < MAX_GLYPHSET; i++) {
        GlyphSet *set = font->sets[i];
        if (set) {
            ren_free_image(set->image);
            lt_free(set);
        }
    }
    lt_free(font->data);
    lt_free(font);
}

void ren_set_font_tab_width(RenFont *font, int n) {
    GlyphSet *set = get_glyphset(font, '\t');
    set->glyphs['\t'].xadvance = n;
}

int ren_get_font_tab_width(RenFont *font) {
    GlyphSet *set = get_glyphset(font, '\t');
    return set->glyphs['\t'].xadvance;
}

int ren_get_font_width(RenFont *font, const char *text) {
    int x = 0;
    const char *p = text;
    unsigned codepoint;
    while (*p) {
        p = utf8_to_codepoint_(p, &codepoint);
        GlyphSet *set = get_glyphset(font, codepoint);
        stbtt_bakedchar *g = &set->glyphs[codepoint & 0xff];
        x += g->xadvance;
    }
    return x;
}

int ren_get_font_height(RenFont *font) { return font->height; }

static inline RenColor blend_pixel(RenColor dst, RenColor src) {
    int ia = 0xff - src.a;
    dst.r = ((src.r * src.a) + (dst.r * ia)) >> 8;
    dst.g = ((src.g * src.a) + (dst.g * ia)) >> 8;
    dst.b = ((src.b * src.a) + (dst.b * ia)) >> 8;
    return dst;
}

static inline RenColor blend_pixel2(RenColor dst, RenColor src, RenColor color) {
    src.a = (src.a * color.a) >> 8;
    int ia = 0xff - src.a;
    dst.r = ((src.r * color.r * src.a) >> 16) + ((dst.r * ia) >> 8);
    dst.g = ((src.g * color.g * src.a) >> 16) + ((dst.g * ia) >> 8);
    dst.b = ((src.b * color.b * src.a) >> 16) + ((dst.b * ia) >> 8);
    return dst;
}

#define rect_draw_loop(expr)            \
    for (int j = y1; j < y2; j++) {     \
        for (int i = x1; i < x2; i++) { \
            *d = expr;                  \
            d++;                        \
        }                               \
        d += dr;                        \
    }

void ren_draw_rect(RenRect rect, RenColor color) {
    if (color.a == 0) {
        return;
    }

    int x1 = rect.x < lt_clip.left ? lt_clip.left : rect.x;
    int y1 = rect.y < lt_clip.top ? lt_clip.top : rect.y;
    int x2 = rect.x + rect.width;
    int y2 = rect.y + rect.height;
    x2 = x2 > lt_clip.right ? lt_clip.right : x2;
    y2 = y2 > lt_clip.bottom ? lt_clip.bottom : y2;

    lt_surface *surf = lt_getsurface(lt_window());
    RenColor *d = (RenColor *)surf->pixels;
    d += x1 + y1 * surf->w;
    int dr = surf->w - (x2 - x1);

    if (color.a == 0xff) {
        rect_draw_loop(color);
    } else {
        rect_draw_loop(blend_pixel(*d, color));
    }
}

void ren_draw_image(RenImage *image, RenRect *sub, int x, int y, RenColor color) {
    if (color.a == 0) {
        return;
    }

    // clip
    int n;
    if ((n = lt_clip.left - x) > 0) {
        sub->width -= n;
        sub->x += n;
        x += n;
    }
    if ((n = lt_clip.top - y) > 0) {
        sub->height -= n;
        sub->y += n;
        y += n;
    }
    if ((n = x + sub->width - lt_clip.right) > 0) {
        sub->width -= n;
    }
    if ((n = y + sub->height - lt_clip.bottom) > 0) {
        sub->height -= n;
    }

    if (sub->width <= 0 || sub->height <= 0) {
        return;
    }

    // draw
    lt_surface *surf = lt_getsurface(lt_window());
    RenColor *s = image->pixels;
    RenColor *d = (RenColor *)surf->pixels;
    s += sub->x + sub->y * image->width;
    d += x + y * surf->w;
    int sr = image->width - sub->width;
    int dr = surf->w - sub->width;

    for (int j = 0; j < sub->height; j++) {
        for (int i = 0; i < sub->width; i++) {
            *d = blend_pixel2(*d, *s, color);
            d++;
            s++;
        }
        d += dr;
        s += sr;
    }
}

int ren_draw_text(RenFont *font, const char *text, int x, int y, RenColor color) {
    RenRect rect;
    const char *p = text;
    unsigned codepoint;
    while (*p) {
        p = utf8_to_codepoint_(p, &codepoint);
        GlyphSet *set = get_glyphset(font, codepoint);
        stbtt_bakedchar *g = &set->glyphs[codepoint & 0xff];
        rect.x = g->x0;
        rect.y = g->y0;
        rect.width = g->x1 - g->x0;
        rect.height = g->y1 - g->y0;
        ren_draw_image(set->image, &rect, x + g->xoff, y + g->yoff, color);
        x += g->xadvance;
    }
    return x;
}

// ----------------------------------------------------------------------------
// lite/renderer_font.c

static int f_load(lua_State *L) {
    const char *filename = luaL_checkstring(L, 1);
    float size = luaL_checknumber(L, 2);
    RenFont **self = (RenFont **)lua_newuserdata(L, sizeof(*self));
    luaL_setmetatable(L, API_TYPE_FONT);
    *self = ren_load_font(filename, size);
    if (!*self) {
        luaL_error(L, "failed to load font");
    }
    return 1;
}

static int f_set_tab_width(lua_State *L) {
    RenFont **self = (RenFont **)luaL_checkudata(L, 1, API_TYPE_FONT);
    int n = luaL_checknumber(L, 2);
    ren_set_font_tab_width(*self, n);
    return 0;
}

static int f_GC(lua_State *L) {
    RenFont **self = (RenFont **)luaL_checkudata(L, 1, API_TYPE_FONT);
    if (*self) {
        rencache_free_font(*self);
    }
    return 0;
}

static int f_get_width(lua_State *L) {
    RenFont **self = (RenFont **)luaL_checkudata(L, 1, API_TYPE_FONT);
    const char *text = luaL_checkstring(L, 2);
    lua_pushnumber(L, ren_get_font_width(*self, text));
    return 1;
}

static int f_get_height(lua_State *L) {
    RenFont **self = (RenFont **)luaL_checkudata(L, 1, API_TYPE_FONT);
    lua_pushnumber(L, ren_get_font_height(*self));
    return 1;
}

int luaopen_renderer_font(lua_State *L) {
    static const luaL_Reg lib[] = {{"__gc", f_GC}, {"load", f_load}, {"set_tab_width", f_set_tab_width}, {"get_width", f_get_width}, {"get_height", f_get_height}, {NULL, NULL}};
    luaL_newmetatable(L, API_TYPE_FONT);
    luaL_setfuncs(L, lib, 0);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    return 1;
}

// ----------------------------------------------------------------------------
// lite/renderer_api.c

static RenColor checkcolor(lua_State *L, int idx, int def) {
    RenColor color;
    if (lua_isnoneornil(L, idx)) {
        return RenColor{(uint8_t)def, (uint8_t)def, (uint8_t)def, 255};
    }
    lua_rawgeti(L, idx, 1);
    lua_rawgeti(L, idx, 2);
    lua_rawgeti(L, idx, 3);
    lua_rawgeti(L, idx, 4);
    color.r = luaL_checknumber(L, -4);
    color.g = luaL_checknumber(L, -3);
    color.b = luaL_checknumber(L, -2);
    color.a = luaL_optnumber(L, -1, 255);
    lua_pop(L, 4);
    return color;
}

static int f_show_debug(lua_State *L) {
    luaL_checkany(L, 1);
    rencache_show_debug(lua_toboolean(L, 1));
    return 0;
}

static int f_get_size(lua_State *L) {
    int w, h;
    ren_get_size(&w, &h);
    lua_pushnumber(L, w);
    lua_pushnumber(L, h);
    return 2;
}

static int f_begin_frame(lua_State *L) {
    rencache_begin_frame();
    return 0;
}

static int f_end_frame(lua_State *L) {
    rencache_end_frame();
    return 0;
}

static int f_set_clip_rect(lua_State *L) {
    RenRect rect;
    rect.x = luaL_checknumber(L, 1);
    rect.y = luaL_checknumber(L, 2);
    rect.width = luaL_checknumber(L, 3);
    rect.height = luaL_checknumber(L, 4);
    rencache_set_clip_rect(rect);
    return 0;
}

static int f_draw_rect(lua_State *L) {
    RenRect rect;
    rect.x = luaL_checknumber(L, 1);
    rect.y = luaL_checknumber(L, 2);
    rect.width = luaL_checknumber(L, 3);
    rect.height = luaL_checknumber(L, 4);
    RenColor color = checkcolor(L, 5, 255);
    rencache_draw_rect(rect, color);
    return 0;
}

static int f_draw_text(lua_State *L) {
    RenFont **font = (RenFont **)luaL_checkudata(L, 1, API_TYPE_FONT);
    const char *text = luaL_checkstring(L, 2);
    int x = luaL_checknumber(L, 3);
    int y = luaL_checknumber(L, 4);
    RenColor color = checkcolor(L, 5, 255);
    x = rencache_draw_text(*font, text, x, y, color);
    lua_pushnumber(L, x);
    return 1;
}

int luaopen_renderer(lua_State *L) {
    static const luaL_Reg lib[] = {{"show_debug", f_show_debug},       {"get_size", f_get_size},   {"begin_frame", f_begin_frame}, {"end_frame", f_end_frame},
                                   {"set_clip_rect", f_set_clip_rect}, {"draw_rect", f_draw_rect}, {"draw_text", f_draw_text},     {NULL, NULL}};
    luaL_newlib(L, lib);
    luaopen_renderer_font(L);
    lua_setfield(L, -2, "font");
    return 1;
}

// ----------------------------------------------------------------------------
// lite/rencache.c

/* a cache over the software renderer -- all drawing operations are stored as
** commands when issued. At the end of the frame we write the commands to a grid
** of hash values, take the cells that have changed since the previous frame,
** merge them into dirty rectangles and redraw only those regions */

#define CELLS_X 80
#define CELLS_Y 50
#define CELL_SIZE 96
#define COMMAND_BUF_SIZE (1024 * 512)

enum { FREE_FONT, SET_CLIP, DRAW_TEXT, DRAW_RECT };

typedef struct {
    int type, size;
    RenRect rect;
    RenColor color;
    RenFont *font;
    int tab_width;
    char text[0];
} Command;

static unsigned cells_buf1[CELLS_X * CELLS_Y];
static unsigned cells_buf2[CELLS_X * CELLS_Y];
static unsigned *cells_prev = cells_buf1;
static unsigned *cells = cells_buf2;
static RenRect rect_buf[CELLS_X * CELLS_Y / 2];
static char command_buf[COMMAND_BUF_SIZE];
static int command_buf_idx;
static RenRect screen_rect;
static bool show_debug;

// 32bit fnv-1a hash
#define HASH_INITIAL 2166136261

static void hash(unsigned *h, const void *data, int size) {
    const unsigned char *p = (unsigned char *)data;
    while (size--) {
        *h = (*h ^ *p++) * 16777619;
    }
}

static inline int cell_idx(int x, int y) { return x + y * CELLS_X; }

static inline bool rects_overlap(RenRect a, RenRect b) { return b.x + b.width >= a.x && b.x <= a.x + a.width && b.y + b.height >= a.y && b.y <= a.y + a.height; }

static RenRect intersect_rects(RenRect a, RenRect b) {
    int x1 = NEKO_MAX(a.x, b.x);
    int y1 = NEKO_MAX(a.y, b.y);
    int x2 = NEKO_MIN(a.x + a.width, b.x + b.width);
    int y2 = NEKO_MIN(a.y + a.height, b.y + b.height);
    return RenRect{x1, y1, NEKO_MAX(0, x2 - x1), NEKO_MAX(0, y2 - y1)};
}

static RenRect merge_rects(RenRect a, RenRect b) {
    int x1 = NEKO_MIN(a.x, b.x);
    int y1 = NEKO_MIN(a.y, b.y);
    int x2 = NEKO_MAX(a.x + a.width, b.x + b.width);
    int y2 = NEKO_MAX(a.y + a.height, b.y + b.height);
    return RenRect{x1, y1, x2 - x1, y2 - y1};
}

static Command *push_command(int type, int size) {
    size_t alignment = 7;                    // alignof(max_align_t) - 1; //< C11 https://github.com/rxi/lite/pull/292/commits/ad1bdf56e3f212446e1c61fd45de8b94de5e2bc3
    size = (size + alignment) & ~alignment;  //< https://github.com/rxi/lite/pull/292/commits/ad1bdf56e3f212446e1c61fd45de8b94de5e2bc3
    Command *cmd = (Command *)(command_buf + command_buf_idx);
    int n = command_buf_idx + size;
    if (n > COMMAND_BUF_SIZE) {
        fprintf(stderr, "Warning: (" __FILE__ "): exhausted command buffer\n");
        return NULL;
    }
    command_buf_idx = n;
    lt_memset(cmd, 0, sizeof(Command));
    cmd->type = type;
    cmd->size = size;
    return cmd;
}

static bool next_command(Command **prev) {
    if (*prev == NULL) {
        *prev = (Command *)command_buf;
    } else {
        *prev = (Command *)(((char *)*prev) + (*prev)->size);
    }
    return *prev != ((Command *)(command_buf + command_buf_idx));
}

void rencache_show_debug(bool enable) { show_debug = enable; }

void rencache_free_font(RenFont *font) {
    Command *cmd = push_command(FREE_FONT, sizeof(Command));
    if (cmd) {
        cmd->font = font;
    }
}

void rencache_set_clip_rect(RenRect rect) {
    Command *cmd = push_command(SET_CLIP, sizeof(Command));
    if (cmd) {
        cmd->rect = intersect_rects(rect, screen_rect);
    }
}

void rencache_draw_rect(RenRect rect, RenColor color) {
    if (!rects_overlap(screen_rect, rect)) {
        return;
    }
    Command *cmd = push_command(DRAW_RECT, sizeof(Command));
    if (cmd) {
        cmd->rect = rect;
        cmd->color = color;
    }
}

int rencache_draw_text(RenFont *font, const char *text, int x, int y, RenColor color) {
    RenRect rect;
    rect.x = x;
    rect.y = y;
    rect.width = ren_get_font_width(font, text);
    rect.height = ren_get_font_height(font);

    if (rects_overlap(screen_rect, rect)) {
        int sz = strlen(text) + 1;
        Command *cmd = push_command(DRAW_TEXT, sizeof(Command) + sz);
        if (cmd) {
            memcpy(cmd->text, text, sz);
            cmd->color = color;
            cmd->font = font;
            cmd->rect = rect;
            cmd->tab_width = ren_get_font_tab_width(font);
        }
    }

    return x + rect.width;
}

void rencache_invalidate(void) { lt_memset(cells_prev, 0xff, sizeof(cells_buf1)); }

void rencache_begin_frame(void) {
    // reset all cells if the screen width/height has changed
    int w, h;
    ren_get_size(&w, &h);
    if (screen_rect.width != w || h != screen_rect.height) {
        screen_rect.width = w;
        screen_rect.height = h;
        rencache_invalidate();
    }
}

static void update_overlapping_cells(RenRect r, unsigned h) {
    int x1 = r.x / CELL_SIZE;
    int y1 = r.y / CELL_SIZE;
    int x2 = (r.x + r.width) / CELL_SIZE;
    int y2 = (r.y + r.height) / CELL_SIZE;

    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            int idx = cell_idx(x, y);
            hash(&cells[idx], &h, sizeof(h));
        }
    }
}

static void push_rect(RenRect r, int *count) {
    // try to merge with existing rectangle
    for (int i = *count - 1; i >= 0; i--) {
        RenRect *rp = &rect_buf[i];
        if (rects_overlap(*rp, r)) {
            *rp = merge_rects(*rp, r);
            return;
        }
    }
    // couldn't merge with previous rectangle: push
    rect_buf[(*count)++] = r;
}

void rencache_end_frame(void) {
    // update cells from commands
    Command *cmd = NULL;
    RenRect cr = screen_rect;
    while (next_command(&cmd)) {
        if (cmd->type == SET_CLIP) {
            cr = cmd->rect;
        }
        RenRect r = intersect_rects(cmd->rect, cr);
        if (r.width == 0 || r.height == 0) {
            continue;
        }
        unsigned h = HASH_INITIAL;
        hash(&h, cmd, cmd->size);
        update_overlapping_cells(r, h);
    }

    // push rects for all cells changed from last frame, reset cells
    int rect_count = 0;
    int max_x = screen_rect.width / CELL_SIZE + 1;
    int max_y = screen_rect.height / CELL_SIZE + 1;
    for (int y = 0; y < max_y; y++) {
        for (int x = 0; x < max_x; x++) {
            // compare previous and current cell for change
            int idx = cell_idx(x, y);
            if (cells[idx] != cells_prev[idx]) {
                push_rect(RenRect{x, y, 1, 1}, &rect_count);
            }
            cells_prev[idx] = HASH_INITIAL;
        }
    }

    // expand rects from cells to pixels
    for (int i = 0; i < rect_count; i++) {
        RenRect *r = &rect_buf[i];
        r->x *= CELL_SIZE;
        r->y *= CELL_SIZE;
        r->width *= CELL_SIZE;
        r->height *= CELL_SIZE;
        *r = intersect_rects(*r, screen_rect);
    }

    // redraw updated regions
    bool has_free_commands = false;
    for (int i = 0; i < rect_count; i++) {
        // draw
        RenRect r = rect_buf[i];
        ren_set_clip_rect(r);

        cmd = NULL;
        while (next_command(&cmd)) {
            switch (cmd->type) {
                case FREE_FONT:
                    has_free_commands = true;
                    break;
                case SET_CLIP:
                    ren_set_clip_rect(intersect_rects(cmd->rect, r));
                    break;
                case DRAW_RECT:
                    ren_draw_rect(cmd->rect, cmd->color);
                    break;
                case DRAW_TEXT:
                    ren_set_font_tab_width(cmd->font, cmd->tab_width);
                    ren_draw_text(cmd->font, cmd->text, cmd->rect.x, cmd->rect.y, cmd->color);
                    break;
            }
        }

        if (show_debug) {
            RenColor color = {(uint8_t)rand(), (uint8_t)rand(), (uint8_t)rand(), 50};
            ren_draw_rect(r, color);
        }
    }

    // update dirty rects
    if (rect_count > 0) {
        ren_update_rects(rect_buf, rect_count);
    }

    // free fonts
    if (has_free_commands) {
        cmd = NULL;
        while (next_command(&cmd)) {
            if (cmd->type == FREE_FONT) {
                ren_free_font(cmd->font);
            }
        }
    }

    // swap cell buffer and reset
    unsigned *tmp = cells;
    cells = cells_prev;
    cells_prev = tmp;
    command_buf_idx = 0;
}

// ----------------------------------------------------------------------------
// lite/system.c

static int f_set_cursor(lua_State *L) {
    static const char *cursor_opts[] = {"arrow", "ibeam", "sizeh", "sizev", "hand", NULL};
    int n = luaL_checkoption(L, 1, "arrow", cursor_opts);
    lt_setcursor(n);
    return 0;
}

static int f_set_window_title(lua_State *L) {
    const char *title = luaL_checkstring(L, 1);
    lt_setwindowtitle(title);
    return 0;
}
static int f_set_window_mode(lua_State *L) {
    static const char *window_opts[] = {"normal", "maximized", "fullscreen", 0};
    enum { WIN_NORMAL, WIN_MAXIMIZED, WIN_FULLSCREEN };
    int n = luaL_checkoption(L, 1, "normal", window_opts);
    lt_setwindowmode(n);
    return 0;
}
static int f_window_has_focus(lua_State *L) {
    unsigned flags = lt_haswindowfocus();
    lua_pushboolean(L, flags);
    return 1;
}

static int f_show_confirm_dialog(lua_State *L) {
    const char *title = luaL_checkstring(L, 1);
    const char *msg = luaL_checkstring(L, 2);
    int id = lt_prompt(msg, title);  // 0:no, 1:yes
    lua_pushboolean(L, !!id);
    return 1;
}

static int f_chdir(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    int err = neko_os_chdir(path);
    if (err) {
        luaL_error(L, "chdir() failed");
    }
    return 0;
}
static int f_list_dir(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    lua_newtable(L);
    lt_globpath(L, path);
    return 1;
}
static int f_absolute_path(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    char *res = lt_realpath(path, NULL);
    if (!res) {
        return 0;
    }
    lua_pushstring(L, res);
    lt_realpath_free(res);
    return 1;
}
static int f_get_file_info(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);

    struct stat s;
    int err = stat(path, &s);
    if (err < 0) {
        lua_pushnil(L);
        lua_pushstring(L, strerror(errno));
        return 2;
    }

    lua_newtable(L);
    lua_pushnumber(L, s.st_mtime);
    lua_setfield(L, -2, "modified");

    lua_pushnumber(L, s.st_size);
    lua_setfield(L, -2, "size");

    if (S_ISREG(s.st_mode)) {
        lua_pushstring(L, "file");
    } else if (S_ISDIR(s.st_mode)) {
        lua_pushstring(L, "dir");
    } else {
        lua_pushnil(L);
    }
    lua_setfield(L, -2, "type");

    return 1;
}

static int f_get_clipboard(lua_State *L) {
    const char *text = lt_getclipboard(lt_window());
    if (!text) {
        return 0;
    }
    lua_pushstring(L, text);
    return 1;
}
static int f_set_clipboard(lua_State *L) {
    const char *text = luaL_checkstring(L, 1);
    lt_setclipboard(lt_window(), text);
    return 0;
}

static int f_get_time(lua_State *L) {
    double ss = lt_time_ms() / 1000.0;
    lua_pushnumber(L, ss);
    return 1;
}
static int f_sleep(lua_State *L) {
    double ss = luaL_checknumber(L, 1);
    lt_sleep_ms(ss * 1000);
    return 0;
}

static int f_exec(lua_State *L) {
    size_t len;
    const char *cmd = luaL_checklstring(L, 1, &len);
    char *buf = (char *)lt_malloc(len + 32);
    if (!buf) {
        luaL_error(L, "buffer allocation failed");
    }
#if _WIN32
    sprintf(buf, "cmd /c \"%s\"", cmd);
    WinExec(buf, SW_HIDE);
#else
    sprintf(buf, "%s &", cmd);
    int res = system(buf);
#endif
    lt_free(buf);
    return 0;
}

static int f_fuzzy_match(lua_State *L) {
    const char *str = luaL_checkstring(L, 1);
    const char *ptn = luaL_checkstring(L, 2);
    int score = 0;
    int run = 0;

    while (*str && *ptn) {
        while (*str == ' ') {
            str++;
        }
        while (*ptn == ' ') {
            ptn++;
        }
        if (tolower(*str) == tolower(*ptn)) {
            score += run * 10 - (*str != *ptn);
            run++;
            ptn++;
        } else {
            score -= 10;
            run = 0;
        }
        str++;
    }
    if (*ptn) {
        return 0;
    }

    lua_pushnumber(L, score - (int)strlen(str));
    return 1;
}

static int f_poll_event(lua_State *L) {  // init.lua > core.step() wakes on mousemoved || inputtext
    int rc = lt_poll_event(L);
    return rc;
}

int luaopen_system(lua_State *L) {
    static const luaL_Reg lib[] = {{"poll_event", f_poll_event},
                                   {"set_cursor", f_set_cursor},
                                   {"set_window_title", f_set_window_title},
                                   {"set_window_mode", f_set_window_mode},
                                   {"window_has_focus", f_window_has_focus},
                                   {"show_confirm_dialog", f_show_confirm_dialog},
                                   {"chdir", f_chdir},
                                   {"list_dir", f_list_dir},
                                   {"absolute_path", f_absolute_path},
                                   {"get_file_info", f_get_file_info},
                                   {"get_clipboard", f_get_clipboard},
                                   {"set_clipboard", f_set_clipboard},
                                   {"get_time", f_get_time},
                                   {"sleep", f_sleep},
                                   {"exec", f_exec},
                                   {"fuzzy_match", f_fuzzy_match},
                                   {NULL, NULL}};
    luaL_newlib(L, lib);
    return 1;
}

// ----------------------------------------------------------------------------
// lite/api/api.c

void api_load_libs(lua_State *L) {
    static const luaL_Reg libs[] = {{"system", luaopen_system}, {"renderer", luaopen_renderer}, {NULL, NULL}};
    for (int i = 0; libs[i].name; i++) {
        luaL_requiref(L, libs[i].name, libs[i].func, 1);
    }
}

// ----------------------------------------------------------------------------
// lite/main.c

static void _key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
    switch (action) {
        case GLFW_PRESS:
            lt_key_down((key), scancode, mods);
            break;
        case GLFW_RELEASE:
            lt_key_up((key), scancode, mods);
            break;
    }
}

static void _char_callback(GLFWwindow *window, unsigned int c) {
    ImGui_ImplGlfw_CharCallback(window, c);
    lt_char_down(c);
}

static void _mouse_callback(GLFWwindow *window, int mouse, int action, int mods) {
    ImGui_ImplGlfw_MouseButtonCallback(window, mouse, action, mods);
    switch (action) {
        case GLFW_PRESS:
            lt_mouse_down((mouse));
            break;
        case GLFW_RELEASE:
            lt_mouse_up((mouse));
            break;
    }
}

static void _cursor_pos_callback(GLFWwindow *window, double x, double y) {
    ImGui_ImplGlfw_CursorPosCallback(window, x, y);
    lt_mouse_move(ImVec2(x, -y));
}

static void _scroll_callback(GLFWwindow *window, double x, double y) {
    ImGui_ImplGlfw_ScrollCallback(window, x, y);
    lt_scroll(ImVec2(x, y));
}

void lt_init(lua_State *L, void *handle, const char *pathdata, int argc, char **argv, float scale, const char *platform) {
    // setup renderer
    ren_init(handle);

    // setup lua context
    api_load_libs(L);

    lua_newtable(L);
    for (int i = 0; i < argc; i++) {
        lua_pushstring(L, argv[i]);
        lua_rawseti(L, -2, i + 1);
    }
    lua_setglobal(L, "ARGS");

    lua_pushstring(L, "1.11");
    lua_setglobal(L, "VERSION");

    lua_pushstring(L, platform);
    lua_setglobal(L, "PLATFORM");

    lua_pushnumber(L, scale);
    lua_setglobal(L, "SCALE");

    lua_pushstring(L, pathdata);
    lua_setglobal(L, "DATADIR");

    lua_pushstring(L, pathdata);
    lua_setglobal(L, "NEKO_LITE_DIR");  // same as datadir

    lua_pushstring(L, os_program_dir().c_str());
    lua_setglobal(L, "EXEDIR");

    // init lite
    luaL_dostring(L, "core = {}");
    luaL_dostring(L,
                  "xpcall(function()\n"
                  "  SCALE = tonumber(os.getenv(\"LITE_SCALE\")) or SCALE\n"
                  "  PATHSEP = package.config:sub(1, 1)\n"
                  "  USERDIR = NEKO_LITE_DIR .. '/data/user/'\n"
                  "  package.path = NEKO_LITE_DIR .. '/data/?.lua;' .. package.path\n"
                  "  package.path = NEKO_LITE_DIR .. '/data/?/init.lua;' .. package.path\n"
                  "  core = require('core')\n"
                  "  core.init()\n"
                  "end, function(err)\n"
                  "  print('Error: ' .. tostring(err))\n"
                  "  print(debug.traceback(nil, 2))\n"
                  "  if core and core.on_error then\n"
                  "    pcall(core.on_error, err)\n"
                  "  end\n"
                  "  os.exit(1)\n"
                  "end)");

#if 1
    glfwSetKeyCallback(glfw_window, _key_callback);
    glfwSetCharCallback(glfw_window, _char_callback);
    glfwSetMouseButtonCallback(glfw_window, _mouse_callback);
    glfwSetCursorPosCallback(glfw_window, _cursor_pos_callback);
    glfwSetScrollCallback(glfw_window, _scroll_callback);
#endif
}

void lt_tick(struct lua_State *L) {
    luaL_dostring(L,
                  "xpcall(function()\n"
                  "  core.run1()\n"
                  "end, function(err)\n"
                  "  print('Error: ' .. tostring(err))\n"
                  "  print(debug.traceback(nil, 2))\n"
                  "  if core and core.on_error then\n"
                  "    pcall(core.on_error, err)\n"
                  "  end\n"
                  "  os.exit(1)\n"
                  "end)");
}

void lt_fini() {
    auto s = lt_getsurface(lt_window());
    lt_free(s->pixels);
}

INPUT_WRAP_event *input_wrap_new_event(event_queue *equeue) {
    INPUT_WRAP_event *event = equeue->events + equeue->head;
    equeue->head = (equeue->head + 1) % INPUT_WRAP_CAPACITY;
    assert(equeue->head != equeue->tail);
    memset(event, 0, sizeof(INPUT_WRAP_event));
    return event;
}

int input_wrap_next_e(event_queue *equeue, INPUT_WRAP_event *event) {
    memset(event, 0, sizeof(INPUT_WRAP_event));

    if (equeue->head != equeue->tail) {
        *event = equeue->events[equeue->tail];
        equeue->tail = (equeue->tail + 1) % INPUT_WRAP_CAPACITY;
    }

    return event->type != INPUT_WRAP_NONE;
}

void input_wrap_free_e(INPUT_WRAP_event *event) { memset(event, 0, sizeof(INPUT_WRAP_event)); }
