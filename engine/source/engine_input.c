#include <engine_config.h>
#include <engine_input.h>
#include <pthread.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#include <xinput.h>

#define KEY_PRESSED         0x80 // Bitmask
#define XINPUT_TARGET       0x00 // Player 1
#define XINPUT_VIBRATE_ON  (XINPUT_VIBRATION){ .wLeftMotorSpeed = 65535, .wRightMotorSpeed = 65535 }
#define XINPUT_VIBRATE_OFF (XINPUT_VIBRATION){ .wLeftMotorSpeed = 0,     .wRightMotorSpeed = 0 }

static XINPUT_STATE state_pad;
static BYTE state_key[256];
#endif

static pthread_mutex_t input_mtx = PTHREAD_MUTEX_INITIALIZER;
static int input_controller = 0;
static float input_vibration = 0;
static float input_joystick_x = 0;
static float input_joystick_y = 0;

void input_set_vibrate(float secs) {
    if (secs > input_vibration) {
        input_vibration = secs;
    }
}

void input_get_joystick(float* x, float* y) {
    pthread_mutex_lock(&input_mtx);
    *x = input_joystick_x;
    *y = input_joystick_y;
    pthread_mutex_unlock(&input_mtx);
}

bool_t input_get_button(input_button_t button) {
    pthread_mutex_lock(&input_mtx);
    bool_t down = FALSE;
#ifdef _WIN32
    switch (button) {
    case INPUT_BUTTON_FULLSCREEN: {
        down =
            (state_key[VK_LMENU] & KEY_PRESSED) &&
            (state_key[VK_RETURN] & KEY_PRESSED);
        break;
    }
    case INPUT_BUTTON_ACTION: {
        down =
            (state_key[VK_SPACE] & KEY_PRESSED) ||
            (state_key[VK_RETURN] & KEY_PRESSED) ||
            (state_pad.Gamepad.wButtons & XINPUT_GAMEPAD_A) ||
            (state_pad.Gamepad.wButtons & XINPUT_GAMEPAD_X);
        break;
    }
    case INPUT_BUTTON_CANCEL: {
        down =
            (state_key[VK_BACK] & KEY_PRESSED) ||
            (state_key[VK_LSHIFT] & KEY_PRESSED) ||
            (state_pad.Gamepad.wButtons & XINPUT_GAMEPAD_B) ||
            (state_pad.Gamepad.wButtons & XINPUT_GAMEPAD_Y);
        break;
    }
    default: { break; }
    }
#else
#warning "Input Button Binds Not Implemented, all buttons will act as depressed"
#endif
    pthread_mutex_unlock(&input_mtx);
    return down;
}

bool_t engine_input_init(const engine_config_t* config) {
    (void)config;
    return TRUE;
}

bool_t engine_input_tick(float delta) {
    input_vibration -= delta;
    float x = 0, y = 0;

#ifdef _WIN32
    // Update State
    pthread_mutex_lock(&input_mtx);
    input_controller = (XInputGetState(XINPUT_TARGET, &state_pad) == ERROR_SUCCESS);
    GetKeyboardState(state_key);
    pthread_mutex_unlock(&input_mtx);

    // Update Virtual Joystick
    if (input_controller) {
        x += (float)state_pad.Gamepad.sThumbLX / INPUT_JOYSTICK_DIV;
        y += (float)state_pad.Gamepad.sThumbLY / INPUT_JOYSTICK_DIV;
    }
    if ((state_key['W'] & KEY_PRESSED) || (state_key[VK_UP] & KEY_PRESSED))    y += 1;
    if ((state_key['A'] & KEY_PRESSED) || (state_key[VK_LEFT] & KEY_PRESSED))  x -= 1;
    if ((state_key['S'] & KEY_PRESSED) || (state_key[VK_DOWN] & KEY_PRESSED))  y -= 1;
    if ((state_key['D'] & KEY_PRESSED) || (state_key[VK_RIGHT] & KEY_PRESSED)) x += 1;

    // Update Vibration
    XInputSetState(XINPUT_TARGET, input_vibration > 0
        ? &XINPUT_VIBRATE_ON
        : &XINPUT_VIBRATE_OFF
    );
#else
#warning "Input Tick Not Implemented, no inputs will be collected"
#endif

    // Scale Virtual Joystick
    // Prevents diagonal movement being faster than straight movements :3
    float magnitude = sqrtf(x * x + y * y);
    if (magnitude > 1.0f) {
        x /= magnitude;
        y /= magnitude;
    }
    x *= INPUT_VECTOR_SCALE;
    y *= INPUT_VECTOR_SCALE;

    pthread_mutex_lock(&input_mtx);
    input_joystick_x = x;
    input_joystick_y = y;
    pthread_mutex_unlock(&input_mtx);

    return TRUE;
}


void engine_input_exit(void) {
    // Do Nothing...
}