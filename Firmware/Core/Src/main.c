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



const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
char encodedUID[6]; // 4 characters + 1 for '/' + 1 for null terminator

void base64_encode(const uint8_t* data, size_t length, char* encoded) {
	size_t i, j;
	for (i = 0, j = 0; i < length;) {
		uint32_t octet_a = i < length ? data[i++] : 0;
		uint32_t octet_b = i < length ? data[i++] : 0;
		uint32_t octet_c = i < length ? data[i++] : 0;

		uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

		encoded[j++] = base64_chars[(triple >> 18) & 0x3F];
		encoded[j++] = base64_chars[(triple >> 12) & 0x3F];
		encoded[j++] = base64_chars[(triple >> 6) & 0x3F];
		encoded[j++] = base64_chars[triple & 0x3F];
	}

	// Add '/' and null terminator
	encoded[j++] = '/';
	encoded[j] = '\0';
}

void GetUID(uint32_t *UID) {
	UID[0] = HAL_GetUIDw0();
	UID[1] = HAL_GetUIDw1();
	UID[2] = HAL_GetUIDw2();
}

void encodeLastThreeBytesOfUID() {
	uint32_t UID[3];
	GetUID(UID);

	// Extract the last three bytes from HAL_GetUIDw0()
	uint8_t lastThreeBytes[3];
	lastThreeBytes[0] = UID[0] & 0xFF;
	lastThreeBytes[1] = (UID[0] >> 8) & 0xFF;
	lastThreeBytes[2] = (UID[0] >> 16) & 0xFF;

	// Encode the last three bytes using base64
	base64_encode(lastThreeBytes, 3, encodedUID);
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

// Highest value that is still fully off
#define PWM_MIN 0x0022

// Lowest value that is fully on
#define PWM_MAX 0xFFF8

void PWM_Set(uint8_t ch, uint16_t v) {
	if (ch == 0) HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].CMP3xR = v;
	if (ch == 1) HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].CMP1xR = v;
	if (ch == 2) HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_D].CMP3xR = v;
	if (ch == 3) HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_D].CMP1xR = v;
}

char message[32] = {0};
char responseBuffer[32] = {0};

uint8_t state[4] = {1, 1, 1, 1};
uint8_t lastState[4] = {1, 1, 1, 1};
float brightness[4] = {0.5f, 0.5f, 0.5f, 0.5f};
float lastBrightness[4] = {0, 0, 0, 0};
float currentBrightness[4] = {0, 0, 0, 0};

char command_topic[] = "/set/";
char brightness_command_topic[] = "/b/set/";

char state_topic[] = "/state/";
char brightness_state_topic[] = "/b/state/";

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

FadeState fadeStates[4] = {0};

void processMessage(char* incomingMessage, char* responseBuffer) {
	// Check if the message starts with the base topic "L4N1/"
	if (strncmp(incomingMessage, encodedUID, strlen(encodedUID)) == 0) {
		// Skip the "L4N1/" part by moving the pointer forward
		char* remainingMessage = incomingMessage + strlen(encodedUID); // No need to skip next "/"

		// Determine the channel (1-4)
		int channel = remainingMessage[0] - '1';
		if (channel < 0 || channel > 3) return; // Invalid channel

		remainingMessage++; // Skip the channel number

		// Check if the remaining message matches "/b/set" or "/set"
		if (strncmp(remainingMessage, brightness_command_topic, strlen(brightness_command_topic)) == 0) {
			// It's a /b/set message, parse the value after "/b/set/"
			remainingMessage += strlen(brightness_command_topic);  // Skip "/b/set"

			// Convert to integer (handle "173" case)
			brightness[channel] = atoi(remainingMessage) / 255.0f;

			// Generate the response message: "L4N1/1/b/state/173"
			sprintf(responseBuffer, "%s%d%s%d", encodedUID, channel + 1, brightness_state_topic, (int)roundf(brightness[channel] * 255.0f));
		} else if (strncmp(remainingMessage, command_topic, strlen(command_topic)) == 0) {
			// It's a /set message, parse the state value
			remainingMessage += strlen(command_topic);  // Skip "/set"

			// Convert to integer
			int parsedState = atoi(remainingMessage);

			// Only allow valid states (0 or 1)
			if (parsedState == 0 || parsedState == 1) {
				state[channel] = parsedState; // Update global state variable

				// Generate the response message: "L4N1/1/state/0" or "L4N1/1/state/1"
				sprintf(responseBuffer, "%s%d%s%d", encodedUID, channel + 1, state_topic, state[channel]);
			}
		}
		send((uint8_t*)responseBuffer, strlen(responseBuffer));
		while (isSending());
	}
}

void setLEDBrightness(uint8_t channel, float value) {
	PWM_Set(channel, PWM_MIN + (int)roundf(value * (PWM_MAX - PWM_MIN)));
}

void startFadeToNewBrightness(uint8_t channel, int steps, int delayUs) {
	fadeStates[channel].channel = channel;
	fadeStates[channel].steps = steps;
	fadeStates[channel].currentStep = 0;
	fadeStates[channel].delayUs = delayUs;
	fadeStates[channel].lastTimestamp = TIM2->CNT;
	fadeStates[channel].startBrightness = currentBrightness[channel];
	fadeStates[channel].endBrightness = state[channel] ? brightness[channel] : 0.0f;
	fadeStates[channel].active = 1;
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
                if (fadeStates[i].currentStep > fadeStates[i].steps) {
                    // Ensure final brightness is set correctly
                    setLEDBrightness(fadeStates[i].channel, powf(fadeStates[i].endBrightness, 2.5f));

                    // Update lastBrightness and lastState at the end of the transition
                    lastBrightness[fadeStates[i].channel] = brightness[fadeStates[i].channel];
                    lastState[fadeStates[i].channel] = state[fadeStates[i].channel];

                    fadeStates[i].active = 0;
                }
            }
        }
    }
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

	encodeLastThreeBytesOfUID();


	for (int i = 0; i < 4; i++) {
		startFadeToNewBrightness(i, 250, 2000);
	}

	while (1) {
		if (dataReady()) {
			getData((uint8_t*)message);

			processMessage(message, responseBuffer);

			for (int i = 0; i < 4; i++) {
				if (state[i] != lastState[i] || brightness[i] != lastBrightness[i]) {
					startFadeToNewBrightness(i, 250, 2000);
				}
			}
		}

		updateFade();
	}
}
