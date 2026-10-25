/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "main.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "can.h"
#include "lcd12864.h"
#include "app_time.h"
#include "adc_keys.h"
#include "key_service.h"
#include "dbc_codec.h"
#include "stdio.h"
#include "string.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)

PUTCHAR_PROTOTYPE
{
    (void)HAL_UART_Transmit(&huart4, (uint8_t *)&ch, 1U, 100U);
    return ch;
}
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define SHELL_LINE_SIZE 64U
#define FW_VERSION      "V1.6.0"
#define DBC_CANBOX_PACKET_COUNT 6U
#define MENU_PAGE_COUNT          3U
#define LCD_ROW_REFRESH_MS       100U // LCD每行刷新周期100ms

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

static void Shell_PrintPrompt(void);
static void Shell_PrintHelp(void);
static void Shell_Execute(char *line);
static void Shell_Process(void);
static uint8_t Shell_ParseHex(const char **text, uint32_t *value);
static uint8_t Shell_ParseDecimal(const char **text, uint32_t *value);
static void Shell_CanSend(const char *arguments);
static void Shell_LcdShow(const char *arguments);
static void CAN_PrintStatus(void);
static void CAN_ProcessReceived(void);
static void Key_ProcessEvents(void);
static void DBC_PrintFrame(const DBC_Frame *frame);
static void DBC_PrintInfo(void);
static void DBC_PrintSignals(void);
static void Shell_DbcEncode(const char *arguments);
static void Shell_DbcSend(const char *arguments);
static void Shell_DbcSet(const char *arguments);
static void Shell_DbcDecode(const char *arguments);
static void Shell_DbcChecksum(const char *arguments);
static uint8_t DBC_SendCanBoxPacket(uint8_t packet_number, uint32_t value,
                                    uint8_t verbose);
static void DBC_TxTask(uint32_t now_ms);
static void DBC_TxPrintStatus(void);
static void Menu_HandleKeyEvent(const KeyEvent *event);
static void Menu_LcdTask(uint32_t now_ms);
static void Menu_RenderRow(uint8_t row, uint32_t now_ms);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static uint8_t can_rx_monitor_enabled = 1U;
static uint8_t key_monitor_enabled = 1U;
static uint32_t key_sample_last_ms;
static uint32_t heartbeat_last_ms;
static DBC_PcSignals dbc_pc_signals;
static DBC_Packet1Receiver dbc_packet1_receiver;
static DBC_Status dbc_packet1_last_status = DBC_STATUS_NOT_HANDLED;
static uint8_t dbc_tx_enabled;
static uint32_t dbc_canbox_values[DBC_CANBOX_PACKET_COUNT];
static uint32_t dbc_tx_last_ms[DBC_CANBOX_PACKET_COUNT];
static uint32_t dbc_tx_queued;
static uint32_t dbc_tx_busy;
// 报文发送目标周期
static const uint32_t dbc_tx_period_ms[DBC_CANBOX_PACKET_COUNT] =
{
  500U, 1000U, 0U, 0U, 1000U, 300U
};
static const uint8_t dbc_canbox_max[DBC_CANBOX_PACKET_COUNT] =
{
  3U, 3U, 15U, 7U, 3U, 3U
};
static const char *const dbc_canbox_short_name[DBC_CANBOX_PACKET_COUNT] =
{
  "DevIx", "UnitSys", "CallLog", "MutePrio", "AudioMute", "KeyACC"
};
static const uint32_t dbc_pc_timeout_ms[DBC_PC_SIGNAL_COUNT] =
{
  200U, 300U, 500U, 2000U, 5000U
};
static uint8_t menu_page;
static uint8_t menu_selected_packet;
static uint8_t menu_refresh_row;
static uint32_t menu_refresh_last_ms;

static void Shell_PrintPrompt(void)
{
  printf("canbox> ");
}

static void Shell_PrintHelp(void)
{
  printf("Supported commands:\r\n");
  printf("  help, h   - show this command list\r\n");
  printf("  checkVer  - show firmware version\r\n");
  printf("  canstat   - show CAN status\r\n");
  printf("  cansend <ext_id> [00..FF] - send an extended CAN frame\r\n");
  printf("  canrx on|off - enable or disable received-frame printing\r\n");
  printf("  canclear  - clear CAN software counters\r\n");
  printf("  lcdtest   - show four-line LCD test pattern\r\n");
  printf("  lcdclear  - clear LCD text\r\n");
  printf("  lcdinit   - resend LCD initialization sequence\r\n");
  printf("  lcdshow <row> <col> <text> - display ASCII text\r\n");
  printf("  keyadc    - show three keypad ADC values and raw key\r\n");
  printf("  keystat   - show debounced key and press duration\r\n");
  printf("  keymon on|off - enable or disable key event printing\r\n");
  printf("  dbcinfo   - show DBC message IDs and signal layout\r\n");
  printf("  dbcencode <packet 1..6> <value> - encode CanBox frame\r\n");
  printf("  dbcsend <packet 1..6> <value> - encode and send CanBox frame\r\n");
  printf("  dbcset <packet 1..6> <value> - set periodic/request signal value\r\n");
  printf("  dbctx on|off|stat - control periodic CanBox transmission\r\n");
  printf("  dbcdecode <packet 2..6> <8 bytes> - decode PC frame\r\n");
  printf("  dbccrc <7 bytes> - calculate PC_Packet_1 checksum\r\n");
  printf("  dbcshow   - show received PC signal state\r\n");
  printf("  dbctest   - run DBC codec self-test\r\n");
  printf("  reset     - reset MCU\r\n");
}

