#include <engine_config.h>
#include <engine_input.h>
#include <engine_render.h>
#include <engine_assets.h>
#include <engine_logger.h>

static bool_t engine_continue = TRUE;

#ifdef _WIN32
#include <windows.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nShowCmd;

    // Initialize Config
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    engine_config_t config = engine_config_init();
    for (int i = 0; i < argc; i++) {
        if (!wcsncmp(argv[i], L"--asset-threads=", 16)) config.asset_threads = _wtoi(argv[i] + 16);
        if (!wcsncmp(argv[i], L"--height=", 9))         config.render_height = _wtoi(argv[i] + 9);
        if (!wcsncmp(argv[i], L"--width=", 8))          config.render_width = _wtoi(argv[i] + 8);
        if (wcsstr(argv[i], L"--fullscreen"))           config.render_fullscreen = TRUE;
        if (wcsstr(argv[i], L"--console"))              config.logger_console = TRUE;
    }
    LocalFree(argv);

    // Initialize Loop
    if (
        !engine_logger_init(&config) ||
        !engine_assets_init(&config) ||
        !engine_input_init(&config) ||
        !engine_render_init(&config)
        ) {
        engine_logger_exit();
        return 1;
    }

    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    while (engine_continue) {

        // Process Tick
        QueryPerformanceCounter(&end);
        float delta = (float)(end.QuadPart - start.QuadPart) / (float)freq.QuadPart;
        start = end;
        if (
            !engine_input_tick(delta) ||
            !engine_assets_tick(delta) ||
            !engine_render_tick(delta) ||
            !engine_logger_tick(delta)
            ) {
            break;
        }

        // Process Events
        MSG event;
        while (PeekMessage(&event, NULL, 0, 0, PM_REMOVE)) {
            if (event.message == WM_QUIT) {
                engine_continue = 0;
                break;
            }
            TranslateMessage(&event);
            DispatchMessage(&event);
        }

        // Sleep .zZ
        if (delta < RENDER_DELAY) {
            DWORD sleep_ms = (DWORD)(RENDER_DELAY - delta);
            if (sleep_ms > 0) {
                Sleep(sleep_ms);
            }
        }
    }
    engine_input_exit();
    engine_render_exit();
    engine_assets_exit();
    engine_logger_exit();
    return 0;
}
#else
#error "Engine Loop Not Implemented"
#endif