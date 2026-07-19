#define WIN32_LEAN_AND_MEAN

#include "tecmo_game.h"
#include "tecmo_audio_output.h"

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

_Static_assert(TECMO_MUSIC_TICK_NUMERATOR == 39375000U,
               "Win32 frame pacing must match the native music cadence");
_Static_assert(TECMO_MUSIC_TICK_DENOMINATOR == 655171U,
               "Win32 frame pacing must match the native music cadence");

typedef struct Win32Backbuffer {
    BITMAPINFO info;
    uint32_t *pixels;
    int width;
    int height;
} Win32Backbuffer;

static Win32Backbuffer g_backbuffer;
static bool g_running;
static TecmoControls g_controls[2];

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
    TecmoControlButton button;
    unsigned player_index = 0U;

    switch (key) {
    case VK_NUMPAD8:
        player_index = 1U;
        button = TECMO_CONTROL_UP;
        break;
    case VK_NUMPAD2:
        player_index = 1U;
        button = TECMO_CONTROL_DOWN;
        break;
    case VK_NUMPAD4:
        player_index = 1U;
        button = TECMO_CONTROL_LEFT;
        break;
    case VK_NUMPAD6:
        player_index = 1U;
        button = TECMO_CONTROL_RIGHT;
        break;
    case VK_NUMPAD9:
        player_index = 1U;
        button = TECMO_CONTROL_CONFIRM;
        break;
    case VK_NUMPAD3:
        player_index = 1U;
        button = TECMO_CONTROL_CANCEL;
        break;
    case VK_NUMPAD1:
        player_index = 1U;
        button = TECMO_CONTROL_SHOOT;
        break;
    case VK_NUMPAD7:
        player_index = 1U;
        button = TECMO_CONTROL_TAB;
        break;
    case VK_UP:
        button = TECMO_CONTROL_UP;
        break;
    case VK_DOWN:
        button = TECMO_CONTROL_DOWN;
        break;
    case VK_LEFT:
        button = TECMO_CONTROL_LEFT;
        break;
    case VK_RIGHT:
        button = TECMO_CONTROL_RIGHT;
        break;
    case VK_RETURN:
        button = TECMO_CONTROL_CONFIRM;
        break;
    case VK_ESCAPE:
        button = TECMO_CONTROL_CANCEL;
        break;
    case VK_SPACE:
        button = TECMO_CONTROL_SHOOT;
        break;
    case VK_TAB:
        button = TECMO_CONTROL_TAB;
        break;
    case 'Q':
        button = TECMO_CONTROL_BANK_PREV;
        break;
    case 'E':
        button = TECMO_CONTROL_BANK_NEXT;
        break;
    case 'T':
        button = TECMO_CONTROL_TABLE_TOGGLE;
        break;
    case 'S':
        button = TECMO_CONTROL_SAVE;
        break;
    case 'R':
        button = TECMO_CONTROL_PRESET_RABBIT;
        break;
    case 'M':
        button = TECMO_CONTROL_PRESET_TECMO;
        break;
    case 'C':
        button = TECMO_CONTROL_PRESET_COMPOSITE;
        break;
    case VK_BACK:
    case VK_DELETE:
        button = TECMO_CONTROL_REMOVE;
        break;
    case VK_F3:
        button = TECMO_CONTROL_DEBUG_TOGGLE;
        break;
    default:
        return;
    }
    tecmo_controls_set_button(&g_controls[player_index], button, down);
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
    LONGLONG next_frame_counter = 0;
    LONGLONG frame_step_whole = 0;
    LONGLONG frame_step_fraction = 0;
    LONGLONG frame_fraction_accumulator = 0;
    TecmoAudioOutput audio_output;
    TecmoRuntime *runtime = 0;
    TecmoGameMemory game_memory;
    void *permanent_block = 0;
    void *transient_block = 0;
    const size_t permanent_size = 16U * 1024U * 1024U;
    const size_t transient_size = 16U * 1024U * 1024U;
    int result = 1;

    memset(&game_memory, 0, sizeof(game_memory));
    memset(&audio_output, 0, sizeof(audio_output));
    runtime = (TecmoRuntime *)VirtualAlloc(0,
                                           sizeof(*runtime),
                                           MEM_RESERVE | MEM_COMMIT,
                                           PAGE_READWRITE);
    permanent_block = VirtualAlloc(0, permanent_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    transient_block = VirtualAlloc(0, transient_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (runtime == 0 || permanent_block == 0 || transient_block == 0) {
        MessageBoxA(0, "Could not allocate game memory.", "Tecmo Port", MB_ICONERROR);
        goto cleanup;
    }

    tecmo_arena_init(&game_memory.permanent, permanent_block, permanent_size);
    tecmo_arena_init(&game_memory.transient, transient_block, transient_size);

    if (!tecmo_runtime_init_with_flags(runtime,
                                       &game_memory,
                                       project_root,
                                       TECMO_RUNTIME_INIT_ALLOW_EMPTY_ROSTER)) {
        MessageBoxA(0, "Could not initialize the native game runtime from the local asset pack.",
                    "Tecmo Port", MB_ICONERROR);
        goto cleanup;
    }
    runtime->normal_play_active = true;
    tecmo_runtime_set_mode(runtime, TECMO_MODE_FIRST_SPRITE);

    win32_resize_backbuffer(&g_backbuffer, 640, 480);
    if (!win32_init_window(instance, &window)) {
        MessageBoxA(0, "Could not create Win32 window.", "Tecmo Port", MB_ICONERROR);
        goto cleanup;
    }

    if (!QueryPerformanceFrequency(&perf_freq) || perf_freq.QuadPart <= 0 ||
        !QueryPerformanceCounter(&last_counter)) {
        MessageBoxA(0, "Could not initialize the native frame clock.",
                    "Tecmo Port", MB_ICONERROR);
        goto cleanup;
    }
    {
        LONGLONG scaled = perf_freq.QuadPart *
                          (LONGLONG)TECMO_MUSIC_TICK_DENOMINATOR;
        frame_step_whole = scaled / (LONGLONG)TECMO_MUSIC_TICK_NUMERATOR;
        frame_step_fraction = scaled % (LONGLONG)TECMO_MUSIC_TICK_NUMERATOR;
        next_frame_counter = last_counter.QuadPart + frame_step_whole;
        frame_fraction_accumulator = frame_step_fraction;
    }
    tecmo_controls_init(&g_controls[0]);
    tecmo_controls_init(&g_controls[1]);
    (void)tecmo_audio_output_init(&audio_output, &runtime->music_player);
    (void)tecmo_audio_output_select_gameplay_player(
        &audio_output, &runtime->gameplay_scene.audio_player);
    g_running = true;

    if (g_backbuffer.pixels != 0) {
        TecmoFramebuffer framebuffer;
        framebuffer.pixels = g_backbuffer.pixels;
        framebuffer.width = g_backbuffer.width;
        framebuffer.height = g_backbuffer.height;
        framebuffer.pitch_pixels = g_backbuffer.width;
        tecmo_runtime_render(runtime, &framebuffer);
        InvalidateRect(window, 0, FALSE);
        UpdateWindow(window);
    }

    while (g_running) {
        MSG message;
        LARGE_INTEGER now;

        while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                g_running = false;
            }
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
        tecmo_audio_output_service(&audio_output);

        QueryPerformanceCounter(&now);
        if (now.QuadPart < next_frame_counter) {
            double remaining = (double)(next_frame_counter - now.QuadPart) /
                               (double)perf_freq.QuadPart;
            DWORD sleep_ms = (DWORD)(remaining * 1000.0);
            if (sleep_ms > 0) {
                Sleep(sleep_ms);
            }
            continue;
        }
        next_frame_counter += frame_step_whole;
        frame_fraction_accumulator += frame_step_fraction;
        if (frame_fraction_accumulator >=
                (LONGLONG)TECMO_MUSIC_TICK_NUMERATOR) {
            ++next_frame_counter;
            frame_fraction_accumulator -=
                (LONGLONG)TECMO_MUSIC_TICK_NUMERATOR;
        }

        tecmo_controls_begin_frame(&g_controls[0]);
        tecmo_controls_begin_frame(&g_controls[1]);
        runtime->frame_seconds = (float)(
            (double)TECMO_MUSIC_TICK_DENOMINATOR /
            (double)TECMO_MUSIC_TICK_NUMERATOR);
        tecmo_runtime_update_players(runtime,
                                     tecmo_controls_held(&g_controls[0]),
                                     tecmo_controls_held(&g_controls[1]));
        if (runtime->quit_requested) {
            g_running = false;
            continue;
        }
        if (g_backbuffer.pixels != 0) {
            TecmoFramebuffer framebuffer;
            framebuffer.pixels = g_backbuffer.pixels;
            framebuffer.width = g_backbuffer.width;
            framebuffer.height = g_backbuffer.height;
            framebuffer.pitch_pixels = g_backbuffer.width;
            tecmo_runtime_render(runtime, &framebuffer);
        }

        InvalidateRect(window, 0, FALSE);
    }

    result = 0;

cleanup:
    tecmo_audio_output_shutdown(&audio_output);
    if (runtime != 0) {
        tecmo_runtime_shutdown(runtime);
    }
    if (g_backbuffer.pixels != 0) {
        VirtualFree(g_backbuffer.pixels, 0, MEM_RELEASE);
        memset(&g_backbuffer, 0, sizeof(g_backbuffer));
    }
    if (permanent_block != 0) {
        VirtualFree(permanent_block, 0, MEM_RELEASE);
    }
    if (transient_block != 0) {
        VirtualFree(transient_block, 0, MEM_RELEASE);
    }
    if (runtime != 0) {
        VirtualFree(runtime, 0, MEM_RELEASE);
    }
    return result;
}