static uint8_t Shell_ParseHex(const char **text, uint32_t *value)
{
  const char *cursor = *text;
  uint32_t parsed = 0U;
  uint8_t digits = 0U;

  while (*cursor == ' ')
  {
    cursor++;
  }
  if ((cursor[0] == '0') && ((cursor[1] == 'x') || (cursor[1] == 'X')))
  {
    cursor += 2;
  }

  while (*cursor != '\0')
  {
    uint8_t nibble;

    if ((*cursor >= '0') && (*cursor <= '9'))
    {
      nibble = (uint8_t)(*cursor - '0');
    }
    else if ((*cursor >= 'a') && (*cursor <= 'f'))
    {
      nibble = (uint8_t)(*cursor - 'a' + 10);
    }
    else if ((*cursor >= 'A') && (*cursor <= 'F'))
    {
      nibble = (uint8_t)(*cursor - 'A' + 10);
    }
    else
    {
      break;
    }

    if (digits >= 8U)
    {
      return 0U;
    }
    parsed = (parsed << 4) | nibble;
    digits++;
    cursor++;
  }

  if ((digits == 0U) || ((*cursor != '\0') && (*cursor != ' ')))
  {
    return 0U;
  }

  *value = parsed;
  *text = cursor;
  return 1U;
}

static uint8_t Shell_ParseDecimal(const char **text, uint32_t *value)
{
  const char *cursor = *text;
  uint32_t parsed = 0U;
  uint8_t digits = 0U;

  while (*cursor == ' ')
  {
    cursor++;
  }

  while ((*cursor >= '0') && (*cursor <= '9'))
  {
    if (parsed > 1000U)
    {
      return 0U;
    }
    parsed = (parsed * 10U) + (uint32_t)(*cursor - '0');
    digits++;
    cursor++;
  }

  if ((digits == 0U) || ((*cursor != '\0') && (*cursor != ' ')))
  {
    return 0U;
  }

  *value = parsed;
  *text = cursor;
  return 1U;
}

static void Shell_CanSend(const char *arguments)
{
  uint32_t id;
  uint32_t value;
  uint8_t data[8];
  uint8_t dlc = 0U;

  if ((Shell_ParseHex(&arguments, &id) == 0U) || (id > 0x1FFFFFFFUL))
  {
    printf("ERR usage: cansend <0..1FFFFFFF> [00..FF, max 8]\r\n");
    return;
  }

  while (1)
  {
    while (*arguments == ' ')
    {
      arguments++;
    }
    if (*arguments == '\0')
    {
      break;
    }
    if ((dlc >= 8U) || (Shell_ParseHex(&arguments, &value) == 0U) || (value > 0xFFU))
    {
      printf("ERR usage: cansend <0..1FFFFFFF> [00..FF, max 8]\r\n");
      return;
    }
    data[dlc++] = (uint8_t)value;
  }

  if (CAN1_SendExtended(id, data, dlc) != 0U)
  {
    printf("CAN TX queued: ID=%08lX DLC=%u\r\n", (unsigned long)id, dlc);
  }
  else
  {
    printf("ERR CAN TX failed: check canstat (mailboxes may be busy)\r\n");
  }
}

static void Shell_LcdShow(const char *arguments)
{
  uint32_t row;
  uint32_t col;
  size_t length;

  if ((Shell_ParseDecimal(&arguments, &row) == 0U) ||
      (Shell_ParseDecimal(&arguments, &col) == 0U) ||
      (row >= LCD12864_ROWS) || (col >= LCD12864_COLS))
  {
    printf("ERR usage: lcdshow <row 0..3> <col 0..15> <ASCII text>\r\n");
    return;
  }

  while (*arguments == ' ')
  {
    arguments++;
  }
  if (*arguments == '\0')
  {
    printf("ERR lcdshow text is empty\r\n");
    return;
  }

  length = strlen(arguments);
  if (length > 255U)
  {
    length = 255U;
  }
  lcd12864_display((uint8_t)row, (uint8_t)col,
                   (const uint8_t *)arguments, (uint8_t)length);
  printf("LCD updated: row=%lu col=%lu\r\n",
         (unsigned long)row, (unsigned long)col);
}

static void DBC_PrintFrame(const DBC_Frame *frame)
{
  uint8_t index;

  if (frame == NULL)
  {
    return;
  }
  printf("ID=%08lX DLC=%u DATA=", (unsigned long)frame->id, frame->dlc);
  for (index = 0U; index < frame->dlc; index++)
  {
    printf("%02X%s", frame->data[index],
           ((index + 1U) < frame->dlc) ? " " : "");
  }
  printf("\r\n");
}

static void DBC_PrintInfo(void)
{
  printf("PC -> CanBox (extended, DLC=8):\r\n");
  printf("  PC1 0CFFD221 500ms multi-frame + checksum\r\n");
  printf("  PC2 0CFFD222 20ms   Data@byte1\r\n");
  printf("  PC3 0CFFD223 50ms   Data@byte2\r\n");
  printf("  PC4 0CFFD224 100ms  Data@byte3\r\n");
  printf("  PC5 0CFFD225 500ms  Data@byte4\r\n");
  printf("  PC6 0CFFD226 1500ms Data@byte5\r\n");
  printf("CanBox -> PC (extended, DLC=8):\r\n");
  printf("  CB1 18FF2517 500ms DevIx byte1.bit1 len2\r\n");
  printf("  CB2 18FF2518 1000ms UnitSysSet byte3.bit3 len2\r\n");
  printf("  CB3 18FF2519 request CallFromLogIx byte5.bit1 len4\r\n");
  printf("  CB4 18FF251A request MutePrioReq byte6.bit1 len3\r\n");
  printf("  CB5 18FF251B 1000ms/request AudioMute byte8.bit1 len2\r\n");
  printf("  CB6 18FF251C 300ms/request KeyACC byte8.bit7 len2\r\n");
}

