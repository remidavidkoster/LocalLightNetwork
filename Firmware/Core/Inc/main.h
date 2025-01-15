/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define NRF_MISO_Pin GPIO_PIN_13
#define NRF_MISO_GPIO_Port GPIOC
#define NRF_MOSI_Pin GPIO_PIN_14
#define NRF_MOSI_GPIO_Port GPIOC
#define NRF_CLK_Pin GPIO_PIN_15
#define NRF_CLK_GPIO_Port GPIOC
#define NRF_SS_Pin GPIO_PIN_0
#define NRF_SS_GPIO_Port GPIOF
#define NRF_CE_Pin GPIO_PIN_1
#define NRF_CE_GPIO_Port GPIOF
#define BUT4_Pin GPIO_PIN_11
#define BUT4_GPIO_Port GPIOB
#define BUT3_Pin GPIO_PIN_12
#define BUT3_GPIO_Port GPIOB
#define BUT2_Pin GPIO_PIN_13
#define BUT2_GPIO_Port GPIOB
#define BUT1_Pin GPIO_PIN_10
#define BUT1_GPIO_Port GPIOA
#define BUT0_Pin GPIO_PIN_15
#define BUT0_GPIO_Port GPIOA
#define NRF_IRQ_Pin GPIO_PIN_5
#define NRF_IRQ_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
