#include "can.h"

#include <string.h>

#define CAN1_REQUIRED_PCLK1_HZ 36000000UL
#define CAN1_INIT_TIMEOUT_MS    100U

static volatile CAN1_Frame can1_rx_queue[CAN1_RX_QUEUE_SIZE];
static volatile uint8_t can1_rx_head;
static volatile uint8_t can1_rx_tail;
static volatile uint8_t can1_initialized;
static volatile CAN1_Statistics can1_statistics;

static uint8_t CAN1_WaitForMcrState(uint32_t mask, uint8_t set)
{
  uint32_t start = HAL_GetTick();

  while ((((CAN1->MSR & mask) != 0U) ? 1U : 0U) != set)
  {
    if ((HAL_GetTick() - start) >= CAN1_INIT_TIMEOUT_MS)
    {
      return 0U;
    }
  }

  return 1U;
}

static void CAN1_ConfigurePins(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_AFIO_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /* CAN1 default mapping: PA11 = RX, PA12 = TX. */
  MODIFY_REG(AFIO->MAPR, AFIO_MAPR_CAN_REMAP, AFIO_MAPR_CAN_REMAP_REMAP1);

  gpio.Pin = GPIO_PIN_11;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &gpio);

  gpio.Pin = GPIO_PIN_12;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &gpio);

  /* Board transceiver control: PC6 = STB_CAN, PB15 = EN_CAN.
   * TCAN1043 normal mode requires STB=HIGH and EN=HIGH. */
  gpio.Pin = CAN1_STB_Pin;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(CAN1_STB_GPIO_Port, &gpio);

  gpio.Pin = CAN1_EN_Pin;
  HAL_GPIO_Init(CAN1_EN_GPIO_Port, &gpio);

  HAL_GPIO_WritePin(CAN1_STB_GPIO_Port, CAN1_STB_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(CAN1_EN_GPIO_Port, CAN1_EN_Pin, GPIO_PIN_SET);
  HAL_Delay(1U);
}