static void DBC_PrintSignals(void)
{
  uint8_t index;

  printf("PC_Packet_1: status=%s length=%u received=%u\r\n",
         DBC_StatusName(dbc_packet1_last_status),
         dbc_packet1_receiver.total_length,
         dbc_packet1_receiver.received_length);
  if (dbc_packet1_last_status == DBC_STATUS_COMPLETE)
  {
    uint8_t shown = dbc_packet1_receiver.received_length;
    if (shown > 16U)
    {
      shown = 16U;
    }
    printf("  data(first %u)=", shown);
    for (index = 0U; index < shown; index++)
    {
      printf("%02X%s", dbc_packet1_receiver.data[index],
             ((index + 1U) < shown) ? " " : "");
    }
    printf("\r\n");
  }

  for (index = 0U; index < DBC_PC_SIGNAL_COUNT; index++)
  {
    if ((dbc_pc_signals.valid_mask & (uint8_t)(1U << index)) != 0U)
    {
      printf("%s=%u last=%lums\r\n", DBC_PcSignalName(index),
             dbc_pc_signals.data[index],
             (unsigned long)dbc_pc_signals.last_update_ms[index]);
    }
    else
    {
      printf("%s=#\r\n", DBC_PcSignalName(index));
    }
  }
}

static void Shell_DbcEncode(const char *arguments)
{
  uint32_t packet_number;
  uint32_t requested_value;
  uint32_t encoded_value;
  DBC_Frame frame;
  DBC_Status status;

  if ((Shell_ParseDecimal(&arguments, &packet_number) == 0U) ||
      (Shell_ParseDecimal(&arguments, &requested_value) == 0U))
  {
    printf("ERR usage: dbcencode <packet 1..6> <value>\r\n");
    return;
  }
  while (*arguments == ' ')
  {
    arguments++;
  }
  if (*arguments != '\0')
  {
    printf("ERR usage: dbcencode <packet 1..6> <value>\r\n");
    return;
  }

  status = DBC_EncodeCanBoxPacket((uint8_t)packet_number, requested_value,
                                  &frame, &encoded_value);
  if (status != DBC_STATUS_OK)
  {
    printf("ERR DBC encode: %s\r\n", DBC_StatusName(status));
    return;
  }
  printf("%s requested=%lu encoded=%lu%s\r\n",
         DBC_CanBoxSignalName((uint8_t)packet_number),
         (unsigned long)requested_value, (unsigned long)encoded_value,
         (requested_value != encoded_value) ? " (clamped)" : "");
  DBC_PrintFrame(&frame);
}
// DBC编码
static uint8_t DBC_SendCanBoxPacket(uint8_t packet_number, uint32_t value,
                                    uint8_t verbose)
{
  DBC_Frame frame;
  DBC_Status status;
  uint32_t encoded_value;

  status = DBC_EncodeCanBoxPacket(packet_number, value, &frame, &encoded_value);
  if (status != DBC_STATUS_OK)
  {
    if (verbose != 0U)
    {
      printf("ERR DBC encode: %s\r\n", DBC_StatusName(status));
    }
    return 0U;
  }

  if (CAN1_SendExtended(frame.id, frame.data, frame.dlc) == 0U)
  {
    dbc_tx_busy++;
    if (verbose != 0U)
    {
      printf("ERR DBC CAN TX busy/failed: packet=%u ID=%08lX\r\n",
             packet_number, (unsigned long)frame.id);
    }
    return 0U;
  }

  dbc_tx_queued++;
  if (verbose != 0U)
  {
    printf("DBC TX %s requested=%lu encoded=%lu%s\r\n",
           DBC_CanBoxSignalName(packet_number),
           (unsigned long)value, (unsigned long)encoded_value,
           (value != encoded_value) ? " (clamped)" : "");
    DBC_PrintFrame(&frame);
  }
  return 1U;
}

static void Shell_DbcSend(const char *arguments)
{
  uint32_t packet_number;
  uint32_t value;

  if ((Shell_ParseDecimal(&arguments, &packet_number) == 0U) ||
      (Shell_ParseDecimal(&arguments, &value) == 0U))
  {
    printf("ERR usage: dbcsend <packet 1..6> <value>\r\n");
    return;
  }
  while (*arguments == ' ')
  {
    arguments++;
  }
  if ((*arguments != '\0') || (packet_number < 1U) ||
      (packet_number > DBC_CANBOX_PACKET_COUNT))
  {
    printf("ERR usage: dbcsend <packet 1..6> <value>\r\n");
    return;
  }

  (void)DBC_SendCanBoxPacket((uint8_t)packet_number, value, 1U);
}

static void Shell_DbcSet(const char *arguments)
{
  uint32_t packet_number;
  uint32_t requested_value;
  uint32_t encoded_value;
  DBC_Frame frame;
  DBC_Status status;

  if ((Shell_ParseDecimal(&arguments, &packet_number) == 0U) ||
      (Shell_ParseDecimal(&arguments, &requested_value) == 0U))
  {
    printf("ERR usage: dbcset <packet 1..6> <value>\r\n");
    return;
  }
  while (*arguments == ' ')
  {
    arguments++;
  }
  if ((*arguments != '\0') || (packet_number < 1U) ||
      (packet_number > DBC_CANBOX_PACKET_COUNT))
  {
    printf("ERR usage: dbcset <packet 1..6> <value>\r\n");
    return;
  }

  status = DBC_EncodeCanBoxPacket((uint8_t)packet_number, requested_value,
                                  &frame, &encoded_value);
  if (status != DBC_STATUS_OK)
  {
    printf("ERR DBC set: %s\r\n", DBC_StatusName(status));
    return;
  }

  dbc_canbox_values[packet_number - 1U] = encoded_value;
  printf("DBC value %s=%lu%s%s\r\n",
         DBC_CanBoxSignalName((uint8_t)packet_number),
         (unsigned long)encoded_value,
         (requested_value != encoded_value) ? " (clamped)" : "",
         (dbc_tx_period_ms[packet_number - 1U] == 0U) ?
         " (request only; use dbcsend)" : "");
}

