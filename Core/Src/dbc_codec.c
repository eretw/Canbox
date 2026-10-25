// CAN驱动和业务逻辑之间的“协议转换层”
#include "dbc_codec.h"
#include <string.h>

typedef struct
{
  uint32_t id;
  uint8_t start_bit;
  uint8_t bit_length;
  uint8_t max_value;
} DBC_MessageDefinition;

static const DBC_MessageDefinition canbox_definitions[6] =
{
  // CAN ID、信号起始位、信号长度、最大值
  { DBC_ID_CANBOX_PACKET_1,  0U, 2U,  3U }, /* DevIx: byte 1, bit 1 */
  { DBC_ID_CANBOX_PACKET_2, 18U, 2U,  3U }, /* UnitSysSet: byte 3, bit 3 */
  { DBC_ID_CANBOX_PACKET_3, 32U, 4U, 15U }, /* CallFromLogIx: byte 5, bit 1 */
  { DBC_ID_CANBOX_PACKET_4, 40U, 3U,  7U }, /* MutePrioReq: byte 6, bit 1 */
  { DBC_ID_CANBOX_PACKET_5, 56U, 2U,  3U }, /* AudioMute: byte 8, bit 1 */
  { DBC_ID_CANBOX_PACKET_6, 62U, 2U,  3U }  /* KeyACC: byte 8, bit 7 */
};

static const DBC_MessageDefinition pc_definitions[DBC_PC_SIGNAL_COUNT] =
{
  { DBC_ID_PC_PACKET_2,  0U, 8U, 255U },
  { DBC_ID_PC_PACKET_3,  8U, 8U, 255U },
  { DBC_ID_PC_PACKET_4, 16U, 8U, 255U },
  { DBC_ID_PC_PACKET_5, 24U, 8U, 255U },
  { DBC_ID_PC_PACKET_6, 32U, 8U, 255U }
};
// 按照小端格式逐bit写入
uint8_t DBC_SetIntelSignal(uint8_t data[DBC_FRAME_DLC], uint8_t start_bit,
                           uint8_t bit_length, uint32_t raw_value)
{
  uint8_t bit;

  if ((data == 0) || (bit_length == 0U) || (bit_length > 32U) ||
      (((uint16_t)start_bit + bit_length) > 64U))
  {
    return 0U;
  }

  for (bit = 0U; bit < bit_length; bit++)
  {
    uint8_t frame_bit = (uint8_t)(start_bit + bit);
    uint8_t mask = (uint8_t)(1U << (frame_bit & 7U));
    uint8_t *byte = &data[frame_bit >> 3U];

    if ((raw_value & (1UL << bit)) != 0U)
    {
      *byte |= mask;
    }
    else
    {
      *byte &= (uint8_t)~mask;
    }
  }
  return 1U;
}// 解码函数，从CAN报文的指定位置取出bit，重新组合成业务数值
uint8_t DBC_GetIntelSignal(const uint8_t data[DBC_FRAME_DLC], uint8_t start_bit,
                           uint8_t bit_length, uint32_t *raw_value)
{
  uint32_t value = 0U;
  uint8_t bit;

  if ((data == 0) || (raw_value == 0) || (bit_length == 0U) ||
      (bit_length > 32U) || (((uint16_t)start_bit + bit_length) > 64U))
  {
    return 0U;
  }

  for (bit = 0U; bit < bit_length; bit++)
  {
    uint8_t frame_bit = (uint8_t)(start_bit + bit);
    if ((data[frame_bit >> 3U] & (uint8_t)(1U << (frame_bit & 7U))) != 0U)
    {
      value |= (1UL << bit);
    }
  }
  *raw_value = value;
  return 1U;
}
// 负责把按键或Shell设置的数值转换成CanBox报文
DBC_Status DBC_EncodeCanBoxPacket(uint8_t packet_number, uint32_t requested_value,
                                  DBC_Frame *frame, uint32_t *encoded_value)
{
  const DBC_MessageDefinition *definition;
  uint32_t value;

  if ((packet_number < 1U) || (packet_number > 6U) || (frame == 0))
  {
    return DBC_STATUS_BAD_ARGUMENT;
  }

  definition = &canbox_definitions[packet_number - 1U];
  value = requested_value;
  if (value > definition->max_value)
  {
    value = definition->max_value;
  }

  frame->id = definition->id;
  frame->dlc = DBC_FRAME_DLC;
  memset(frame->data, 0, sizeof(frame->data));
  if (DBC_SetIntelSignal(frame->data, definition->start_bit,
                         definition->bit_length, value) == 0U)
  {
    return DBC_STATUS_BAD_ARGUMENT;
  }
  if (encoded_value != 0)
  {
    *encoded_value = value;
  }
  return DBC_STATUS_OK;
}