uint8_t CAN1_Init(void)
{
  uint32_t btr;
  // 清空软件状态
  can1_initialized = 0U;
  CAN1_ClearStatistics();
  can1_rx_head = 0U;
  can1_rx_tail = 0U;
  // 检查PCLK1时钟
  if (HAL_RCC_GetPCLK1Freq() != CAN1_REQUIRED_PCLK1_HZ)
  {
    return 0U;
  }
  // CAN引脚配置
  /*这一步让板载CAN收发器进入当前板卡定义的工作状态。MCU内部CAN控制器不能直接连接CANH/CANL，
   * 必须经过CAN收发器完成电平转换*/
  CAN1_ConfigurePins();
  // 进入 CAN初始化模式
  __HAL_RCC_CAN1_CLK_ENABLE();

  CLEAR_BIT(CAN1->MCR, CAN_MCR_SLEEP);
  SET_BIT(CAN1->MCR, CAN_MCR_INRQ);
  // 等待硬件设置INAK
  if (CAN1_WaitForMcrState(CAN_MSR_INAK, 1U) == 0U)
  {
    return 0U;
  }

  /* 配置工作模式
   * Bus-Off是CAN节点的自我保护机制：当控制器连续发生大量发送错误的时，会主动退出总线，不再发送报文，避免节点持续干扰其他设备
   * 导致进入Bus-Off的原因：1、CANH/CANL 接反 2、波特率不一致 3、USB-CAN 没有打开 4、USB-CAN 工作在 Silent 模式，不能返回 ACK
   * 5、总线没有正确终端电阻 6、总线上只有 STM32 一个发送节点，无其他节点 ACK
   * 每次发送失败后，发送错误计数器 TEC 会增长。错误持续发生时，控制器依次进入：Error Warning、Error Passive、Bus-Off
   * 进入Bus-Off后，控制器停止发送，不再影响总线。
   * MCR -- CAN主控制寄存器 、 ABOM -- Automatic Bus-Off Management、ABOM = 1：启用硬件自动 Bus-Off 管理
   * 启用后，控制器进入Bus-Off时不需要软件重新初始化CAN，硬件检测到总线连续出现规定数量的空闲状态--通常描述为128次“连续隐形位”
   * -- 就会自动退出Bus-off，重新参与通信。
   * */
  CAN1->MCR = CAN_MCR_INRQ | CAN_MCR_ABOM; // 代码没有设置 TXFP，因此 bxCAN 默认按照 CAN 标识符优先级选择待发送邮箱，而不是单纯按照邮箱0、1、2的顺序

  /* 配置500kbit/s位时序 */
  btr = (0U << 24) | (1U << 20) | (8U << 16) | 5U;
  CAN1->BTR = btr;

  /* Filter bank 0: 32-bit mask mode, accept all IDs into FIFO0. */
  SET_BIT(CAN1->FMR, CAN_FMR_FINIT);
  MODIFY_REG(CAN1->FMR, CAN_FMR_CAN2SB, (14U << CAN_FMR_CAN2SB_Pos));
  CLEAR_BIT(CAN1->FA1R, 1UL << 0);
  CLEAR_BIT(CAN1->FM1R, 1UL << 0);
  SET_BIT(CAN1->FS1R, 1UL << 0);
  CLEAR_BIT(CAN1->FFA1R, 1UL << 0);
  // 配置接收过滤器
  CAN1->sFilterRegister[0].FR1 = 0U;
  CAN1->sFilterRegister[0].FR2 = 0U;
  SET_BIT(CAN1->FA1R, 1UL << 0);
  CLEAR_BIT(CAN1->FMR, CAN_FMR_FINIT);
  /* 打开CAN中断
   * EWGIE:Error Warning 中断
   * EPVIE:Error Passive 中断
   * BOFIE:Bus-Off 中断
   * LECIE:最近错误类型变化中断
   * ERRIE:总错误中断开关
   * */
  CAN1->IER = CAN_IER_TMEIE | CAN_IER_FMPIE0 | CAN_IER_FOVIE0 |
              CAN_IER_EWGIE | CAN_IER_EPVIE | CAN_IER_BOFIE |
              CAN_IER_LECIE | CAN_IER_ERRIE;

  HAL_NVIC_SetPriority(CAN1_TX_IRQn, 1U, 0U);
  HAL_NVIC_EnableIRQ(CAN1_TX_IRQn);
  HAL_NVIC_SetPriority(CAN1_RX0_IRQn, 1U, 0U);
  HAL_NVIC_EnableIRQ(CAN1_RX0_IRQn);
  HAL_NVIC_SetPriority(CAN1_SCE_IRQn, 1U, 0U);
  HAL_NVIC_EnableIRQ(CAN1_SCE_IRQn);
  // 退出初始化模式
  CLEAR_BIT(CAN1->MCR, CAN_MCR_INRQ);
  if (CAN1_WaitForMcrState(CAN_MSR_INAK, 0U) == 0U)
  {
    return 0U;
  }

  can1_initialized = 1U;
  return 1U;
}