static void DBC_TxTask(uint32_t now_ms)
{
  uint8_t index;

  if (dbc_tx_enabled == 0U)
  {
    return;
  }

  for (index = 0U; index < DBC_CANBOX_PACKET_COUNT; index++)
  {
    uint32_t period_ms = dbc_tx_period_ms[index];
    uint32_t elapsed_ms = (uint32_t)(now_ms - dbc_tx_last_ms[index]);
    if ((period_ms != 0U) && (elapsed_ms >= period_ms))
    {
      if (DBC_SendCanBoxPacket((uint8_t)(index + 1U),
                               dbc_canbox_values[index], 0U) != 0U)
      {
        dbc_tx_last_ms[index] += (elapsed_ms / period_ms) * period_ms;
      }
      /* Queue at most one frame per main-loop pass. This also prevents four
         simultaneous periodic frames from exhausting bxCAN's three mailboxes. */
      return;
    }
  }
}

static void DBC_TxPrintStatus(void)
{
  uint8_t index;

  printf("DBC periodic TX: %s queued=%lu busy=%lu\r\n",
         (dbc_tx_enabled != 0U) ? "on" : "off",
         (unsigned long)dbc_tx_queued, (unsigned long)dbc_tx_busy);
  for (index = 0U; index < DBC_CANBOX_PACKET_COUNT; index++)
  {
    if (dbc_tx_period_ms[index] != 0U)
    {
      printf("  CB%u %s=%lu period=%lums\r\n", index + 1U,
             DBC_CanBoxSignalName((uint8_t)(index + 1U)),
             (unsigned long)dbc_canbox_values[index],
             (unsigned long)dbc_tx_period_ms[index]);
    }
    else
    {
      printf("  CB%u %s=%lu request-only\r\n", index + 1U,
             DBC_CanBoxSignalName((uint8_t)(index + 1U)),
             (unsigned long)dbc_canbox_values[index]);
    }
  }
}

static void Shell_DbcDecode(const char *arguments)
{
  static const uint32_t packet_ids[5] =
  {
    DBC_ID_PC_PACKET_2, DBC_ID_PC_PACKET_3, DBC_ID_PC_PACKET_4,
    DBC_ID_PC_PACKET_5, DBC_ID_PC_PACKET_6
  };
  uint32_t packet_number;
  uint32_t byte_value;
  uint8_t data[8];
  uint8_t index;
  uint8_t signal_index;
  uint8_t signal_value;
  DBC_Status status;

  if ((Shell_ParseDecimal(&arguments, &packet_number) == 0U) ||
      (packet_number < 2U) || (packet_number > 6U))
  {
    printf("ERR usage: dbcdecode <packet 2..6> <8 hex bytes>\r\n");
    return;
  }
  for (index = 0U; index < 8U; index++)
  {
    if ((Shell_ParseHex(&arguments, &byte_value) == 0U) || (byte_value > 0xFFU))
    {
      printf("ERR usage: dbcdecode <packet 2..6> <8 hex bytes>\r\n");
      return;
    }
    data[index] = (uint8_t)byte_value;
  }
  while (*arguments == ' ')
  {
    arguments++;
  }
  if (*arguments != '\0')
  {
    printf("ERR too many data bytes\r\n");
    return;
  }

  status = DBC_DecodePcFrame(packet_ids[packet_number - 2U], data, 8U,
                             AppTime_GetMs(), &dbc_pc_signals,
                             &signal_index, &signal_value);
  if (status == DBC_STATUS_OK)
  {
    printf("DBC decoded %s=%u\r\n",
           DBC_PcSignalName(signal_index), signal_value);
  }
  else
  {
    printf("ERR DBC decode: %s\r\n", DBC_StatusName(status));
  }
}

static void Shell_DbcChecksum(const char *arguments)
{
  uint8_t bytes[7];
  uint8_t index;
  uint32_t value;

  for (index = 0U; index < 7U; index++)
  {
    if ((Shell_ParseHex(&arguments, &value) == 0U) || (value > 0xFFU))
    {
      printf("ERR usage: dbccrc <7 hex bytes>\r\n");
      return;
    }
    bytes[index] = (uint8_t)value;
  }
  while (*arguments == ' ')
  {
    arguments++;
  }
  if (*arguments != '\0')
  {
    printf("ERR usage: dbccrc <7 hex bytes>\r\n");
    return;
  }
  printf("PC_Packet_1 checksum=%02X\r\n", DBC_Packet1Checksum(bytes));
}

static void CAN_PrintStatus(void)
{
  CAN1_Statistics statistics;

  CAN1_GetStatistics(&statistics);
  printf("CAN1: %s, 500K, normal mode\r\n", CAN1_GetStateName());
  printf("TEC=%u REC=%u LEC=%u ESR=%08lX\r\n",
         CAN1_GetTEC(), CAN1_GetREC(), CAN1_GetLastErrorCode(),
         (unsigned long)CAN1->ESR);
  printf("TX queued=%lu ok=%lu failed=%lu\r\n",
         (unsigned long)statistics.tx_queued,
         (unsigned long)statistics.tx_ok,
         (unsigned long)statistics.tx_failed);
  printf("RX=%lu dropped=%lu overrun=%lu error_irq=%lu monitor=%s\r\n",
         (unsigned long)statistics.rx_received,
         (unsigned long)statistics.rx_dropped,
         (unsigned long)statistics.fifo_overrun,
         (unsigned long)statistics.error_irqs,
         (can_rx_monitor_enabled != 0U) ? "on" : "off");
}

