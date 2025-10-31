#include <platform_time.h>
#include <engine_logger.h>
#include <engine_config.h>
#include <pthread.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#endif

static pthread_mutex_t logger_mtx = PTHREAD_MUTEX_INITIALIZER;
static bool_t logger_continue = TRUE;
static int logger_buffer_length = 0;
static unsigned char logger_buffer[LOG_BUFFER_ENTRIES];
static char* exit_caption = NULL;
static char* exit_message = NULL;

static void logger_log_flush(void) {
    pthread_mutex_lock(&logger_mtx);
    if (logger_buffer_length > 0) {
        fwrite(logger_buffer, 1, (size_t)logger_buffer_length, stdout);
        fflush(stdout);
        logger_buffer_length = 0;
    };
    pthread_mutex_unlock(&logger_mtx);
}

void logger(const unsigned char severity, const char* origin, const char* fmt, ...) {
    char buffer_message[LOG_BUFFER_ENTRIES];
    char buffer_entry[LOG_BUFFER_ENTRIES];
    unsigned int h, m, s, ms = 0;
    time_get(&h, &m, &s, &ms);

    // Generate Message
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer_message, sizeof(buffer_message), fmt, args);
    va_end(args);

    // Generate Entry
    int buffer_length = snprintf(buffer_entry, sizeof(buffer_entry),
        "%02d:%02d:%02d.%03d | %-5s | %-6s | %s\n",
        h, m, s, ms, logger_str_severity(severity), origin, buffer_message
    );
    if (logger_buffer_length + buffer_length >= LOG_BUFFER_ENTRIES) {
        // Buffer Full! Flush Immediately...
        logger_log_flush();
    }

    // Copy Entry
    pthread_mutex_lock(&logger_mtx);
    memcpy(logger_buffer + logger_buffer_length, buffer_entry, (size_t)buffer_length);
    logger_buffer_length += buffer_length;
    pthread_mutex_unlock(&logger_mtx);

    // Error Handling
    if (severity >= LERROR) {
        logger_continue = FALSE;
        char buffer_caption[LOG_BUFFER_CAPTION];
        snprintf(buffer_caption, LOG_BUFFER_CAPTION, "OneShot Error (%s)", origin);
        exit_caption = strdup(buffer_caption);
        exit_message = strdup(buffer_message);
    }
}

bool_t engine_logger_init(const engine_config_t* config) {
#ifdef _WIN32
    if (config->logger_console) {
        AllocConsole();
        FILE* out;
        freopen_s(&out, "CONOUT$", "w", stdout);
        freopen_s(&out, "CONOUT$", "w", stderr);
        SetConsoleTitle("Console");
    }
#endif
    return TRUE;
}

bool_t engine_logger_tick(float delta) {
    (void)delta;
    logger_log_flush();
    return logger_continue;
}

void engine_logger_exit(void) {
    logger_log_flush();
#ifdef _WIN32
    if (exit_message && exit_caption) {
        MessageBoxA(NULL, exit_message, exit_caption, MB_OK | MB_ICONERROR);
    }
#endif
    free(exit_message);
    free(exit_caption);
}