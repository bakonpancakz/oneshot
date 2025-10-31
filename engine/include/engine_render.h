#include <engine_config.h>
#pragma once

#define RENDER_DELAY 1000 / 60
#define RENDER_TITLE "KUMA"

bool_t engine_render_init(const engine_config_t* config);
bool_t engine_render_tick(float delta);
void engine_render_exit();