/**
  ******************************************************************************
  * @file    Templates/Src/stm32f4xx_hal_msp.c
  * @author  MCD Application Team
  * @version V1.2.5
  * @date    17-February-2017
  * @brief   HAL MSP module.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

void HAL_MspInit(void)
{
}

void HAL_MspDeInit(void)
{
}

void HAL_ADC_MspInit(ADC_HandleTypeDef* hadc)
{
  GPIO_InitTypeDef GPIO_Initure;
  
  __HAL_RCC_ADC1_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_DMA2_CLK_ENABLE();
  
  GPIO_Initure.Pin = GPIO_PIN_5;
  GPIO_Initure.Mode = GPIO_MODE_ANALOG;
  GPIO_Initure.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_Initure);
  
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
}

void HAL_DAC_MspInit(DAC_HandleTypeDef* hdac)
{
  GPIO_InitTypeDef GPIO_Initure;
  
  __HAL_RCC_DAC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  
  GPIO_Initure.Pin = GPIO_PIN_4;
  GPIO_Initure.Mode = GPIO_MODE_ANALOG;
  GPIO_Initure.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_Initure);
}

void HAL_UART_MspInit(UART_HandleTypeDef* huart)
{
  GPIO_InitTypeDef GPIO_Initure;
  
  __HAL_RCC_USART1_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  
  GPIO_Initure.Pin = GPIO_PIN_9;
  GPIO_Initure.Mode = GPIO_MODE_AF_PP;
  GPIO_Initure.Pull = GPIO_PULLUP;
  GPIO_Initure.Speed = GPIO_SPEED_FAST;
  GPIO_Initure.Alternate = GPIO_AF7_USART1;
  HAL_GPIO_Init(GPIOA, &GPIO_Initure);
  
  GPIO_Initure.Pin = GPIO_PIN_10;
  HAL_GPIO_Init(GPIOA, &GPIO_Initure);
}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef* htim)
{
  if(htim->Instance == TIM3 || htim->Instance == TIM6)
  {
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_TIM6_CLK_ENABLE();
  }
}