static void CAN_ProcessReceived(void)
{
  CAN1_Frame frame;
  // 从软件环形队列中取出一帧，使用while尽量把当前队列的所有报文处理完
  while (CAN1_Read(&frame) != 0U)
  {
    uint8_t i;
    DBC_Status dbc_status = DBC_STATUS_NOT_HANDLED;
    uint8_t signal_index = 0U;
    uint8_t signal_value = 0U;

    // 判断是否为扩展帧
    if (frame.is_extended != 0U)
    {
      // 特殊处理 PC_PACKET_1
      if (frame.id == DBC_ID_PC_PACKET_1)
      {
        dbc_status = DBC_Packet1Push(&dbc_packet1_receiver, frame.data,
                                     frame.dlc, frame.timestamp_ms);
        dbc_packet1_last_status = dbc_status;
      }
      else
      {
    	// 解析 PC2～PC6
        dbc_status = DBC_DecodePcFrame(frame.id, frame.data, frame.dlc,
                                       frame.timestamp_ms, &dbc_pc_signals,
                                       &signal_index, &signal_value);
      }
    }

    if (can_rx_monitor_enabled == 0U)
    {
      continue;
    }

    if (dbc_status == DBC_STATUS_OK)
    {
      printf("\r\nDBC RX %s=%u\r\n",
             DBC_PcSignalName(signal_index), signal_value);
    }
    else if (dbc_status == DBC_STATUS_COMPLETE)
    {
      printf("\r\nDBC RX PC_Packet_1 complete, length=%u\r\n",
             dbc_packet1_receiver.received_length);
    }
    else if ((dbc_status != DBC_STATUS_NOT_HANDLED) &&
             (dbc_status != DBC_STATUS_IN_PROGRESS))
    {
      printf("\r\nDBC RX rejected: %s\r\n", DBC_StatusName(dbc_status));
    }

    printf("\r\nCAN RX %s ID=%08lX DLC=%u DATA=",
           (frame.is_extended != 0U) ? "EXT" : "STD",
           (unsigned long)frame.id, frame.dlc);
    for (i = 0U; i < frame.dlc; i++)
    {
      printf("%02X", frame.data[i]);
      if ((i + 1U) < frame.dlc)
      {
        printf(" ");
      }
    }
    printf(" T=%lums\r\n", (unsigned long)frame.timestamp_ms);
    // 据监视开关把解析结果和原始报文输出到串口
    Shell_PrintPrompt();
  }
}

static void Menu_FormatPcValue(uint8_t index, uint32_t now_ms, char value[4])
{
  if ((index < DBC_PC_SIGNAL_COUNT) &&
      ((dbc_pc_signals.valid_mask & (uint8_t)(1U << index)) != 0U) &&
      ((uint32_t)(now_ms - dbc_pc_signals.last_update_ms[index]) <=
       dbc_pc_timeout_ms[index]))
  {
    (void)snprintf(value, 4U, "%03u", dbc_pc_signals.data[index]);
  }
  else
  {
    value[0] = '#';
    value[1] = '#';
    value[2] = '#';
    value[3] = '\0';
  }
}

static void Menu_RenderRow(uint8_t row, uint32_t now_ms)
{
  char line[LCD12864_COLS + 1U];

  memset(line, 0, sizeof(line));
  if (menu_page == 0U)
  {
    uint8_t packet = menu_selected_packet;
    // 读取刚修改的变量并组织文字
    if (row == 0U)
    {
      (void)snprintf(line, sizeof(line), "TX CB%u %s", packet + 1U,
                     (dbc_tx_period_ms[packet] != 0U) ? "AUTO" : "REQUEST");
    }
    else if (row == 1U)
    {
      (void)snprintf(line, sizeof(line), "%s=%lu/%u",
                     dbc_canbox_short_name[packet],
                     (unsigned long)dbc_canbox_values[packet],
                     dbc_canbox_max[packet]);
    }
    else if (row == 2U)
    {
      if (dbc_tx_period_ms[packet] != 0U)
      {
        uint16_t period_ms = (uint16_t)dbc_tx_period_ms[packet];
        (void)snprintf(line, sizeof(line), "P%uMS %s", period_ms,
                       (dbc_tx_enabled != 0U) ? "ON" : "OFF");
      }
      else
      {
        (void)snprintf(line, sizeof(line), "CONFIRM=SEND");
      }
    }
    else
    {
      (void)snprintf(line, sizeof(line), "LR SEL UD VALUE");
    }
  }
  else if (menu_page == 1U)
  {
    char first[4];
    char second[4];
    if (row == 0U)
    {
      Menu_FormatPcValue(0U, now_ms, first);
      (void)snprintf(line, sizeof(line), "RX PC2:%s", first);
    }
    else if (row == 1U)
    {
      Menu_FormatPcValue(1U, now_ms, first);
      Menu_FormatPcValue(2U, now_ms, second);
      (void)snprintf(line, sizeof(line), "P3:%s P4:%s", first, second);
    }
    else if (row == 2U)
    {
      Menu_FormatPcValue(3U, now_ms, first);
      Menu_FormatPcValue(4U, now_ms, second);
      (void)snprintf(line, sizeof(line), "P5:%s P6:%s", first, second);
    }
    else
    {
      (void)snprintf(line, sizeof(line), "#=TIMEOUT/NONE");
    }
  }
  else
  {
    CAN1_Statistics statistics;
    CAN1_GetStatistics(&statistics);
    if (row == 0U)
    {
      (void)snprintf(line, sizeof(line), "CAN %.12s", CAN1_GetStateName());
    }
    else if (row == 1U)
    {
      (void)snprintf(line, sizeof(line), "TX:%lu RX:%lu",
                     (unsigned long)statistics.tx_ok,
                     (unsigned long)statistics.rx_received);
    }
    else if (row == 2U)
    {
      (void)snprintf(line, sizeof(line), "DROP:%lu ERR:%lu",
                     (unsigned long)statistics.rx_dropped,
                     (unsigned long)statistics.error_irqs);
    }
    else
    {
      (void)snprintf(line, sizeof(line), "TEC:%u REC:%u",
                     CAN1_GetTEC(), CAN1_GetREC());
    }
  }
  //最后显示LCD
  LCD12864_WriteLine(row, line);
}

