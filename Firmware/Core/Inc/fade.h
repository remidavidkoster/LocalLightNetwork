#ifndef FADE_H
#define FADE_H

#include <stdint.h>
#include "config.h"

typedef struct {
    uint8_t channel;
    int steps;
    int currentStep;
    int delayUs;
    uint32_t lastTimestamp;
    float startBrightness;
    float endBrightness;
    uint8_t active;
} FadeState;

typedef struct {
    int steps;
    int currentStep;
    int delayUs;
    uint32_t lastTimestamp;
    float startBrightness;
    float endBrightness;
    float startCCTf;
    float endCCTf;
    uint8_t active;
} FadeStateCCT;

extern FadeState fadeStates[4];
extern FadeStateCCT fadeStatesCCT;
extern uint16_t state[4];
extern uint16_t lastState[4];
extern uint16_t brightness[4];
extern uint16_t lastBrightness[4];
extern float currentBrightness[4];
extern uint16_t CCT;
extern uint16_t lastCCT;

void startFadeToNewBrightness(uint8_t channel, int steps, int delayUs);
void startFadeToNewBrightnessCCT(int steps, int delayUs);
void updateFade(void);
void updateFadeCCT(void);

#endif // FADE_H