uint8_t CAN1_SendExtended(uint32_t extended_id, const uint8_t *data, uint8_t dlc)
{
  CAN_TxMailBox_TypeDef *mailbox;
  uint32_t mailbox_index;
  uint32_t low = 0U;
  uint32_t high = 0U;
  uint8_t i;
  // 串口没有初始化就发送、扩展帧ID超过29位、DLC超过8字节、DLC非0但数据指针为空
  if ((can1_initialized == 0U) || (extended_id > 0x1FFFFFFFUL) ||
      (dlc > 8U) || ((dlc > 0U) && (data == NULL)))
  {
    can1_statistics.tx_failed++;
    return 0U;
  }
  /* 发送邮箱调度选择，发送函数依次检查邮箱0,1,2（STM32F105自带的3个硬件发送邮箱）
   * TSR为发送状态寄存器，三个邮箱都忙时，本次发送直接失败，不会进入软件等待序列
   * */
  if ((CAN1->TSR & CAN_TSR_TME0) != 0U)
  {
    mailbox_index = 0U;
  }
  else if ((CAN1->TSR & CAN_TSR_TME1) != 0U)
  {
    mailbox_index = 1U;
  }
  else if ((CAN1->TSR & CAN_TSR_TME2) != 0U)
  {
    mailbox_index = 2U;
  }
  else
  {
    can1_statistics.tx_failed++;
    return 0U;
  }

  for (i = 0U; (i < dlc) && (i < 4U); i++)
  {
    low |= ((uint32_t)data[i]) << (8U * i);
  }
  for (i = 4U; i < dlc; i++)
  {
    high |= ((uint32_t)data[i]) << (8U * (i - 4U));
  }

  mailbox = &CAN1->sTxMailBox[mailbox_index];
  //找到空邮箱后写入
  mailbox->TDTR = (uint32_t)dlc;
  mailbox->TDLR = low; // Data[0]～Data[3]
  mailbox->TDHR = high; // Data[4]～Data[7]
  // CAN_TI0R_IDE将IDE置1，告诉CAN控制器，这是一帧29位扩展帧
  mailbox->TIR = (extended_id << 3) | CAN_TI0R_IDE;
  // 请求CAN控制器发送
  SET_BIT(mailbox->TIR, CAN_TI0R_TXRQ);
  /*发送请求后，CPU的工作基本完成，后续由bxCAN硬件自动完成：
   * 等待CAN总线空闲
   * 与本节点其他待发送邮箱比较优先级
   * 参加总线仲裁
   * 仲裁成功之后发送报文
   * 等待其他节点返回ACK
   * 成功后释放邮箱并触发中断
   * */
  can1_statistics.tx_queued++; // 表示成功写入硬件邮箱
  return 1U;
}

uint8_t CAN1_Read(CAN1_Frame *frame)
{
  uint8_t tail;

  if ((frame == NULL) || (can1_rx_tail == can1_rx_head))
  {
    return 0U;
  }

  tail = can1_rx_tail;
  memcpy(frame, (const void *)&can1_rx_queue[tail], sizeof(*frame));
  can1_rx_tail = (uint8_t)((tail + 1U) % CAN1_RX_QUEUE_SIZE);
  return 1U;
}

uint8_t CAN1_IsInitialized(void)
{
  return can1_initialized;
}
// Error
const char *CAN1_GetStateName(void)
{
  uint32_t esr;

  if (can1_initialized == 0U)
  {
    return "Not initialized";
  }
  /* 判断当前是否处于Bus-Off
   * CAN_ESR_BOFF:Bus-Off
   * CAN_ESR_EPVF:Error Passive
   * CAN_ESR_EWGF:Error Warning
   * */
  esr = CAN1->ESR;
  if ((esr & CAN_ESR_BOFF) != 0U)
  {
    return "Bus Off";
  }
  if ((esr & CAN_ESR_EPVF) != 0U)
  {
    return "Error Passive";
  }
  if ((esr & CAN_ESR_EWGF) != 0U)
  {
    return "Error Warning";
  }
  return "Error Active";
}
// TEC：发送错误计数器、REC：接收错误计数器
uint8_t CAN1_GetTEC(void)
{
  return (uint8_t)((CAN1->ESR & CAN_ESR_TEC) >> CAN_ESR_TEC_Pos);
}

uint8_t CAN1_GetREC(void)
{
  return (uint8_t)((CAN1->ESR & CAN_ESR_REC) >> CAN_ESR_REC_Pos);
}

uint8_t CAN1_GetLastErrorCode(void)
{
  return (uint8_t)((CAN1->ESR & CAN_ESR_LEC) >> CAN_ESR_LEC_Pos);
}

void CAN1_GetStatistics(CAN1_Statistics *statistics)
{
  if (statistics != NULL)
  {
    memcpy(statistics, (const void *)&can1_statistics, sizeof(*statistics));
  }
}

void CAN1_ClearStatistics(void)
{
  memset((void *)&can1_statistics, 0, sizeof(can1_statistics));
}

