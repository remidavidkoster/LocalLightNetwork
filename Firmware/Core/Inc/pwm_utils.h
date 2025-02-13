#ifndef PWM_UTILS_H
#define PWM_UTILS_H

#include <stdint.h>

#define PWM_MIN 0x0022
#define PWM_MAX 0xFFF8

void PWM_Set(uint8_t ch, uint16_t v);
float cosFast(float x);
float flatCos(float x);
float CIELAB(float x);
void setLEDBrightness(uint8_t channel, float value);

#endif // PWM_UTILS_H