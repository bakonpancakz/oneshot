#include <engine_config.h>
#include <engine_audio.h>
#include <engine_assets.h>
#include <stdlib.h>

// static unsigned int output_rate = 48000;
// static unsigned int output_channels = 2;
// static signed short* output_buffer = NULL;

// static void audio_prepare(unsigned int samples, unsigned int channels) {
//     (void)samples;
//     (void)channels;
// }

bool_t engine_audio_init(const engine_config_t* config) {
    (void)config;
    return TRUE;
}

bool_t engine_audio_tick(float delta) {
    (void)delta;
    return TRUE;
}

void engine_audio_exit(void) {
    // Close Audio Thread
}
