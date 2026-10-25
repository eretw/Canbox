# TIM3 1 ms 调度与 ADC 按键测试说明

## 1. 本次实现内容

- TIM3 从原来的 500 ms 中断改为 1 ms 中断。
- TIM3 中断只累加 `app_time_ms`，业务均放在主循环。
- ADC 按键每 10 ms 采样；PC3 指示灯每 500 ms 翻转。
- ADC1 轮询采集三组板载电阻梯按键：
  - PC4 / ADC1_IN14：Confirm、Left、Right；
  - PC5 / ADC1_IN15：Cancel、Up、Down；
  - PB0 / ADC1_IN8：Reverse、Next Page、Previous Page。
- 每通道取 4 个样本平均，按下和释放均进行 40 ms 消抖。
- 松手时产生一次事件：小于 2000 ms 为 SHORT，否则为 LONG。
- 多组同时按下、ADC 落入无效区或转换超时均按 `INVALID` 处理。
- 最近一次按键事件显示在 LCD 第 4 行，并可通过 UART4 输出。

## 2. 时钟参数

```text
TIM3 counter clock = 72 MHz / (71 + 1) = 1 MHz
TIM3 update rate   = 1 MHz / (999 + 1) = 1 kHz
TIM3 interrupt     = 1 ms
ADC clock          = PCLK2 72 MHz / 6 = 12 MHz
```

## 3. 理论 ADC 值与默认阈值

| 下拉电阻 | 理论 ADC 值 | 默认范围 |
|---:|---:|---:|
| 100 Ω | 约 85 | 0～600 |
| 4.7 kΩ | 约 2048 | 1500～2250 |
| 10 kΩ | 约 2786 | 2400～3200 |
| 未按键 | 接近 4095 | 3500～4095 |

实板存在误差，必须用 `keyadc` 查看实测值。如果落在区间外，再调整 `adc_keys.c` 顶部阈值宏。

## 4. 启动预期

串口参数：115200、8 data bits、1 stop bit、no parity、no flow control。

```text
CANBox UART Shell V1.4.0
ADC keypad ready: PC4/PC5/PB0, scan period 10ms
LCD12864 initialization sequence sent
...
canbox>
```

LCD 显示：

```text
CANBox V1.4.0
UART4 115200
KEYPAD READY
Press any key
```

## 5. Shell 命令

### `keyadc`

显示三个 ADC 原始值和未经消抖的键值：

```text
KEY ADC: PC4/CH14=4092 PC5/CH15=4091 PB0/CH8=4093 raw=NONE
```

### `keystat`

显示消抖后的稳定按键、已按住时间和 1 ms 系统节拍：

```text
KEY stable=UP duration=1260ms tick=35742ms
```

### `keymon on` / `keymon off`

打开或关闭事件自动打印。默认开启，示例：

```text
KEY UP SHORT duration=326ms
KEY NEXT_PAGE LONG duration=2537ms
```

关闭打印不会停止按键采集或 LCD 事件显示。

## 6. 推荐测试步骤

1. 烧录 `Debug/demo.elf`，确认启动版本为 V1.4.0。
2. 不按键发送 `keyadc`，三个值应大于 3500。
3. 分别按住九个键并发送 `keyadc`，记录通道值和 `raw` 名称。
4. 每个键短按一次，松手后应只输出一次 `SHORT`。
5. 按住任意键超过 2 秒再松开，应只输出一次 `LONG`。
6. 按住期间发送 `keystat`，`duration` 应持续增加。
7. LCD 第四行应显示最近一次按键名称和 SHORT/LONG。
8. 连续快速按键时 UART Shell、LCD 和 CAN 接收处理不能卡死。

## 7. 故障定位

- 三个通道为 65535：ADC 转换超时，检查是否烧录 V1.4.0。
- 未按键小于 3500：检查卡键、按键网络或释放阈值。
- 显示 `INVALID`：可能位于阈值空档、同时按了多组键或 ADC 超时。
- 键名和丝印不一致：根据 `keyadc` 实测结果修改 `ADC_Keys_Map()`，不要只改输出文字。

## 8. 主要接口

```c
uint8_t ADC_Keys_Init(void);
void ADC_Keys_SampleTask(void);
KeyRaw ADC_Keys_GetRaw(void);
uint16_t ADC_Keys_GetAdc(uint8_t group);

void KeyService_Init(uint32_t now_ms);
void KeyService_Task(uint32_t now_ms);
uint8_t KeyService_GetEvent(KeyEvent *event);
```

驱动层负责 ADC 与原始键值；服务层负责消抖、持续时间和短按/长按，避免业务逻辑与硬件采集耦合。
