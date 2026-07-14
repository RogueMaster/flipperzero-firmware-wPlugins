#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#include <Shlobj.h>
#include <strsafe.h>
#include <stdlib.h>

#include "game.hpp"

#ifndef NDEBUG
#include <stdio.h>
#include <time.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STBIR_DEFAULT_FILTER_UPSAMPLE STBIR_FILTER_BOX
#include "stb_image_write.h"
#include "stb_image_resize.h"
#include "gif.h"
#endif

#ifdef _MSC_VER
extern "C" int _fltused = 0;
#endif

static HWND hwnd;
static uint8_t* pixels;
static HDC hdc;
static HDC hdc_mem;
static PAINTSTRUCT gpaint{};
static int ww, wh;
static HBRUSH brush_black = NULL;

static DWORD const dwStyle = WS_VISIBLE | WS_OVERLAPPEDWINDOW;
static DWORD const dwExStyle = WS_EX_CLIENTEDGE;

static constexpr int ZOOM = 6 * 128 / FBW;

static constexpr int SZOOM = 2; // screenshot zoom

static char persistent_path[MAX_PATH] = {};
static uint8_t persistent_data[1024];
static bool path_defined = false;

static uint64_t freq;

static uint8_t btn_states = 0;

// time when allowed to repeat
static uint64_t btn_reptimes[8] = {};

static double const REP_INIT_TIME = 0.32;
static double const REP_REPEAT_TIME = 0.16;

static constexpr UINT_PTR TIMER_ID = 0x1001;

#ifndef NDEBUG
static GifWriter gif;
static bool gif_recording = false;
#endif
static uint64_t gif_frame_time = 0;

void save_audio_on_off() {}

uint8_t read_persistent(uint16_t addr)
{
    return persistent_data[addr % 1024];
}

void update_persistent(uint16_t addr, uint8_t data)
{
    persistent_data[addr % 1024] = data;
}

static bool audio = false;

void toggle_audio()
{
    audio = !audio;
}

bool audio_enabled()
{
    return audio;
}

void flush_persistent()
{
    if(!path_defined) return;
    HANDLE f = CreateFileA(
        persistent_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if(f != INVALID_HANDLE_VALUE)
    {
        (void)WriteFile(f, persistent_data, 1024, NULL, NULL);
        CloseHandle(f);
    }
}

#ifndef NDEBUG
static uint64_t perf_counter()
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (uint64_t)t.QuadPart;
}
#endif

uint16_t time_ms()
{
#ifndef NDEBUG
    return uint16_t(perf_counter() * 1000 / freq);
#else
    return 0;
#endif
}

static void screenshot()
{
#ifndef NDEBUG
    char fname[256];
    time_t rawtime;
    struct tm* ti;
    time(&rawtime);
    ti = localtime(&rawtime);
    snprintf(fname, sizeof(fname), "screenshot_%04d%02d%02d%02d%02d%02d.png",
        ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
        ti->tm_hour + 1, ti->tm_min, ti->tm_sec);
    uint8_t* resized = (uint8_t*)malloc(FBW * FBH * SZOOM * SZOOM * 4);
    stbir_resize_uint8(
        pixels, FBW, FBH, 0,
        resized, FBW * SZOOM, FBH * SZOOM, 0,
        4);
    stbi_write_png(fname, FBW * SZOOM, FBH * SZOOM, 4, resized, 0);
    free(resized);
#endif
}

static void send_gif_frame()
{
#ifndef NDEBUG
    if(gif_recording)
    {
        uint64_t t = perf_counter();
        double dt = double(t - gif_frame_time) / freq;
        gif_frame_time = t;
        GifWriteFrame(&gif, pixels, FBW, FBH, int(dt * 100 + 1.5));
    }
#endif
}

static void screen_recording_toggle()
{
#ifndef NDEBUG
    if(gif_recording)
    {
        GifEnd(&gif);
        gif_recording = false;
    }
    else
    {
        char fname[256];
        time_t rawtime;
        struct tm* ti;
        time(&rawtime);
        ti = localtime(&rawtime);
        snprintf(fname, sizeof(fname), "recording_%04d%02d%02d%02d%02d%02d.gif",
            ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
            ti->tm_hour + 1, ti->tm_min, ti->tm_sec);
        GifBegin(&gif, fname, FBW, FBH, 33);
        gif_frame_time = perf_counter();
        gif_recording = true;
        send_gif_frame();
    }
#endif
}