void CAN1_TxIRQHandler(void)
{
  uint32_t tsr = CAN1->TSR;
  uint32_t clear = 0U;
  // 检查每个邮箱的结果，RQCP0/1/2：对应邮箱的发送请求已经结束；TXOK0/1/2：对应邮箱发送成功；
  if ((tsr & CAN_TSR_RQCP0) != 0U)
  {
    can1_statistics.tx_ok += ((tsr & CAN_TSR_TXOK0) != 0U) ? 1U : 0U;
    can1_statistics.tx_failed += ((tsr & CAN_TSR_TXOK0) == 0U) ? 1U : 0U;
    clear |= CAN_TSR_RQCP0;
  }
  if ((tsr & CAN_TSR_RQCP1) != 0U)
  {
    can1_statistics.tx_ok += ((tsr & CAN_TSR_TXOK1) != 0U) ? 1U : 0U;
    can1_statistics.tx_failed += ((tsr & CAN_TSR_TXOK1) == 0U) ? 1U : 0U;
    clear |= CAN_TSR_RQCP1;
  }
  if ((tsr & CAN_TSR_RQCP2) != 0U)
  {
    can1_statistics.tx_ok += ((tsr & CAN_TSR_TXOK2) != 0U) ? 1U : 0U;
    can1_statistics.tx_failed += ((tsr & CAN_TSR_TXOK2) == 0U) ? 1U : 0U;
    clear |= CAN_TSR_RQCP2;
  }

  if (clear != 0U)
  {
    CAN1->TSR = clear;
  }
}

void CAN1_Rx0IRQHandler(void)
{
  while ((CAN1->RF0R & CAN_RF0R_FMP0) != 0U)
  {
    CAN1_Frame frame;
    uint32_t rir = CAN1->sFIFOMailBox[0].RIR;
    uint32_t rdtr = CAN1->sFIFOMailBox[0].RDTR;
    uint32_t low = CAN1->sFIFOMailBox[0].RDLR;
    uint32_t high = CAN1->sFIFOMailBox[0].RDHR;
    uint8_t next;
    uint8_t i;
    // 接收中断读取IDE位
    frame.is_extended = ((rir & CAN_RI0R_IDE) != 0U) ? 1U : 0U;
    frame.is_remote = ((rir & CAN_RI0R_RTR) != 0U) ? 1U : 0U;
    // 根据类型使用不同方式提取ID（IDE=0\IDE=1）
    frame.id = (frame.is_extended != 0U) ? (rir >> 3) : (rir >> 21);
    frame.dlc = (uint8_t)(rdtr & CAN_RDT0R_DLC);
    if (frame.dlc > 8U)
    {
      frame.dlc = 8U;
    }
    frame.timestamp_ms = HAL_GetTick();

    for (i = 0U; i < 4U; i++)
    {
      frame.data[i] = (uint8_t)(low >> (8U * i));
      frame.data[i + 4U] = (uint8_t)(high >> (8U * i));
    }

    next = (uint8_t)((can1_rx_head + 1U) % CAN1_RX_QUEUE_SIZE);
    if (next == can1_rx_tail)
    {
      can1_statistics.rx_dropped++;
    }
    else
    {
      memcpy((void *)&can1_rx_queue[can1_rx_head], &frame, sizeof(frame));
      can1_rx_head = next;
      can1_statistics.rx_received++;
    }

    CAN1->RF0R = CAN_RF0R_RFOM0;
  }

  if ((CAN1->RF0R & CAN_RF0R_FOVR0) != 0U)
  {
    can1_statistics.fifo_overrun++;
    CAN1->RF0R = CAN_RF0R_FOVR0;
  }
}
/* 保存发生错误时的 ESR 状态
 * 累加错误中断次数
 * 清除错误状态和中断标志
 * */
void CAN1_SceIRQHandler(void)
{
  can1_statistics.last_esr = CAN1->ESR;
  can1_statistics.error_irqs++;

  /* Write 111b to LEC to clear the bxCAN last-error field. */
  SET_BIT(CAN1->ESR, CAN_ESR_LEC);
  CAN1->MSR = CAN_MSR_ERRI;
}
