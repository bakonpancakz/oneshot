#include <engine_config.h>
#pragma once

// // Prepare X samples of audio for X channels
// static void audio_prepare(unsigned int samples, unsigned int channels);

bool_t engine_audio_init(const engine_config_t* config);
bool_t engine_audio_tick(float delta);
void engine_audio_exit(void);