static void Menu_LcdTask(uint32_t now_ms)
{
  if (AppTime_IsDue(now_ms, &menu_refresh_last_ms,
                    LCD_ROW_REFRESH_MS) != 0U)
  {
    Menu_RenderRow(menu_refresh_row, now_ms);
    menu_refresh_row = (uint8_t)((menu_refresh_row + 1U) % LCD12864_ROWS);
  }
}
// 对按键进行处理
static void Menu_HandleKeyEvent(const KeyEvent *event)
{
  uint8_t packet;

  if (event == NULL)
  {
    return;
  }
  // NEXT/PREVIOUS 修改menu_page,切换TX/RX/CAN页面
  if (event->key == KEY_RAW_NEXT_PAGE)
  {
    menu_page = (uint8_t)((menu_page + 1U) % MENU_PAGE_COUNT);
  }
  else if (event->key == KEY_RAW_PREVIOUS_PAGE)
  {
    menu_page = (menu_page == 0U) ? (MENU_PAGE_COUNT - 1U) :
                                    (uint8_t)(menu_page - 1U);
  }
  else if (menu_page == 0U)
  {
	// 当前选择CB1～CB6
    packet = menu_selected_packet;
    if (event->key == KEY_RAW_LEFT)
    {
      menu_selected_packet = (packet == 0U) ?
                             (DBC_CANBOX_PACKET_COUNT - 1U) :
                             (uint8_t)(packet - 1U);
    }
    else if (event->key == KEY_RAW_RIGHT)
    {
      menu_selected_packet = (uint8_t)((packet + 1U) %
                                       DBC_CANBOX_PACKET_COUNT);
    }
    else if (event->key == KEY_RAW_UP)
    {
      // 长按UP直接变成最大值
      if (event->type == KEY_EVENT_LONG)
      {
        dbc_canbox_values[packet] = dbc_canbox_max[packet];
      }
      else if (dbc_canbox_values[packet] < dbc_canbox_max[packet])
      {
        dbc_canbox_values[packet]++;
      }
    }
    // 长按DOWN直接变成0
    else if (event->key == KEY_RAW_DOWN)
    {
      if (event->type == KEY_EVENT_LONG)
      {
        dbc_canbox_values[packet] = 0U;
      }
      else if (dbc_canbox_values[packet] > 0U)
      {
        dbc_canbox_values[packet]--;
      }
    }
    else if (event->key == KEY_RAW_CONFIRM)
    {
      (void)DBC_SendCanBoxPacket((uint8_t)(packet + 1U),
                                 dbc_canbox_values[packet], 1U);
    }
    else if (event->key == KEY_RAW_CANCEL)
    {
      dbc_canbox_values[packet] = 0U;
    }
    else if (event->key == KEY_RAW_REVERSE)
    {
      dbc_tx_enabled = (dbc_tx_enabled == 0U) ? 1U : 0U;
    }
  }

  menu_refresh_row = 0U;
  menu_refresh_last_ms = 0U;
}

static void Key_ProcessEvents(void)
{
  KeyEvent event;

  while (KeyService_GetEvent(&event) != 0U)
  {
    const char *type_name = (event.type == KEY_EVENT_LONG) ? "LONG" : "SHORT";
    // 按键事件进行处理
    Menu_HandleKeyEvent(&event);

    if (key_monitor_enabled != 0U)
    {
      printf("\r\nKEY %s %s duration=%lums\r\n",
             ADC_Keys_GetName(event.key), type_name,
             (unsigned long)event.duration_ms);
      Shell_PrintPrompt();
    }
  }
}

