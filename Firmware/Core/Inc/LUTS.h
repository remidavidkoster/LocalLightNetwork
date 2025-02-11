#ifndef LUTS_H
#define LUTS_H

#ifdef __cplusplus
extern "C" {
#endif

// Include necessary standard libraries
#include <stdint.h>

// Define constants
#define LUT_SIZE 4096

// Declare functions
float getLUTValue(float t);


#ifdef __cplusplus
}
#endif

#endif // LUTS_H