#pragma once

static const float INPUT_VECTOR_SCALE = 1.0f;
static const float INPUT_JOYSTICK_DIV = 32768.0f;

typedef enum {
    INPUT_BUTTON_FULLSCREEN = 101,
    INPUT_BUTTON_ACTION = 102,
    INPUT_BUTTON_CANCEL = 103,
} input_button_t;

// Vibrate the controller for X seconds (if connected)
void input_set_vibrate(float secs);

// Get scaled direction of left (movement) Virtual Joystick
void input_get_joystick(float* x, float* y);

// Return pressed state of the requested button
bool_t input_get_button(input_button_t button);

bool_t engine_input_init(const engine_config_t* config);
bool_t engine_input_tick(float delta);
void engine_input_exit(void);