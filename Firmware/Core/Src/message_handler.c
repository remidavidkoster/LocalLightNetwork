#include "message_handler.h"
#include "usart.h"
#include "system.h"
#include "uid_encoder.h"
#include "NRF24L01P.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "config.h"
#include "fade.h"

char state_topic[] = "/st/";
char command_topic[] = "/ct/";
char brightness_state_topic[] = "/bst/";
char brightness_command_topic[] = "/bct/";
char color_temp_state_topic[] = "/ctst/";
char color_temp_command_topic[] = "/ctct/";

void sendResponseMessage(int channel, const char* topic, int value) {
    char responseBuffer[32];
    sprintf(responseBuffer, "%s%d%s%d", encodedUID, channel, topic, value);
    send((uint8_t*)responseBuffer, strlen(responseBuffer));
    while (isSending());
}

uint16_t handleCommand(const char* remainingMessage, const char* command, int channel, const char* topic, uint16_t* returnValue, int minValue, int maxValue) {
    if (strncmp(remainingMessage, command, strlen(command)) == 0) {
        remainingMessage += strlen(command);  // Skip the command

        // Parse the value
        uint16_t parsedValue = atoi(remainingMessage);

        // Ensure the value is within the valid range
        parsedValue = LIMIT(minValue, parsedValue, maxValue);

        // Generate the response message
        sendResponseMessage(channel + 1, topic, parsedValue);

        // Return the parsed integer value
        *returnValue = parsedValue;

        return 1;
    }
    return 0;
}

// Process incoming MQTT message
void processMessage(char* incomingMessage, char* responseBuffer) {

    // Check if the message starts with "UID/"
    if (strncmp(incomingMessage, encodedUID, strlen(encodedUID)) == 0) {

        // Skip the "UID/" part by moving the pointer forwards
        char* remainingMessage = incomingMessage + strlen(encodedUID); // No need to skip next "/"

        // Determine the channel (1-4)
        int channel = remainingMessage[0] - '1';
        if (channel < 0 || channel > 3) return; // Invalid channel

        // Skip the channel number by incrementing the pointer
        remainingMessage++;

        // If we got and handled a brightness command
        handleCommand(remainingMessage, brightness_command_topic, channel, brightness_state_topic, &brightness[channel], 0, 255);

        // Handle state command
        handleCommand(remainingMessage, command_topic, channel, state_topic, &state[channel], 0, 1);

        // Handle color temperature command
        handleCommand(remainingMessage, color_temp_command_topic, channel, color_temp_state_topic, &CCT, MIN_CCT, MAX_CCT);
    }
}