uint8_t poll_btns()
{
    return btn_states;
}

static uint8_t translate_button(WPARAM wParam)
{
    switch(wParam)
    {
    case VK_UP     : return BTN_UP;
    case VK_DOWN   : return BTN_DOWN;
    case VK_LEFT   : return BTN_LEFT;
    case VK_RIGHT  : return BTN_RIGHT;
    case 'A'       : return BTN_A;
    case 'B'       : return BTN_B;
    default        : return 0;
    }
}

static constexpr uint8_t BTN_ARROWS = BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT;

static void refresh()
{
    InvalidateRect(hwnd, NULL, FALSE);
}

static void paint()
{
#ifndef NDEBUG
    //if(perf_counter() - gif_frame_time > 0.05 * freq)
        send_gif_frame();
#endif
    for(int i = 0; i < BUF_BYTES; ++i)
    {
        int x = i % FBW;
        int y = (i / FBW) * 8;
        uint8_t b = buf[i];
        for(int j = 0; j < 8; ++j, b >>= 1)
        {
            uint8_t color = (b & 1) ? 0xff : 0x00;
            uint8_t* p = &pixels[((y + j) * FBW + x) * 4];
            p[0] = color;
            p[1] = color;
            p[2] = color;
            p[3] = 0xff;
        }
    }
    refresh();
    for(auto& b : buf) b = 0;
}

static constexpr int RESIZE_SNAP_PIXELS = 32;
static void calculate_resize_snap(void)
{
    if(ww > RESIZE_SNAP_PIXELS && (ww + RESIZE_SNAP_PIXELS / 2) % FBW < RESIZE_SNAP_PIXELS)
    {
        ww += RESIZE_SNAP_PIXELS / 2;
        ww -= (ww % FBW);
    }
    if(wh > RESIZE_SNAP_PIXELS && (wh + RESIZE_SNAP_PIXELS / 2) % FBH < RESIZE_SNAP_PIXELS)
    {
        wh += RESIZE_SNAP_PIXELS / 2;
        wh -= (wh % FBH);
    }
}

static int imx, imy, imw, imh;
static int imdirty;

static void update_im(void)
{
    if(ww < FBW || wh < FBH)
    {
        int f = ww * FBH - wh * FBW;
        if(f > 0)
        {
            /* window is too wide: bars on the left and right */
            imw = wh * FBW / FBH;
            imh = wh;
        }
        else
        {
            /* window is too narrow: bars on the top and bottom */
            imh = ww * FBH / FBW;
            imw = ww;
        }
    }
    else
    {
        int n = tmin(ww / FBW, wh / FBH);
        imw = FBW * n;
        imh = FBH * n;
    }
    imx = (ww - imw) / 2;
    imy = (wh - imh) / 2;
    imdirty = 1;
}

static LRESULT CALLBACK window_proc(HWND w, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
#if 1
    case WM_SIZING:
    {
        RECT* r = (RECT*)lParam;
        RECT cr = { 0, 0, 0, 0 };
        int dw, dh;
        AdjustWindowRectEx(&cr, dwStyle, FALSE, dwExStyle);
        dw = cr.right - cr.left;
        dh = cr.bottom - cr.top;
        ww = r->right - r->left - dw;
        wh = r->bottom - r->top - dh;
        calculate_resize_snap();
        dw += ww;
        dh += wh;
        switch(wParam)
        {
        case WMSZ_BOTTOMLEFT:
            r->left = r->right - dw;
            /* fallthrough */
        case WMSZ_BOTTOM:
            r->bottom = r->top + dh;
            break;
        case WMSZ_TOPLEFT:
            r->top = r->bottom - dh;
            /* fallthrough */
        case WMSZ_LEFT:
            r->left = r->right - dw;
            break;
        case WMSZ_TOPRIGHT:
            r->right = r->left + dw;
            /* fallthrough */
        case WMSZ_TOP:
            r->top = r->bottom - dh;
            break;
        case WMSZ_BOTTOMRIGHT:
            r->bottom = r->top + dh;
            /* fallthrough */
        case WMSZ_RIGHT:
            r->right = r->left + dw;
            break;
        }
        break;
    }
#endif
    case WM_SIZE:
        ww = (int)LOWORD(lParam);
        wh = (int)HIWORD(lParam);
        update_im();
        refresh();
        break;
    case WM_CLOSE:
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_PAINT:
        BeginPaint(w, &gpaint);
        if(ww == FBW && wh == FBH)
            BitBlt(hdc, 0, 0, FBW, FBH, hdc_mem, 0, 0, SRCCOPY);
        else
        {
            if(imdirty)
            {
                RECT r;
                r.top = 0;
                r.left = 0;
                r.bottom = wh;
                r.right = ww;
                FillRect(hdc, &r, brush_black);
                imdirty = 0;
            }
            StretchBlt(hdc, imx, imy, imw, imh, hdc_mem, 0, 0, FBW, FBH, SRCCOPY);
        }
        EndPaint(w, &gpaint);
        break;
    case WM_KEYDOWN:
        if(wParam == 'R')
            screen_recording_toggle();
        if(wParam == VK_F3)
            screenshot();
        btn_states |= translate_button(wParam);
        break;
    case WM_KEYUP:
        btn_states &= ~translate_button(wParam);
        break;
    default:
        return DefWindowProcA(w, msg, wParam, lParam);
    }
    return 0;
}

