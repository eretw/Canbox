/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.c
  * @brief   This file provides code for the configuration
  *          of the USART instances.
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
/* Includes ------------------------------------------------------------------*/
#include "usart.h"

/* USER CODE BEGIN 0 */
//UART中断缓冲区定义
#define UART4_RX_BUFFER_SIZE 128U

static uint8_t uart4_rx_byte;
static uint8_t uart4_rx_buffer[UART4_RX_BUFFER_SIZE];
static volatile uint16_t uart4_rx_head;
static volatile uint16_t uart4_rx_tail;
static volatile uint32_t uart4_rx_overflow_count;

/* USER CODE END 0 */

UART_HandleTypeDef huart4;

/* UART4 init function */
void MX_UART4_Init(void)
{

  /* USER CODE BEGIN UART4_Init 0 */

  /* USER CODE END UART4_Init 0 */

  /* USER CODE BEGIN UART4_Init 1 */

  /* USER CODE END UART4_Init 1 */
  huart4.Instance = UART4;
  huart4.Init.BaudRate = 115200;
  huart4.Init.WordLength = UART_WORDLENGTH_8B;
  huart4.Init.StopBits = UART_STOPBITS_1;
  huart4.Init.Parity = UART_PARITY_NONE;
  huart4.Init.Mode = UART_MODE_TX_RX;
  huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart4.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART4_Init 2 */

  /* USER CODE END UART4_Init 2 */

}

void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(uartHandle->Instance==UART4)
  {
  /* USER CODE BEGIN UART4_MspInit 0 */

  /* USER CODE END UART4_MspInit 0 */
    /* UART4 clock enable */
    __HAL_RCC_UART4_CLK_ENABLE();

    __HAL_RCC_GPIOC_CLK_ENABLE();
    /**UART4 GPIO Configuration
    PC10     ------> UART4_TX
    PC11     ------> UART4_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* UART4 interrupt Init */
    HAL_NVIC_SetPriority(UART4_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(UART4_IRQn);
  /* USER CODE BEGIN UART4_MspInit 1 */

  /* USER CODE END UART4_MspInit 1 */
  }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef* uartHandle)
{

  if(uartHandle->Instance==UART4)
  {
  /* USER CODE BEGIN UART4_MspDeInit 0 */

  /* USER CODE END UART4_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_UART4_CLK_DISABLE();

    /**UART4 GPIO Configuration
    PC10     ------> UART4_TX
    PC11     ------> UART4_RX
    */
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_10|GPIO_PIN_11);

    /* UART4 interrupt Deinit */
    HAL_NVIC_DisableIRQ(UART4_IRQn);
  /* USER CODE BEGIN UART4_MspDeInit 1 */

  /* USER CODE END UART4_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

void UART4_StartReceive(void)
{
  uart4_rx_head = 0U;
  uart4_rx_tail = 0U;
  uart4_rx_overflow_count = 0U;
  (void)HAL_UART_Receive_IT(&huart4, &uart4_rx_byte, 1U);
}
// 主循环读取
uint8_t UART4_ReadByte(uint8_t *byte)
{
  if ((byte == NULL) || (uart4_rx_head == uart4_rx_tail))
  {
    return 0U;
  }

  *byte = uart4_rx_buffer[uart4_rx_tail];
  uart4_rx_tail = (uint16_t)((uart4_rx_tail + 1U) % UART4_RX_BUFFER_SIZE);
  return 1U;
}

uint32_t UART4_GetRxOverflowCount(void)
{
  return uart4_rx_overflow_count;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  uint16_t next_head;

  if (huart->Instance != UART4)
  {
    return;
  }

  next_head = (uint16_t)((uart4_rx_head + 1U) % UART4_RX_BUFFER_SIZE);
  if (next_head != uart4_rx_tail)
  {
    uart4_rx_buffer[uart4_rx_head] = uart4_rx_byte;
    uart4_rx_head = next_head;
  }
  else
  {
    uart4_rx_overflow_count++;
  }

  (void)HAL_UART_Receive_IT(&huart4, &uart4_rx_byte, 1U);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == UART4)
  {
    (void)HAL_UART_Receive_IT(&huart4, &uart4_rx_byte, 1U);
  }
}

/* USER CODE END 1 */
