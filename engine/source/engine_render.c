#include <engine_config.h>
#include <engine_render.h>
#include <engine_logger.h>
#include <engine_input.h>
// #include <vulkan/vulkan.h>
#include <string.h>

static int render_height_def = 720;     // (Default) Window Height
static int render_width_def = 1280;     // (Default) Window Width
static int render_height_cur;           // (Current) Window Height
static int render_width_cur;            // (Current) Window Width
static bool_t render_continue = TRUE;
static bool_t render_fullscreen = FALSE;
static bool_t render_fullscreen_debounce = FALSE;
static unsigned int* render_framebuffer = NULL;
static unsigned int render_resolution = 0;

#ifdef _WIN32
#include <windows.h>

#define WINDOW_STYLE_FULLSCREEN   (WS_POPUP)
#define WINDOW_STYLE_WINDOWED     (WS_VISIBLE | WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
#define WINDOW_STYLE(F)           (F ? WINDOW_STYLE_FULLSCREEN : WINDOW_STYLE_WINDOWED)
#define WINDOW_EXSTYLE_FULLSCREEN (WS_EX_APPWINDOW)
#define WINDOW_EXSTYLE_WINDOWED   (WS_EX_LEFT)
#define WINDOW_EXSTYLE(F)         (F ? WINDOW_EXSTYLE_FULLSCREEN : WINDOW_EXSTYLE_WINDOWED)

BITMAPINFO window_bitmap;
HWND window_handle;
HDC window_device;
#endif

static int render_fullscreen_set(int enabled) {
    render_fullscreen = enabled;
    int rw = 0, rh = 0;

#ifdef _WIN32
    int px = 0, py = 0;
    MONITORINFO mi;
    HMONITOR hm = MonitorFromWindow(window_handle, MONITOR_DEFAULTTONEAREST);
    mi.cbSize = sizeof(mi);
    GetMonitorInfo(hm, &mi);

    if (render_fullscreen) {
        rw = mi.rcMonitor.right - mi.rcMonitor.left;
        rh = mi.rcMonitor.bottom - mi.rcMonitor.top;
        px = mi.rcMonitor.left;
        py = mi.rcMonitor.top;
    }
    else {
        rw = render_width_def;
        rh = render_height_def;
        px = mi.rcMonitor.left + ((mi.rcMonitor.right - mi.rcMonitor.left) - rw) / 2;
        py = mi.rcMonitor.top + ((mi.rcMonitor.bottom - mi.rcMonitor.top) - rh) / 2;
    }

    SetWindowLongPtr(window_handle, GWL_STYLE, WINDOW_STYLE(render_fullscreen));
    SetWindowLongPtr(window_handle, GWL_EXSTYLE, WINDOW_EXSTYLE(render_fullscreen));
    SetWindowPos(window_handle, HWND_TOP, px, py, rw, rh, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
#endif

    render_width_cur = rw;
    render_height_cur = rh;
    return 1;
}

#ifdef _WIN32
static LRESULT CALLBACK RenderWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CLOSE:
    case WM_DESTROY: {
        render_continue = FALSE;
        return 0;
    }
    case WM_SYSCOMMAND: {
        if ((wParam & 0xFFF0) == SC_MAXIMIZE) {
            render_fullscreen_set(!render_fullscreen);
            return 0;
        }
        break;
    }
    default: { break; }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}
#endif

bool_t engine_render_init(const engine_config_t* config) {

    // Load Configuration
    if (config->render_fullscreen) {
        render_fullscreen = TRUE;
        logger(LINFO, ORENDER, "Setting Window Fullscreen");
    }
    if (config->render_height > 0) {
        render_height_def = config->render_height;
        logger(LINFO, ORENDER, "Setting Window Height : %dpx", config->render_height);
    }
    if (config->render_width > 0) {
        logger(LINFO, ORENDER, "Setting Window Width : %dpx", config->render_width);
        render_width_def = config->render_width;
    }
    render_fullscreen = config->render_fullscreen;
    render_height_cur = render_height_def;
    render_width_cur = render_width_def;

#ifdef _WIN32
    // Prepare Window
    HINSTANCE hInstance = GetModuleHandle(NULL);
    WNDCLASS window_class = {
       .lpfnWndProc = RenderWndProc,
       .hInstance = hInstance,
       .lpszClassName = "RenderWindowClass",
       .hIcon = LoadIcon(hInstance, "IDI_APPICON"),
       .hCursor = LoadCursor(NULL, IDC_ARROW),
    };
    RegisterClass(&window_class);

    window_handle = CreateWindowEx(
        0, window_class.lpszClassName, RENDER_TITLE, 0,
        CW_USEDEFAULT, CW_USEDEFAULT, 0, 0,
        NULL, NULL, hInstance, NULL
    );
    if (window_handle == NULL) {
        logger(LERROR, ORENDER, "Unable to Create Window (%lu)", GetLastError());
        return FALSE;
    }
    window_device = GetDC(window_handle);
    render_fullscreen_set(render_fullscreen);
#endif

    return TRUE;
}

bool_t engine_render_tick(float delta) {
    (void)delta;
    if (!render_continue) {
        return FALSE;
    }

    // Toggle Fullscreen
    if (input_get_button(INPUT_BUTTON_FULLSCREEN)) {
        if (!render_fullscreen_debounce) {
            render_fullscreen_debounce = TRUE;
            render_fullscreen_set(!render_fullscreen);
        }
    }
    else {
        render_fullscreen_debounce = FALSE;
    }

    // Allocate Framebuffer
    unsigned int framebuffer_resolution = render_height_cur * render_width_cur;
    unsigned int framebuffer_size = framebuffer_resolution * sizeof(unsigned int);
    if (framebuffer_resolution != render_resolution) {
        free(render_framebuffer);
        render_framebuffer = NULL;
#ifdef _WIN32
        // Resize Window Bitmap Buffer
        window_bitmap = (BITMAPINFO){
                .bmiHeader = {
                    .biSize = sizeof(BITMAPINFOHEADER),
                    .biWidth = render_width_cur,
                    .biHeight = -render_height_cur,
                    .biPlanes = 1,
                    .biBitCount = 32,
                    .biCompression = BI_RGB,
                }
        };
#endif
    }
    if (render_framebuffer == NULL) {
        render_framebuffer = malloc(framebuffer_size);
        if (render_framebuffer == NULL) {
            logger(LERROR, ORENDER, "Failed to allocate %d byte Frame Buffer (%s)",
                framebuffer_size, strerror(errno)
            );
            return FALSE;
        }
        render_resolution = framebuffer_resolution;
        memset(render_framebuffer, 0, framebuffer_size);
        logger(LINFO, ORENDER, "Resolution Changed (%dx%d)", render_width_cur, render_height_cur);
    }

    // TODO: Upload Textures & Shaders
    // TODO: Render Frame

    // Submit Framebuffer
#ifdef _WIN32
    StretchDIBits(
        window_device,
        0, 0, render_width_cur, render_height_cur,
        0, 0, render_width_cur, render_height_cur,
        render_framebuffer,
        &window_bitmap,
        DIB_RGB_COLORS,
        SRCCOPY
    );
#endif

    return TRUE;
}

void engine_render_exit() {
    if (render_framebuffer) {
        free(render_framebuffer);
        render_framebuffer = NULL;
    }
#ifdef _WIN32
    if (window_device && window_handle) {
        ReleaseDC(window_handle, window_device);
        window_device = NULL;
    }
    if (window_handle) {
        DestroyWindow(window_handle);
        window_handle = NULL;
    }
#endif
}