#ifdef NDEBUG
void main(void)
#else
int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR     lpCmdLine,
    int       nShowCmd
)
#endif
{
#ifndef NDEBUG
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nShowCmd;
#else
    HINSTANCE hInstance = GetModuleHandleA(NULL);
#endif{

    MSG msg{};
    WNDCLASS wc{};
    BITMAPINFO bmi{};
    RECT wr;
    HBITMAP hbitmap;
    static char const* const CLASS_NAME = "minigolf";

    {
        LARGE_INTEGER t;
        QueryPerformanceFrequency(&t);
        freq = (uint64_t)t.QuadPart;
    }

    if(SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, persistent_path)))
    {
        StringCchCatA(persistent_path, sizeof(persistent_path), "\\minigolf_save");
        HANDLE f = CreateFile(
            persistent_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, NULL);
        if(f != INVALID_HANDLE_VALUE)
        {
            (void)ReadFile(f, persistent_data, 1024, NULL, NULL);
            CloseHandle(f);
        }
        path_defined = true;
    }

    wc.lpfnWndProc = window_proc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    RegisterClass(&wc);

    ww = ZOOM * FBW;
    wh = ZOOM * FBH;
    update_im();

    wr.left = wr.top = 0;
    wr.right = ww;
    wr.bottom = wh;
    AdjustWindowRectEx(&wr, dwStyle, FALSE, dwExStyle);
    hwnd = CreateWindowEx(
        dwExStyle,
        CLASS_NAME,
        CLASS_NAME,
        dwStyle,
        CW_USEDEFAULT, CW_USEDEFAULT,
        (int)(wr.right - wr.left),
        (int)(wr.bottom - wr.top),
        NULL, NULL, hInstance, NULL);

    if(hwnd == NULL)
        goto byebye;

    hdc = GetDC(hwnd);
    hdc_mem = CreateCompatibleDC(hdc);

    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = FBW;
    bmi.bmiHeader.biHeight = -FBH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    hbitmap = CreateDIBSection(0, &bmi, DIB_RGB_COLORS, (void**)&pixels, NULL, 0);
    if(hbitmap == 0)
        goto byebye;
    SelectObject(hdc_mem, hbitmap);

    brush_black = CreateSolidBrush(RGB(0, 0, 0));
    SetStretchBltMode(hdc, COLORONCOLOR);

    ShowWindow(hwnd, SW_NORMAL);

    game_setup();

    btn_states = 0;
    SetTimer(hwnd, TIMER_ID, (UINT)16, (TIMERPROC)NULL);
    while(GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if(msg.hwnd == hwnd && msg.message == WM_TIMER && msg.wParam == TIMER_ID)
        {
            game_loop();
            paint();
#if ARDUGOLF_FX
            fx_read_data_bytes(get_hole_fx_addr(leveli), &buf[0], 1024);
            levelext = fxlevel.ext;
#endif
        }
    }

byebye:
#ifndef NDEBUG
    return 0;
#else
    ExitProcess(0);
#endif
}

#ifdef NDEBUG
#pragma function(memset)
void* __cdecl memset(void* dst, int c, size_t count)
{
    char* bytes = (char*)dst;
    while(count--) *bytes++ = (char)c;
    return dst;
}
#pragma function(memcpy)
void* __cdecl memcpy(void* dst, void const* src, size_t count)
{
    char* bytes = (char*)dst;
    char const* src_bytes = (char*)src;
    while(count--) *bytes++ = *src_bytes++;
    return dst;
}
#endif

#endif
