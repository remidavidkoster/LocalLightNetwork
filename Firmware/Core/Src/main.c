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
#include "fade.h"
#include "message_handler.h"
#include "config.h"


#include <math.h>
#include <stdio.h>

char message[32] = {0};
char responseBuffer[32] = {0};

uint8_t mode = MODE_CCT; // Default mode

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
            sendResponseMessage(channel + 1, state_topic, state[channel]);
            HAL_Delay(10);
            sendResponseMessage(channel + 1, brightness_state_topic, brightness[channel]);
            HAL_Delay(10);
        }
        for (int i = 0; i < 4; i++) {
            startFadeToNewBrightness(i, 250, 2000);
        }
    }

    else if (mode == MODE_CCT) {
        sendResponseMessage(1, state_topic, state[0]);
        HAL_Delay(10);
        sendResponseMessage(1, brightness_state_topic, brightness[0]);
        HAL_Delay(10);
        sendResponseMessage(1, color_temp_state_topic, CCT);
        startFadeToNewBrightnessCCT(250, 2000);
    }

    while (1) {
        if (dataReady()) {
            getData((uint8_t*)message);

            processMessage(message, responseBuffer);

            if (mode == MODE_4_CHANNELS) {

            	// Check if any set points changed
                for (int i = 0; i < 4; i++) {
                    if (state[i] != lastState[i] || brightness[i] != lastBrightness[i]) {
                        startFadeToNewBrightness(i, 250, 2000);
                    }
                }
            }

            else if (mode == MODE_CCT) {

            	// Check if any set points changed
                if (state[0] != lastState[0] || brightness[0] != lastBrightness[0] || CCT != lastCCT) {
                    startFadeToNewBrightnessCCT(250, 2000);
                }
            }
        }

        if (mode == MODE_4_CHANNELS) updateFade();
        else if (mode == MODE_CCT) updateFadeCCT();
    }
}
