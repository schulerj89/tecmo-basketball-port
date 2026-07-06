#define WIN32_LEAN_AND_MEAN

#include "tecmo_game.h"

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct Win32Backbuffer {
    BITMAPINFO info;
    uint32_t *pixels;
    int width;
    int height;
} Win32Backbuffer;

static Win32Backbuffer g_backbuffer;
static bool g_running;
static TecmoInput g_input;

static void win32_resize_backbuffer(Win32Backbuffer *buffer, int width, int height)
{
    if (buffer->pixels != 0) {
        VirtualFree(buffer->pixels, 0, MEM_RELEASE);
    }

    buffer->width = width;
    buffer->height = height;
    buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
    buffer->info.bmiHeader.biWidth = width;
    buffer->info.bmiHeader.biHeight = -height;
    buffer->info.bmiHeader.biPlanes = 1;
    buffer->info.bmiHeader.biBitCount = 32;
    buffer->info.bmiHeader.biCompression = BI_RGB;

    buffer->pixels = (uint32_t *)VirtualAlloc(0, (SIZE_T)width * (SIZE_T)height * sizeof(uint32_t),
                                              MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

static void win32_present_backbuffer(HWND window, Win32Backbuffer *buffer)
{
    PAINTSTRUCT paint;
    HDC dc = BeginPaint(window, &paint);
    RECT client;
    GetClientRect(window, &client);
    StretchDIBits(dc,
                  0,
                  0,
                  client.right - client.left,
                  client.bottom - client.top,
                  0,
                  0,
                  buffer->width,
                  buffer->height,
                  buffer->pixels,
                  &buffer->info,
                  DIB_RGB_COLORS,
                  SRCCOPY);
    EndPaint(window, &paint);
}

static void win32_set_key(WPARAM key, bool down)
{
    switch (key) {
    case VK_UP:
        g_input.up = down;
        break;
    case VK_DOWN:
        g_input.down = down;
        break;
    case VK_LEFT:
        g_input.left = down;
        break;
    case VK_RIGHT:
        g_input.right = down;
        break;
    case VK_RETURN:
        g_input.confirm = down;
        break;
    case VK_ESCAPE:
        g_input.cancel = down;
        break;
    case VK_SPACE:
        g_input.shoot = down;
        break;
    case VK_TAB:
        g_input.tab = down;
        break;
    default:
        break;
    }
}

static LRESULT CALLBACK win32_window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
    switch (message) {
    case WM_CLOSE:
    case WM_DESTROY:
        g_running = false;
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        win32_set_key(w_param, true);
        return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        win32_set_key(w_param, false);
        return 0;
    case WM_PAINT:
        win32_present_backbuffer(window, &g_backbuffer);
        return 0;
    default:
        return DefWindowProcA(window, message, w_param, l_param);
    }
}

static bool win32_init_window(HINSTANCE instance, HWND *window_out)
{
    WNDCLASSA window_class;
    RECT window_rect = {0, 0, 960, 720};
    DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;

    memset(&window_class, 0, sizeof(window_class));
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = win32_window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorA(0, IDC_ARROW);
    window_class.lpszClassName = "TecmoBasketballPortWindow";

    if (!RegisterClassA(&window_class)) {
        return false;
    }

    AdjustWindowRect(&window_rect, style, FALSE);
    *window_out = CreateWindowExA(0,
                                  window_class.lpszClassName,
                                  "Tecmo Basketball Native Port Prototype",
                                  style,
                                  CW_USEDEFAULT,
                                  CW_USEDEFAULT,
                                  window_rect.right - window_rect.left,
                                  window_rect.bottom - window_rect.top,
                                  0,
                                  0,
                                  instance,
                                  0);
    return *window_out != 0;
}

int tecmo_run_win32_game(const char *project_root)
{
    HINSTANCE instance = GetModuleHandleA(0);
    HWND window = 0;
    LARGE_INTEGER perf_freq;
    LARGE_INTEGER last_counter;
    TecmoRuntime runtime;
    TecmoGameMemory game_memory;
    void *permanent_block;
    void *transient_block;
    const size_t permanent_size = 16U * 1024U * 1024U;
    const size_t transient_size = 16U * 1024U * 1024U;

    memset(&game_memory, 0, sizeof(game_memory));
    permanent_block = VirtualAlloc(0, permanent_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    transient_block = VirtualAlloc(0, transient_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (permanent_block == 0 || transient_block == 0) {
        MessageBoxA(0, "Could not allocate game memory.", "Tecmo Port", MB_ICONERROR);
        return 1;
    }

    tecmo_arena_init(&game_memory.permanent, permanent_block, permanent_size);
    tecmo_arena_init(&game_memory.transient, transient_block, transient_size);

    if (!tecmo_runtime_init(&runtime, &game_memory, project_root)) {
        MessageBoxA(0, "Could not load local decomp roster data. Pass --root or set TECMO_DECOMP_ROOT.",
                    "Tecmo Port", MB_ICONERROR);
        return 1;
    }

    win32_resize_backbuffer(&g_backbuffer, 640, 480);
    if (!win32_init_window(instance, &window)) {
        MessageBoxA(0, "Could not create Win32 window.", "Tecmo Port", MB_ICONERROR);
        tecmo_runtime_shutdown(&runtime);
        return 1;
    }

    QueryPerformanceFrequency(&perf_freq);
    QueryPerformanceCounter(&last_counter);
    g_running = true;

    while (g_running) {
        MSG message;
        LARGE_INTEGER now;
        double elapsed;

        while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                g_running = false;
            }
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }

        QueryPerformanceCounter(&now);
        elapsed = (double)(now.QuadPart - last_counter.QuadPart) / (double)perf_freq.QuadPart;
        if (elapsed < (1.0 / 60.0)) {
            DWORD sleep_ms = (DWORD)(((1.0 / 60.0) - elapsed) * 1000.0);
            if (sleep_ms > 0) {
                Sleep(sleep_ms);
            }
            continue;
        }
        last_counter = now;

        tecmo_runtime_update(&runtime, &g_input);
        if (runtime.quit_requested) {
            g_running = false;
            continue;
        }
        if (g_backbuffer.pixels != 0) {
            TecmoFramebuffer framebuffer;
            framebuffer.pixels = g_backbuffer.pixels;
            framebuffer.width = g_backbuffer.width;
            framebuffer.height = g_backbuffer.height;
            framebuffer.pitch_pixels = g_backbuffer.width;
            tecmo_runtime_render(&runtime, &framebuffer);
        }

        InvalidateRect(window, 0, FALSE);
    }

    tecmo_runtime_shutdown(&runtime);
    if (g_backbuffer.pixels != 0) {
        VirtualFree(g_backbuffer.pixels, 0, MEM_RELEASE);
    }
    VirtualFree(permanent_block, 0, MEM_RELEASE);
    VirtualFree(transient_block, 0, MEM_RELEASE);
    return 0;
}
