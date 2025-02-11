#ifndef SYSTEM_H
#define SYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"

void SystemClock_Config(void);
void Error_Handler(void);

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line);
#endif /* USE_FULL_ASSERT */

#ifdef __cplusplus
}
#endif

#endif // SYSTEM_H
