#ifndef UID_ENCODER_H
#define UID_ENCODER_H

#include <stdint.h>

void base64_encode(const uint8_t* data, uint32_t length, char* encoded);
void GetUID(uint32_t *UID);
void encodeLastThreeBytesOfUID();

extern char encodedUID[6]; // 4 characters + 1 for '/' + 1 for null terminator

#endif // UID_ENCODER_H
