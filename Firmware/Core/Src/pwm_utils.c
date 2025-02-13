#include "pwm_utils.h"
#include "hrtim.h"
#include <math.h>

#define ABS(x) ((x) < 0 ? -(x) : (x))
#define PI 3.14159265359f

void PWM_Set(uint8_t ch, uint16_t v) {
    if (ch == 0) HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].CMP3xR = v;
    if (ch == 1) HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].CMP1xR = v;
    if (ch == 2) HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_D].CMP3xR = v;
    if (ch == 3) HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_D].CMP1xR = v;
}

float cosFast(float x) {
    x *= 0.159154943f;
    x -= 0.25f + (int)(x + 0.25f);
    x *= 16.0f * (ABS(x) - 0.5f);
    return x;
}

// Cosine ramp from 0 to 1
float flatCos(float x) {
    return 0.5f - 0.5f * (cosFast(PI * x));
}

float CIELAB(float x) {
    float lum = x * 100.0f;
    return lum < 8.0f ? lum / 903.3f : powf(((lum + 16) / 116), 3);
}

void setLEDBrightness(uint8_t channel, float value) {
    PWM_Set(channel, PWM_MIN + (int)roundf(value * (PWM_MAX - PWM_MIN)));
}