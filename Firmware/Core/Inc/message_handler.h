#ifndef MESSAGE_HANDLER_H
#define MESSAGE_HANDLER_H

#include <stdint.h>

extern char encodedUID[];
extern char state_topic[];
extern char command_topic[];
extern char brightness_state_topic[];
extern char brightness_command_topic[];
extern char color_temp_state_topic[];
extern char color_temp_command_topic[];

void processMessage(char* incomingMessage, char* responseBuffer);
void sendResponseMessage(int channel, const char* topic, int value);
uint16_t handleCommand(const char* remainingMessage, const char* command, int channel, const char* topic, uint16_t* returnValue, int minValue, int maxValue);

#endif // MESSAGE_HANDLER_H