#include <engine_config.h>
#pragma once

#define LOG_BUFFER_ENTRIES  2048
#define LOG_BUFFER_CAPTION  64

typedef enum {
    LDEBUG = 10u,
    LINFO = 20u,
    LWARN = 30u,
    LERROR = 40u,
} __attribute__((__packed__)) debug_level_t;

#define OMAIN   "ENGINE"
#define OASSET  "ASSETS"
#define OINPUT  "INPUT"
#define OAUDIO  "AUDIO"
#define ORENDER "RENDER"
#define OLOGGER "LOGGER"

static inline const char* logger_str_severity(const debug_level_t severity) {
    switch (severity) {
    case LDEBUG: return "DEBUG";
    case LINFO:  return "INFO";
    case LWARN:  return "WARN";
    case LERROR: return "ERROR";
    default:     return "N/A";
    }
}

// Create a Log Entry for Debugging
void logger(const unsigned char severity, const char* origin, const char* fmt, ...);

bool_t engine_logger_init(const engine_config_t* config);
bool_t engine_logger_tick(float delta);
void engine_logger_exit();