//收到有效报文之后，LCD更新（用于解析PC发给开发板的普通单帧报文）
DBC_Status DBC_DecodePcFrame(uint32_t id, const uint8_t *data, uint8_t dlc,
                             uint32_t now_ms, DBC_PcSignals *signals,
                             uint8_t *signal_index, uint8_t *signal_value)
{
  uint8_t index;
  uint32_t value;

  if ((data == 0) || (signals == 0))
  {
    return DBC_STATUS_BAD_ARGUMENT;
  }
  if (dlc != DBC_FRAME_DLC)
  {
    return DBC_STATUS_BAD_DLC;
  }

  for (index = 0U; index < DBC_PC_SIGNAL_COUNT; index++)
  {
    if (id == pc_definitions[index].id)
    {
      if (DBC_GetIntelSignal(data, pc_definitions[index].start_bit,
                             pc_definitions[index].bit_length, &value) == 0U)
      {
        return DBC_STATUS_BAD_ARGUMENT;
      }
      // 保存信号值
      signals->data[index] = (uint8_t)value;
      // 标记该信号已经收到过
      signals->valid_mask |= (uint8_t)(1U << index);
      // 用于判断信号是否超时
      signals->last_update_ms[index] = now_ms;
      if (signal_index != 0)
      {
        *signal_index = index;
      }
      if (signal_value != 0)
      {
        *signal_value = (uint8_t)value;
      }
      return DBC_STATUS_OK;
    }
  }
  return DBC_STATUS_NOT_HANDLED;
}

uint8_t DBC_Packet1Checksum(const uint8_t bytes[7])
{
  uint16_t sum = 0U;
  uint8_t index;

  if (bytes == 0)
  {
    return 0xFFU;
  }
  for (index = 0U; index < 7U; index++)
  {
    sum = (uint16_t)(sum + bytes[index]);
  }
  return (uint8_t)((((sum >> 6U) & 0x03U) + (sum >> 3U) + sum) & 0x07U);
}

void DBC_Packet1ReceiverInit(DBC_Packet1Receiver *receiver)
{
  if (receiver != 0)
  {
    memset(receiver, 0, sizeof(*receiver));
  }
}

DBC_Status DBC_Packet1Push(DBC_Packet1Receiver *receiver,
                           const uint8_t *data, uint8_t dlc, uint32_t now_ms)
{
  uint8_t sequence;
  uint8_t source_index;
  uint8_t source_count;

  if ((receiver == 0) || (data == 0))
  {
    return DBC_STATUS_BAD_ARGUMENT;
  }
  if (dlc != DBC_FRAME_DLC)
  {
    return DBC_STATUS_BAD_DLC;
  }
  if (DBC_Packet1Checksum(data) != data[7])
  {
    receiver->active = 0U;
    return DBC_STATUS_BAD_CHECKSUM;
  }

  sequence = data[0];
  if (sequence == 1U)
  {
    receiver->total_length = data[1];
    receiver->received_length = 0U;
    receiver->expected_sequence = 2U;
    receiver->last_frame_ms = now_ms;
    receiver->active = (receiver->total_length > 0U) ? 1U : 0U;
    if (receiver->active == 0U)
    {
      return DBC_STATUS_BAD_ARGUMENT;
    }
    source_index = 2U;
    source_count = 5U;
  }
  else
  {
    if ((receiver->active == 0U) ||
        ((uint32_t)(now_ms - receiver->last_frame_ms) > DBC_PACKET1_TIMEOUT_MS))
    {
      receiver->active = 0U;
      return DBC_STATUS_TIMEOUT;
    }
    if (sequence != receiver->expected_sequence)
    {
      receiver->active = 0U;
      return DBC_STATUS_BAD_SEQUENCE;
    }
    receiver->expected_sequence++;
    receiver->last_frame_ms = now_ms;
    source_index = 1U;
    source_count = 6U;
  }

  while ((source_count > 0U) &&
         (receiver->received_length < receiver->total_length))
  {
    receiver->data[receiver->received_length++] = data[source_index++];
    source_count--;
  }

  if (receiver->received_length >= receiver->total_length)
  {
    receiver->active = 0U;
    return DBC_STATUS_COMPLETE;
  }
  return DBC_STATUS_IN_PROGRESS;
}

