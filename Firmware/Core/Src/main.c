#include "main.h"
#include "hrtim.h"
#include "tim.h"
#include "usart.h"
#include "usb_device.h"
#include "gpio.h"
#include "NRF24L01P.h"
#include "system.h"
#include "LUTs.h"

#define LIMIT(min, x, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

#include <math.h>

#define ABS(x) ((x) < 0 ? -(x) : (x))

#define PI 3.14159265359f

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

// Highest value that is still fully off
#define PWM_MIN 0x0022

// Lowest value that is fully on
#define PWM_MAX 0xFFF8

void SystemClock_Config(void);

void PWM_Set(uint8_t ch, uint16_t v) {
    if (ch == 1 || !ch) HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].CMP1xR = v;
    if (ch == 2 || !ch) HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].CMP3xR = v;
    if (ch == 3 || !ch) HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_D].CMP1xR = v;
    if (ch == 4 || !ch) HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_D].CMP3xR = v;
}

char message[32] = {0};
char responseBuffer[32] = {0};

uint8_t state = 1;
uint8_t lastState = 1;
float brightness = 0.5f;
float lastBrightness = 0;

char base_topic[] = "L4N1/1";

char command_topic[] = "/set/";
char brightness_command_topic[] = "/b/set/";

char state_topic[] = "/state/";
char brightness_state_topic[] = "/b/state/";

void processMessage(char* incomingMessage, char* responseBuffer) {
    // Check if the message starts with the base topic "L4N1/1"
    if (strncmp(incomingMessage, base_topic, strlen(base_topic)) == 0) {
        // Skip the "L4N1/1" part by moving the pointer forward
        char* remainingMessage = incomingMessage + strlen(base_topic); // No need to skip next "/"

        // Check if the remaining message matches "/b/set" or "/set"
        if (strncmp(remainingMessage, brightness_command_topic, strlen(brightness_command_topic)) == 0) {
            // It's a /b/set message, parse the value after "/b/set/"
            remainingMessage += strlen(brightness_command_topic);  // Skip "/b/set"

            // Convert to integer (handle "173" case)
            brightness = atoi(remainingMessage) / 255.0f;

            // Generate the response message: "L4N1/1/b/state/173"
            sprintf(responseBuffer, "%s%s%d", base_topic, brightness_state_topic, (int)roundf(brightness * 255.0f));
        } else if (strncmp(remainingMessage, command_topic, strlen(command_topic)) == 0) {
            // It's a /set message, parse the state value
            remainingMessage += strlen(command_topic);  // Skip "/set"

            // Convert to integer
            int parsedState = atoi(remainingMessage);

            // Only allow valid states (0 or 1)
            if (parsedState == 0 || parsedState == 1) {
                state = parsedState; // Update global state variable

                // Generate the response message: "L4N1/1/state/0" or "L4N1/1/state/1"
                sprintf(responseBuffer, "%s%s%d", base_topic, state_topic, state);
            }
        }
    }
}

void setLEDBrightness(float value) {
    PWM_Set(0, PWM_MIN + (int)roundf(value * (PWM_MAX - PWM_MIN)));
}

void fadeToNewBrightness(int steps, int delayUs) {
    float startBrightness = lastState ? lastBrightness : 0.0f;
    float endBrightness = state ? brightness : 0.0f;

    for (int i = 0; i <= steps; i++) {
        float t = (float)i / (float)steps;  // Normalized time [0,1]
        float lutValue = getLUTValue(t);    // Smooth LUT interpolation
        float interpolatedBrightness = startBrightness + lutValue * (endBrightness - startBrightness);

        // Apply gamma correction
        float gammaCorrected = powf(interpolatedBrightness, 2.5f);
        setLEDBrightness(gammaCorrected);

        // Small delay
        uint32_t timestamp = TIM2->CNT;
        while (TIM2->CNT - timestamp < delayUs);
    }

    // Ensure final brightness is set correctly
    setLEDBrightness(powf(endBrightness, 2.5f));

    // Update lastBrightness and lastState at the end of the transition
    lastBrightness = brightness;
    lastState = state;
}

int main(void) {
    // Reset of all peripherals, Initializes the Flash interface and the Systick.
    HAL_Init();

    // Configure the system clock
    SystemClock_Config();

    // Initialize all configured peripherals
    MX_GPIO_Init();
    MX_HRTIM1_Init();
    //  MX_USART2_UART_Init();
    MX_USB_Device_Init();
    MX_TIM17_Init();
    MX_TIM2_Init();

    HAL_TIM_Base_Start(&htim17);
    HAL_TIM_Base_Start(&htim2);

    HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TA1);  // Enable the generation of the waveform signal on the designated output
    HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TA2);  // Enable the generation of the waveform signal on the designated output
    HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TD1);  // Enable the generation of the waveform signal on the designated output
    HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TD2);  // Enable the generation of the waveform signal on the designated output
    HAL_HRTIM_WaveformCountStart(&hhrtim1, HRTIM_TIMERID_TIMER_A);  // Start the counter of the Timer A operating in waveform mode
    HAL_HRTIM_WaveformCountStart(&hhrtim1, HRTIM_TIMERID_TIMER_D);  // Start the counter of the Timer A operating in waveform mode

    // NRF24L01P init
    init();
    setAddressWidth(3);
    setRADDR((uint8_t *)"L4N");
    setTADDR((uint8_t *)"L4N");

    channel = 100;
    config(1);

    fadeToNewBrightness(250, 2000);

    while (1) {
        if (dataReady()) {
            getData((uint8_t*)message);

            processMessage(message, responseBuffer);

            send((uint8_t*)responseBuffer, strlen(responseBuffer));
            while (isSending());

            if (state != lastState || brightness != lastBrightness) {
                fadeToNewBrightness(250, 2000);
            }
        }
    }
}
