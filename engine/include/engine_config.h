#pragma once

// Special Boolean Type to protect against WINAPI conflicts
typedef unsigned char bool_t;
#define TRUE  1
#define FALSE 0

#define ASSET_WORKER_LIMIT      8
#define ASSET_CACHE_LINE_SIZE   64    

typedef struct {
    bool_t logger_console;      // Console Enabled?
    int asset_threads;          // Asset Thread Count
    int render_height;          // Render Window Height
    int render_width;           // Render Window Width
    bool_t render_fullscreen;   // Render Window Fullscreen?
} engine_config_t;

static inline engine_config_t engine_config_init() {
    return (engine_config_t) {
        .logger_console = FALSE,
            .asset_threads = 2,
            .render_height = 0,
            .render_width = 0,
            .render_fullscreen = FALSE
    };
}