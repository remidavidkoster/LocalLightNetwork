#include "main.h"
#include "hrtim.h"
#include "tim.h"
#include "usart.h"
#include "usb_device.h"
#include "gpio.h"
#include "NRF24L01P.h"
#include "system.h"
#include "LUTs.h"
#include "uid_encoder.h"
#include "pwm_utils.h"

#define LIMIT(min, x, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

#include <math.h>
#include <stdio.h>

#define MODE_4_CHANNELS 0
#define MODE_CCT 1

#define CCT_CHANNEL_1 0
#define CCT_CHANNEL_2 1

#define MIN_CCT 200
#define MAX_CCT 250

char message[32] = {0};
char responseBuffer[32] = {0};

uint8_t state[4] = {1, 1, 1, 1};
uint8_t lastState[4] = {1, 1, 1, 1};
float brightness[4] = {0.5f, 0.5f, 0.5f, 0.5f};
float lastBrightness[4] = {0, 0, 0, 0};
float currentBrightness[4] = {0, 0, 0, 0};


uint16_t CCT = 222;
uint16_t lastCCT = 222;
uint16_t currentCCT = 222;




char state_topic[] = "/st/";
char command_topic[] = "/ct/";

char brightness_state_topic[] = "/bst/";
char brightness_command_topic[] = "/bct/";

char color_temp_state_topic[] = "/ctst/";
char color_temp_command_topic[] = "/ctct/";

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

FadeStateCCT fadeStatesCCT = {0};



uint8_t mode = MODE_CCT; // Default mode


void processMessage(char* incomingMessage, char* responseBuffer) {
	// Check if the message starts with the base topic "L4N1/"
	if (strncmp(incomingMessage, encodedUID, strlen(encodedUID)) == 0) {
		// Skip the "L4N1/" part by moving the pointer forward
		char* remainingMessage = incomingMessage + strlen(encodedUID); // No need to skip next "/"

		// Determine the channel (1-4)
		int channel = remainingMessage[0] - '1';
		if (channel < 0 || channel > 3) return; // Invalid channel

		remainingMessage++; // Skip the channel number

		// Check if the remaining message matches "/bct/" or "/ct/"
		if (strncmp(remainingMessage, brightness_command_topic, strlen(brightness_command_topic)) == 0) {
			// It's a /bct/ message, parse the value after "/bct/"
			remainingMessage += strlen(brightness_command_topic);  // Skip "/bct/"

			// Convert to integer (handle "173" case)
			brightness[channel] = atoi(remainingMessage) / 255.0f;

			// Generate the response message: "base_topic1/bst/173"
			sprintf(responseBuffer, "%s%d%s%d", encodedUID, channel + 1, brightness_state_topic, (int)roundf(brightness[channel] * 255.0f));
		} else if (strncmp(remainingMessage, command_topic, strlen(command_topic)) == 0) {
			// It's a /ct/ message, parse the state value
			remainingMessage += strlen(command_topic);  // Skip "/ct/"

			// Convert to integer
			int parsedState = atoi(remainingMessage);

			// Only allow valid states (0 or 1)
			if (parsedState == 0 || parsedState == 1) {
				state[channel] = parsedState; // Update global state variable

				// Generate the response message: "base_topic1/st/0" or "base_topic1/st/1"
				sprintf(responseBuffer, "%s%d%s%d", encodedUID, channel + 1, state_topic, state[channel]);
			}
		} else if (strncmp(remainingMessage, color_temp_command_topic, strlen(color_temp_command_topic)) == 0) {
			// It's a /ctct/ message, parse the color temperature value
			remainingMessage += strlen(color_temp_command_topic);  // Skip "/ctct/"

			// Convert to integer
			int parsedColorTemp = atoi(remainingMessage);

			// Ensure the color temperature is within the valid range
			parsedColorTemp = LIMIT(MIN_CCT, parsedColorTemp, MAX_CCT);

			// Update the color temperature
			CCT = parsedColorTemp;

			// Generate the response message: "base_topic1/ctst/250"
			sprintf(responseBuffer, "%s%d%s%d", encodedUID, channel + 1, color_temp_state_topic, CCT);
		}
		send((uint8_t*)responseBuffer, strlen(responseBuffer));
		while (isSending());
	}
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
	fadeStatesCCT.endBrightness = state[0] ? brightness[0] : 0.0f;
	fadeStatesCCT.startCCTf = (float)(lastCCT - MIN_CCT) / (MAX_CCT - MIN_CCT);
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


void sendResponseMessage(const char* uid, int channel, const char* topic, int value) {
    char responseBuffer[32];
    sprintf(responseBuffer, "%s%d%s%d", uid, channel, topic, value);
    send((uint8_t*)responseBuffer, strlen(responseBuffer));
    while (isSending());
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

	flushTx();
	flushRx();

	encodeLastThreeBytesOfUID();

	if (mode == MODE_4_CHANNELS) {
		for (int channel = 0; channel < 4; channel++) {
			sendResponseMessage(encodedUID, channel + 1, state_topic, state[channel]);
			HAL_Delay(10);
		}

		for (int channel = 0; channel < 4; channel++) {
			sendResponseMessage(encodedUID, channel + 1, brightness_state_topic, (int)roundf(brightness[channel] * 255.0f));
			HAL_Delay(10);
		}
	}

	else if (mode == MODE_CCT){
		sendResponseMessage(encodedUID, 1, state_topic, state[channel]);
		HAL_Delay(10);
		sendResponseMessage(encodedUID, 1, brightness_state_topic, (int)roundf(brightness[channel] * 255.0f));
		HAL_Delay(10);
		sendResponseMessage(encodedUID, 1, color_temp_state_topic, CCT);
	}

	if (mode == MODE_4_CHANNELS){
		for (int i = 0; i < 4; i++) {
			startFadeToNewBrightness(i, 250, 2000);
		}
	}
	else if (mode == MODE_CCT){
		startFadeToNewBrightnessCCT(250, 2000);
	}

	while (1) {
		if (dataReady()) {
			getData((uint8_t*)message);

			processMessage(message, responseBuffer);


			if (mode == MODE_4_CHANNELS) {
				for (int i = 0; i < 4; i++) {
					if (state[i] != lastState[i] || brightness[i] != lastBrightness[i]) {
						startFadeToNewBrightness(i, 250, 2000);
					}
				}
			} else if (mode == MODE_CCT) {
				if (state[0] != lastState[0] || brightness[0] != lastBrightness[0] || CCT != lastCCT) {
					startFadeToNewBrightnessCCT(250, 2000);
				}
			}
		}

		if (mode == MODE_4_CHANNELS) updateFade();
		else if (mode == MODE_CCT) updateFadeCCT();
	}
}