const char *DBC_StatusName(DBC_Status status)
{
  static const char *const names[] =
  {
    "OK", "NOT_HANDLED", "BAD_ARGUMENT", "BAD_DLC", "BAD_CHECKSUM",
    "BAD_SEQUENCE", "TIMEOUT", "IN_PROGRESS", "COMPLETE"
  };
  if ((uint32_t)status >= (sizeof(names) / sizeof(names[0])))
  {
    return "UNKNOWN";
  }
  return names[status];
}

const char *DBC_PcSignalName(uint8_t signal_index)
{
  static const char *const names[DBC_PC_SIGNAL_COUNT] =
  {
    "PC2.Data", "PC3.Data", "PC4.Data", "PC5.Data", "PC6.Data"
  };
  return (signal_index < DBC_PC_SIGNAL_COUNT) ? names[signal_index] : "UNKNOWN";
}

const char *DBC_CanBoxSignalName(uint8_t packet_number)
{
  static const char *const names[6] =
  {
    "DevIx", "UnitSysSet", "CallFromLogIx", "MutePrioReq", "AudioMute", "KeyACC"
  };
  return ((packet_number >= 1U) && (packet_number <= 6U)) ?
         names[packet_number - 1U] : "UNKNOWN";
}

void DBC_RunSelfTest(uint8_t *passed, uint8_t *failed)
{
  DBC_Frame frame;
  DBC_PcSignals signals = {0};
  DBC_Packet1Receiver receiver;
  uint8_t bytes[8] = {0};
  uint32_t encoded;
  uint8_t index;
  uint8_t value;
  uint8_t ok = 0U;
  uint8_t bad = 0U;

#define DBC_TEST(condition) do { if (condition) { ok++; } else { bad++; } } while (0)

  DBC_TEST((DBC_EncodeCanBoxPacket(2U, 3U, &frame, &encoded) == DBC_STATUS_OK) &&
           (frame.id == DBC_ID_CANBOX_PACKET_2) && (frame.data[2] == 0x0CU) &&
           (encoded == 3U));
  DBC_TEST((DBC_EncodeCanBoxPacket(4U, 99U, &frame, &encoded) == DBC_STATUS_OK) &&
           (frame.data[5] == 0x07U) && (encoded == 7U));
  DBC_TEST((DBC_EncodeCanBoxPacket(6U, 3U, &frame, &encoded) == DBC_STATUS_OK) &&
           (frame.data[7] == 0xC0U));

  bytes[2] = 0x5AU;
  DBC_TEST((DBC_DecodePcFrame(DBC_ID_PC_PACKET_4, bytes, 8U, 123U,
                              &signals, &index, &value) == DBC_STATUS_OK) &&
           (index == 2U) && (value == 0x5AU) &&
           ((signals.valid_mask & 0x04U) != 0U));
  DBC_TEST(DBC_DecodePcFrame(DBC_ID_PC_PACKET_4, bytes, 7U, 123U,
                             &signals, &index, &value) == DBC_STATUS_BAD_DLC);

  DBC_Packet1ReceiverInit(&receiver);
  bytes[0] = 1U; bytes[1] = 8U; bytes[2] = 1U; bytes[3] = 2U;
  bytes[4] = 3U; bytes[5] = 4U; bytes[6] = 5U;
  bytes[7] = DBC_Packet1Checksum(bytes);
  DBC_TEST(DBC_Packet1Push(&receiver, bytes, 8U, 100U) == DBC_STATUS_IN_PROGRESS);
  bytes[0] = 2U; bytes[1] = 6U; bytes[2] = 7U; bytes[3] = 8U;
  bytes[4] = 0xFFU; bytes[5] = 0xFFU; bytes[6] = 0xFFU;
  bytes[7] = DBC_Packet1Checksum(bytes);
  DBC_TEST((DBC_Packet1Push(&receiver, bytes, 8U, 110U) == DBC_STATUS_COMPLETE) &&
           (receiver.received_length == 8U) && (receiver.data[0] == 1U) &&
           (receiver.data[7] == 8U));

#undef DBC_TEST
  if (passed != 0)
  {
    *passed = ok;
  }
  if (failed != 0)
  {
    *failed = bad;
  }
}