static void Shell_Execute(char *line)
{
  char *end;

  while (*line == ' ')
  {
    line++;
  }

  end = line + strlen(line);
  while ((end > line) && (end[-1] == ' '))
  {
    *--end = '\0';
  }

  if (*line == '\0')
  {
    return;
  }
  // help/h 显示全部支持的命令
  if ((strcmp(line, "help") == 0) || (strcmp(line, "h") == 0))
  {
    Shell_PrintHelp();
  }
  // 显示当前固件版本
  else if (strcmp(line, "checkVer") == 0)
  {
    printf("%s\r\n", FW_VERSION);
  }
  // canstat显示CAN控制器状态
  /*CAN1: Error Active, 500K, normal mode
   *TEC=0 REC=0 LEC=0 ESR=00000000
   *TX queued=10 ok=10 failed=0
   *RX=5 dropped=0 overrun=0 error_irq=0
   *TEC：发送错误计数
   *REC：接收错误计数
   *LEC：最后一次CAN错误类型
   *TX queued：放入发送邮箱的数量
   *TX ok：实际发送成功数量
   *TX failed：发送失败数量
   *RX：收到的报文数量
   *dropped：软件队列满导致的丢帧
   *overrun：CAN硬件FIFO溢出
   *error_irq：CAN错误中断次数
   */
  else if (strcmp(line, "canstat") == 0)
  {
    CAN_PrintStatus();
  }
  // 直接发送一帧29位扩展CAN报文
  // 示例 cansend 18FF2517 03 00 00 00 00 00 00 00
  else if ((strncmp(line, "cansend", 7U) == 0) &&
           ((line[7] == '\0') || (line[7] == ' ')))
  {
    Shell_CanSend(&line[7]);
  }
  // 打开CAN接收报文打印
  else if (strcmp(line, "canrx on") == 0)
  {
    can_rx_monitor_enabled = 1U;
    printf("CAN RX monitor on\r\n");
  }
  // 关闭CAN接收报文的串口打印
  else if (strcmp(line, "canrx off") == 0)
  {
    can_rx_monitor_enabled = 0U;
    printf("CAN RX monitor off\r\n");
  }
  // 清空CAN软件统计计数
  else if (strcmp(line, "canclear") == 0)
  {
    CAN1_ClearStatistics();
    printf("CAN counters cleared\r\n");
  }
  // 显示四行固定测试字符
  else if (strcmp(line, "lcdtest") == 0)
  {
    LCD12864_TestPattern();
    printf("LCD test pattern sent\r\n");
  }
  // 清空LCD
  else if (strcmp(line, "lcdclear") == 0)
  {
    LCD12864_Clear();
    printf("LCD cleared\r\n");
  }
  // 重新发送LCD初始化序列
  else if (strcmp(line, "lcdinit") == 0)
  {
    (void)LCD12864_Init();
    printf("LCD initialization sequence sent\r\n");
  }
  // lcdshow <row> <col> <text> 在指定位置显示ASCII文本 row：0～3 col：0～15
  else if ((strncmp(line, "lcdshow", 7U) == 0) &&
           ((line[7] == '\0') || (line[7] == ' ')))
  {
    Shell_LcdShow(&line[7]);
  }
  // 查看三组按键电阻网络的ADC值及当前原始按键：
  else if (strcmp(line, "keyadc") == 0)
  {
    printf("KEY ADC: PC4/CH14=%u PC5/CH15=%u PB0/CH8=%u raw=%s\r\n",
           ADC_Keys_GetAdc(0U), ADC_Keys_GetAdc(1U), ADC_Keys_GetAdc(2U),
           ADC_Keys_GetName(ADC_Keys_GetRaw()));
  }
  // 查看消抖后的稳定按键和按下时长
  else if (strcmp(line, "keystat") == 0)
  {
    uint32_t now_ms = AppTime_GetMs();
    KeyRaw stable_key = KeyService_GetStableKey();
    printf("KEY stable=%s duration=%lums tick=%lums\r\n",
           ADC_Keys_GetName(stable_key),
           (unsigned long)KeyService_GetPressedDuration(now_ms),
           (unsigned long)now_ms);
  }
  // 打开按键事件打印
  else if (strcmp(line, "keymon on") == 0)
  {
    key_monitor_enabled = 1U;
    printf("Key event monitor on\r\n");
  }
  // 关闭按键事件打印
  else if (strcmp(line, "keymon off") == 0)
  {
    key_monitor_enabled = 0U;
    printf("Key event monitor off\r\n");
  }
  // 显示DBC报文ID、信号位置和周期
  /* PC1～PC6：0CFFD221～0CFFD226
   * CB1～CB6：18FF2517～18FF251C
   * */
  else if (strcmp(line, "dbcinfo") == 0)
  {
    DBC_PrintInfo();
  }
  // 只进行DBC编码，不发送CAN报文
  /*dbcencode <报文编号1..6> <信号值>
   * dbcencode 1 3
   * DevIx requested=3 encoded=3
   * ID=18FF2517 DLC=8 DATA=03 00 00 00 00 00 00 00
   * */
  else if ((strncmp(line, "dbcencode", 9U) == 0) &&
           ((line[9] == '\0') || (line[9] == ' ')))
  {
    Shell_DbcEncode(&line[9]);
  }
  // DBC编码后立即发送CAN报文(编码、显示并真正发送)
  /*dbcsend <报文编号1..6> <信号值>
   * */
  else if ((strncmp(line, "dbcsend", 7U) == 0) &&
           ((line[7] == '\0') || (line[7] == ' ')))
  {
    Shell_DbcSend(&line[7]);
  }
  // 修改程序保存的DBC信号值
  else if ((strncmp(line, "dbcset", 6U) == 0) &&
           ((line[6] == '\0') || (line[6] == ' ')))
  {
    Shell_DbcSet(&line[6]);
  }
  // 打开DBC周期发送，同时重新计算各报文的周期起点
  else if (strcmp(line, "dbctx on") == 0)
  {
    uint8_t index;
    uint32_t now_ms = AppTime_GetMs();
    for (index = 0U; index < DBC_CANBOX_PACKET_COUNT; index++)
    {
      dbc_tx_last_ms[index] = now_ms;
    }
    dbc_tx_enabled = 1U;
    printf("DBC periodic TX on\r\n");
  }
  // 关闭所有DBC周期发送
  else if (strcmp(line, "dbctx off") == 0)
  {
    dbc_tx_enabled = 0U;
    printf("DBC periodic TX off\r\n");
  }
  // 显示周期发送状态
  else if (strcmp(line, "dbctx stat") == 0)
  {
    DBC_TxPrintStatus();
  }
  // 不经过真实CAN，直接调用软件解码函数
  // dbcdecode <PC报文编号2..6> <8字节数据> dbcdecode 4 11 22 33 44 55 66 77 88
  // 用于离线验证PC2～PC6的DBC信号解析是否正确
  else if ((strncmp(line, "dbcdecode", 9U) == 0) &&
           ((line[9] == '\0') || (line[9] == ' ')))
  {
    Shell_DbcDecode(&line[9]);
  }
  // 计算PC Packet 1的校验值
  else if ((strncmp(line, "dbccrc", 6U) == 0) &&
           ((line[6] == '\0') || (line[6] == ' ')))
  {
    Shell_DbcChecksum(&line[6]);
  }
  // 显示已经从CAN总线接收到的PC端信号状态，如果某个PC报文尚未收到，会显示PC4=#
  else if (strcmp(line, "dbcshow") == 0)
  {
    DBC_PrintSignals();
  }
  // 执行DBC模块内部自测 DBC self-test: passed=6 failed=0 PASS
  else if (strcmp(line, "dbctest") == 0)
  {
    uint8_t passed;
    uint8_t failed;
    DBC_RunSelfTest(&passed, &failed);
    printf("DBC self-test: passed=%u failed=%u %s\r\n",
           passed, failed, (failed == 0U) ? "PASS" : "FAIL");
  }
  // 执行mcu复位
  else if (strcmp(line, "reset") == 0)
  {
    printf("Resetting MCU...\r\n");
    HAL_Delay(20U);
    NVIC_SystemReset();
  }
  else
  {
    printf("ERR unknown command: %s\r\n", line);
  }
}

