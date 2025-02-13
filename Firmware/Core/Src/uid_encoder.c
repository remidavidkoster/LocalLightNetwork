#include "uid_encoder.h"
#include "stm32g4xx_hal.h"

const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
char encodedUID[6]; // 4 characters + 1 for '/' + 1 for null terminator

void base64_encode(const uint8_t* data, uint32_t length, char* encoded) {
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
