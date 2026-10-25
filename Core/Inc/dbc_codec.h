#ifndef DBC_CODEC_H
#define DBC_CODEC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define DBC_FRAME_DLC               8U
#define DBC_PC_SIGNAL_COUNT         5U
#define DBC_PACKET1_MAX_DATA        255U
#define DBC_PACKET1_TIMEOUT_MS      1000U

#define DBC_ID_PC_PACKET_1          0x0CFFD221UL
#define DBC_ID_PC_PACKET_2          0x0CFFD222UL
#define DBC_ID_PC_PACKET_3          0x0CFFD223UL
#define DBC_ID_PC_PACKET_4          0x0CFFD224UL
#define DBC_ID_PC_PACKET_5          0x0CFFD225UL
#define DBC_ID_PC_PACKET_6          0x0CFFD226UL

#define DBC_ID_CANBOX_PACKET_1      0x18FF2517UL
#define DBC_ID_CANBOX_PACKET_2      0x18FF2518UL
#define DBC_ID_CANBOX_PACKET_3      0x18FF2519UL
#define DBC_ID_CANBOX_PACKET_4      0x18FF251AUL
#define DBC_ID_CANBOX_PACKET_5      0x18FF251BUL
#define DBC_ID_CANBOX_PACKET_6      0x18FF251CUL

typedef enum
{
  DBC_STATUS_OK = 0,
  DBC_STATUS_NOT_HANDLED,
  DBC_STATUS_BAD_ARGUMENT,
  DBC_STATUS_BAD_DLC,
  DBC_STATUS_BAD_CHECKSUM,
  DBC_STATUS_BAD_SEQUENCE,
  DBC_STATUS_TIMEOUT,
  DBC_STATUS_IN_PROGRESS,
  DBC_STATUS_COMPLETE
} DBC_Status;

typedef struct
{
  uint32_t id;
  uint8_t dlc;
  uint8_t data[DBC_FRAME_DLC];
} DBC_Frame;

typedef struct
{
  uint8_t data[DBC_PC_SIGNAL_COUNT];
  uint8_t valid_mask; //表示0表示全部还没有收到有效数据，LCD启动时显示###
  uint32_t last_update_ms[DBC_PC_SIGNAL_COUNT];
} DBC_PcSignals;

typedef struct
{
  uint8_t active;// 是否正在接收一组多帧数据
  uint8_t expected_sequence;//下一帧应该是什么序号
  uint8_t total_length;//完整数据总长度
  uint8_t received_length;//已经收到多少字节
  uint32_t last_frame_ms;//上一帧到达时间
  uint8_t data[DBC_PACKET1_MAX_DATA];// 拼接后的完整数据
} DBC_Packet1Receiver;

uint8_t DBC_SetIntelSignal(uint8_t data[DBC_FRAME_DLC], uint8_t start_bit,
                           uint8_t bit_length, uint32_t raw_value);
uint8_t DBC_GetIntelSignal(const uint8_t data[DBC_FRAME_DLC], uint8_t start_bit,
                           uint8_t bit_length, uint32_t *raw_value);

DBC_Status DBC_EncodeCanBoxPacket(uint8_t packet_number, uint32_t requested_value,
                                  DBC_Frame *frame, uint32_t *encoded_value);
DBC_Status DBC_DecodePcFrame(uint32_t id, const uint8_t *data, uint8_t dlc,
                             uint32_t now_ms, DBC_PcSignals *signals,
                             uint8_t *signal_index, uint8_t *signal_value);

uint8_t DBC_Packet1Checksum(const uint8_t bytes[7]);
void DBC_Packet1ReceiverInit(DBC_Packet1Receiver *receiver);
DBC_Status DBC_Packet1Push(DBC_Packet1Receiver *receiver,
                           const uint8_t *data, uint8_t dlc, uint32_t now_ms);

const char *DBC_StatusName(DBC_Status status);
const char *DBC_PcSignalName(uint8_t signal_index);
const char *DBC_CanBoxSignalName(uint8_t packet_number);
void DBC_RunSelfTest(uint8_t *passed, uint8_t *failed);

#ifdef __cplusplus
}
#endif

#endif /* DBC_CODEC_H */