static void Shell_Process(void)
{
  static char line[SHELL_LINE_SIZE];
  static uint16_t length;
  static uint8_t last_was_cr;
  uint8_t byte;

  while (UART4_ReadByte(&byte) != 0U)
  {
	// 收到\r或\n时，程序认为一条命令输入完成
    if ((byte == '\r') || (byte == '\n'))
    {
      if ((byte == '\n') && (last_was_cr != 0U))
      {
        last_was_cr = 0U;
        continue;
      }

      last_was_cr = (byte == '\r') ? 1U : 0U;
      printf("\r\n");
      line[length] = '\0';
      // 识别命令
      Shell_Execute(line);
      length = 0U;
      Shell_PrintPrompt();
    }
    else if ((byte == '\b') || (byte == 0x7FU))
    {
      last_was_cr = 0U;
      if (length > 0U)
      {
        length--;
        printf("\b \b");
      }
    }
    else if ((byte >= 0x20U) && (byte <= 0x7EU))
    {
      last_was_cr = 0U;
      if (length < (SHELL_LINE_SIZE - 1U))
      {
        line[length++] = (char)byte;
        (void)__io_putchar((int)byte);
      }
      else
      {
        length = 0U;
        printf("\r\nERR line too long\r\n");
        Shell_PrintPrompt();
      }
    }
  }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* 初始化GPIO、TIM3、UART4 */
  MX_GPIO_Init();
  MX_TIM3_Init();
  MX_UART4_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start_IT(&htim3);
  UART4_StartReceive();
  printf("\r\nCANBox UART Shell %s\r\n", FW_VERSION);
  /* 初始化DBC接收状态 */
  DBC_Packet1ReceiverInit(&dbc_packet1_receiver);
  printf("DBC codec ready: 12 extended frames, defensive checks enabled\r\n");
  /*初始化ADC按键与LCD*/
  if (ADC_Keys_Init() != 0U)
  {
	// 按键消抖
    KeyService_Init(AppTime_GetMs());
    printf("ADC keypad ready: PC4/PC5/PB0, scan period 10ms\r\n");
  }
  else
  {
    printf("ERR ADC keypad initialization failed\r\n");
  }
  (void)LCD12864_Init();
  LCD12864_WriteLine(0U, "CANBox V1.6.0");
  LCD12864_WriteLine(1U, "UART4 115200");
  LCD12864_WriteLine(2U, "KEY MENU READY");
  LCD12864_WriteLine(3U, "NEXT/PREV PAGE");
  printf("LCD12864 initialization sequence sent\r\n");
  /*初始化CAN1*/
  if (CAN1_Init() != 0U)
  {
    uint8_t index;
    uint32_t now_ms = AppTime_GetMs();
    for (index = 0U; index < DBC_CANBOX_PACKET_COUNT; index++)
    {
      dbc_tx_last_ms[index] = now_ms;
    }
    printf("CAN1 ready: 500K extended frame, normal mode\r\n");
    // 在CAN初始化成功后执行
    dbc_tx_enabled = 1U;
    printf("DBC periodic TX ready: CB1/CB2/CB5/CB6 enabled\r\n");
  }
  else
  {
    printf("ERR CAN1 init failed (PCLK1 must be 36 MHz)\r\n");
  }
  printf("Type help or h for commands.\r\n");
  // 在串口终端打印命令提示符
  Shell_PrintPrompt();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    uint32_t now_ms = AppTime_GetMs();

    Shell_Process();
    CAN_ProcessReceived();
    DBC_TxTask(now_ms);
    // 按键改变LCD过程
    // 按键采样周期10ms
    if (AppTime_IsDue(now_ms, &key_sample_last_ms, 10U) != 0U)
    {
      // ADC识别按键，ADC先把电压转换为对应按键
      ADC_Keys_SampleTask();
      // 按键消抖后产生按键事件
      KeyService_Task(now_ms);
    }
    // 主循环取出事件
    Key_ProcessEvents();
    // 按键处理结束LCD重新刷新
    Menu_LcdTask(now_ms);
    if (AppTime_IsDue(now_ms, &heartbeat_last_ms, 500U) != 0U)
    {
      HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_3);
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.Prediv1Source = RCC_PREDIV1_SOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  RCC_OscInitStruct.PLL2.PLL2State = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the Systick interrupt time
  */
  __HAL_RCC_PLLI2S_ENABLE();
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
