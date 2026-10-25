#ifndef __CAN_H
#define __CAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define CAN1_RX_QUEUE_SIZE 16U

typedef struct
{
  uint32_t id;
  uint32_t timestamp_ms;
  uint8_t data[8];
  uint8_t dlc;
  uint8_t is_extended;
  uint8_t is_remote;
} CAN1_Frame;

typedef struct
{
  uint32_t tx_queued;
  uint32_t tx_ok;
  uint32_t tx_failed;
  uint32_t rx_received;
  uint32_t rx_dropped;
  uint32_t fifo_overrun;
  uint32_t error_irqs;
  uint32_t last_esr;
} CAN1_Statistics;

/*
 * CAN1 bit timing with PCLK1 = 36 MHz:
 *   Prescaler = 6, 1 bit = 12 Tq, BS1 = 9 Tq, BS2 = 2 Tq, SJW = 1 Tq
 *   Baud rate = 36 MHz / (6 * 12) = 500 kbit/s
 *   Sample point = (1 + 9) / 12 = 83.33%
 */
uint8_t CAN1_Init(void);
uint8_t CAN1_SendExtended(uint32_t extended_id, const uint8_t *data, uint8_t dlc);
uint8_t CAN1_Read(CAN1_Frame *frame);
uint8_t CAN1_IsInitialized(void);
const char *CAN1_GetStateName(void);
uint8_t CAN1_GetTEC(void);
uint8_t CAN1_GetREC(void);
uint8_t CAN1_GetLastErrorCode(void);
void CAN1_GetStatistics(CAN1_Statistics *statistics);
void CAN1_ClearStatistics(void);

/* Called by stm32f1xx_it.c. Do not call these functions from application code. */
void CAN1_TxIRQHandler(void);
void CAN1_Rx0IRQHandler(void);
void CAN1_SceIRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* __CAN_H */
