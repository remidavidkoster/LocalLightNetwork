#include "fade.h"
#include "tim.h"
#include "pwm_utils.h"
#include <math.h>
#include "LUTs.h"

FadeState fadeStates[4] = {0};
FadeStateCCT fadeStatesCCT = {0};
uint16_t state[4] = {1, 1, 1, 1};
uint16_t lastState[4] = {1, 1, 1, 1};
uint16_t brightness[4] = {127, 127, 127, 127};
uint16_t lastBrightness[4] = {0, 0, 0, 0};
float currentBrightness[4] = {0, 0, 0, 0};
uint16_t CCT = 225;
uint16_t lastCCT = 225;
float currentCCT = 0.5f;

void startFadeToNewBrightness(uint8_t channel, int steps, int delayUs) {
    fadeStates[channel].channel = channel;
    fadeStates[channel].steps = steps;
    fadeStates[channel].currentStep = 0;
    fadeStates[channel].delayUs = delayUs;
    fadeStates[channel].lastTimestamp = TIM2->CNT;
    fadeStates[channel].startBrightness = currentBrightness[channel];
    fadeStates[channel].endBrightness = state[channel] ? brightness[channel] / 255.0f : 0.0f;
    fadeStates[channel].active = 1;

    // Update lastBrightness and lastState
    lastBrightness[fadeStates[channel].channel] = brightness[fadeStates[channel].channel];
    lastState[fadeStates[channel].channel] = state[fadeStates[channel].channel];
}

void startFadeToNewBrightnessCCT(int steps, int delayUs) {
    fadeStatesCCT.steps = steps;
    fadeStatesCCT.currentStep = 0;
    fadeStatesCCT.delayUs = delayUs;
    fadeStatesCCT.lastTimestamp = TIM2->CNT;
    fadeStatesCCT.startBrightness = currentBrightness[0];
    fadeStatesCCT.endBrightness = state[0] ? brightness[0] / 255.0f : 0.0f;
    fadeStatesCCT.startCCTf = currentCCT;
    fadeStatesCCT.endCCTf = (float)(CCT - MIN_CCT) / (MAX_CCT - MIN_CCT);
    fadeStatesCCT.active = 1;

    // Update lastBrightness and lastState
    lastBrightness[0] = brightness[0];
    lastCCT = CCT;
    lastState[0] = state[0];
}

void updateFade() {
    for (int i = 0; i < 4; i++) {
        if (fadeStates[i].active) {
            uint32_t currentTimestamp = TIM2->CNT;
            if (currentTimestamp - fadeStates[i].lastTimestamp >= fadeStates[i].delayUs) {
                fadeStates[i].lastTimestamp = currentTimestamp;
                float t = (float)fadeStates[i].currentStep / (float)fadeStates[i].steps;  // Normalized time [0,1]
                float lutValue = getLUTValue(t);    // Smooth LUT interpolation
                float interpolatedBrightness = fadeStates[i].startBrightness + lutValue * (fadeStates[i].endBrightness - fadeStates[i].startBrightness);

                // Apply gamma correction
                float gammaCorrected = powf(interpolatedBrightness, 2.5f);
                setLEDBrightness(fadeStates[i].channel, gammaCorrected);

                // Update currentBrightness for when we get a new fade halfway through
                currentBrightness[fadeStates[i].channel] = interpolatedBrightness;

                fadeStates[i].currentStep++;
                if (fadeStates[i].currentStep >= fadeStates[i].steps) {
                    // Ensure final brightness is set correctly
                    setLEDBrightness(fadeStates[i].channel, powf(fadeStates[i].endBrightness, 2.5f));

                    fadeStates[i].active = 0;
                }
            }
        }
    }
}

void updateFadeCCT() {
    if (fadeStatesCCT.active) {
        uint32_t currentTimestamp = TIM2->CNT;
        if (currentTimestamp - fadeStatesCCT.lastTimestamp >= fadeStatesCCT.delayUs) {
            fadeStatesCCT.lastTimestamp = currentTimestamp;
            float t = (float)fadeStatesCCT.currentStep / (float)fadeStatesCCT.steps;  // Normalized time [0,1]
            float lutValue = getLUTValue(t);    // Smooth LUT interpolation
            float interpolatedBrightness = fadeStatesCCT.startBrightness + lutValue * (fadeStatesCCT.endBrightness - fadeStatesCCT.startBrightness);
            float interpolatedCCT = fadeStatesCCT.startCCTf + lutValue * (fadeStatesCCT.endCCTf - fadeStatesCCT.startCCTf);

            // Apply gamma correction
            float gammaCorrected = powf(interpolatedBrightness, 2.5f);

            setLEDBrightness(CCT_CHANNEL_1, gammaCorrected * (1.0f - interpolatedCCT));
            setLEDBrightness(CCT_CHANNEL_2, gammaCorrected * interpolatedCCT);

            // Update currentBrightness for when we get a new fade halfway through
            currentBrightness[0] = interpolatedBrightness;
            currentCCT = interpolatedCCT;

            fadeStatesCCT.currentStep++;
            if (fadeStatesCCT.currentStep >= fadeStatesCCT.steps) {

                // Ensure final brightness is set correctly
                gammaCorrected = powf(fadeStatesCCT.endBrightness, 2.5f);

                setLEDBrightness(CCT_CHANNEL_1, gammaCorrected * (1.0f - fadeStatesCCT.endCCTf));
                setLEDBrightness(CCT_CHANNEL_2, gammaCorrected * fadeStatesCCT.endCCTf);

                fadeStatesCCT.active = 0;
            }
        }
    }
}
