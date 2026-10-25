# CAN1 + UART4 联调说明

## 1. 当前代码功能

- CAN1 使用 PA11（RX）和 PA12（TX）。
- APB1 时钟 36 MHz，CAN 波特率 500 Kbps。
- 位时间：Prescaler=6、BS1=9 Tq、BS2=2 Tq、SJW=1 Tq。
- 每位 12 Tq，采样点 83.33%。
- CAN 工作在 Normal 模式，自动重发和自动 Bus-Off 恢复已启用。
- 过滤器在当前调试阶段接收全部标准帧和扩展帧。
- 接收中断只负责将报文放入 16 帧环形队列，报文打印在主循环中完成。
- 发送命令固定发送 29 位扩展数据帧。
- PC6（STB_CAN）输出低、PB15（EN_CAN）输出高，使能板载高速 CAN 收发器。

## 2. 接线

### USB-CAN 与开发板

| USB-CAN | 开发板 |
|---|---|
| CANH | CANH |
| CANL | CANL |
| GND（如果模块提供） | GND |

注意：

1. CANH 必须接 CANH，CANL 必须接 CANL，不能交叉。
2. 如果 USB-CAN 提供 GND，建议与开发板 GND 共地，尤其是非隔离 USB-CAN。
3. 如果 USB-CAN 确实只有 CANH/CANL 两个端子，则只能连接这两根线；这类模块通常已经隔离或没有引出参考地。先用短线调试。
4. 不要把 USB-CAN 或 USB-TTL 的 VCC 接到已经单独供电的开发板。
5. 板载 CAN 收发器还需要开发板正常供电。只连接 J-Link 的 VTref/VCC 检测脚不能代替开发板电源。

### USB-TTL 与开发板 UART4

| USB-TTL | 开发板 GOLD_DEBUG/UART4 |
|---|---|
| RXD | TXD |
| TXD | RXD |
| GND | GND |
| VCC | 不接（开发板已单独供电时） |

UART4 与 CAN1 是不同外设，可以同时连接电脑，不会互相冲突。

## 3. 板上拨码开关

根据 `G.ICS0705.CAN.1C23451.pdf` 原理图：

- SW3 用于选择 CAN1 连接高速还是低速收发器。使用高速 CANH/CANL 时，闭合高速通道 CAN1_H_TX 和 CAN1_H_RX，也就是 SW3 的 1、3 通道；低速通道 2、4 断开。
- SW4 的 2、4 通道用于将 CAN1 高速收发器的 `Loop_CAN_H/Loop_CAN_L` 接到外部 `CAN_H/CAN_L`。
- SW4 的 1、3 通道控制板载分裂终端电阻。开发板和 USB-CAN 是总线两端且 USB-CAN 端也启用 120 欧终端时，可闭合 1、3。

拨码编号和 ON 方向应以板上丝印为准。断电后用万用表测 CANH 与 CANL：总线两端均有 120 欧终端时，总电阻应约为 60 欧。若约 120 欧表示只有一端终端；若接近 0 欧说明接线或开关设置错误。

## 4. PC 软件参数

### 串口助手

- 115200 baud
- 8 data bits
- 1 stop bit
- no parity
- no flow control
- 建议发送结尾选择 CRLF；代码同时兼容 CR、LF、CRLF。

### USB-CAN 软件

- Channel：实际连接的通道
- Bit rate：500 Kbps
- Mode：Normal/Active，不能使用 Silent/Listen-only
- Frame format：测试发送时选择 Extended（29-bit）
- Frame type：Data frame

Normal 模式下，发送节点需要另一个正常工作的节点返回 ACK。因此 USB-CAN 必须打开并工作在 Active 模式，否则 STM32 的 TEC 会不断升高并最终 Bus-Off。

## 5. 烧录后测试步骤

1. 断电完成 CANH、CANL、建议的 GND、UART4 和拨码连接。
2. 给开发板正常供电，连接 USB-TTL 和 USB-CAN。
3. 在 STM32CubeIDE 中对 `demo` 执行 `Refresh`、`Clean Project`、`Build Project`。
4. 使用 J-Link 下载 `Debug/demo.elf`，点击 Resume/F8 让程序运行。
5. 打开串口助手，应看到：

   ```text
   CANBox UART Shell V1.4.0
   LCD12864 initialization sequence sent
   CAN1 ready: 500K extended frame, normal mode
   Type help or h for commands.
   canbox>
   ```

6. 串口发送 `canstat`，未发生错误时应显示 `Error Active`、TEC=0、REC=0。
7. 串口发送：

   ```text
   cansend 18FF50E5 01 02 03 04 05 06 07 08
   ```

   USB-CAN 软件应收到扩展 ID `18FF50E5`、DLC 8、数据 `01 02 03 04 05 06 07 08`。
8. 在 USB-CAN 软件中发送扩展帧：

   ```text
   ID   = 18FF51E5
   DLC  = 8
   DATA = 11 22 33 44 55 66 77 88
   ```

   串口应自动显示类似：

   ```text
   CAN RX EXT ID=18FF51E5 DLC=8 DATA=11 22 33 44 55 66 77 88
   ```

9. 再执行 `canstat`，检查 TX ok 和 RX 计数是否增加。

## 6. Shell 命令

| 命令 | 作用 |
|---|---|
| `help` / `h` | 显示命令列表 |
| `checkVer` | 显示固件版本 |
| `canstat` | 显示 CAN 状态、TEC/REC、LEC 和收发计数 |
| `cansend ID [DATA...]` | 发送一条 29 位扩展帧；ID 和数据使用十六进制 |
| `canrx on` | 打开接收报文串口打印 |
| `canrx off` | 关闭打印，但仍接收并取出队列中的报文 |
| `canclear` | 清除软件收发统计计数 |
| `reset` | MCU 软件复位 |

`cansend` 示例：

```text
cansend 123 01 02
cansend 0x18FF50E5 AA BB CC DD
cansend 18FF50E5
```

## 7. 常见故障判断

### TX queued 增加，但 TX ok 不增加

随后 `TEC` 快速增加，通常表示没有节点 ACK。检查：

- USB-CAN 是否已经打开通道；
- 是否为 500 Kbps；
- 是否误设为 Silent/Listen-only；
- CANH/CANL 是否接反；
- SW3、SW4 是否选择了 CAN1 高速路径；
- 总线是否有两端 120 欧终端；
- 板载 CAN 收发器是否得到正常板卡电源。

### 串口显示 Bus Off

先修正接线、波特率或 USB-CAN 模式，再执行 `reset`。代码启用了自动 Bus-Off 恢复，但重新复位更方便从零观察统计数据。

### USB-CAN 能看到 STM32 报文，但 STM32 看不到 USB-CAN 报文

- 确认 USB-CAN 确实点击了发送；
- 确认 USB-CAN TX 计数增加且没有报错；
- 检查 CAN1_RX 的高速通道开关；
- 执行 `canrx on`；
- 执行 `canstat` 查看 REC、RX 和 overrun。

### LEC 数值含义

- 0：No Error
- 1：Stuff Error
- 2：Form Error
- 3：ACK Error
- 4：Bit Recessive Error
- 5：Bit Dominant Error
- 6：CRC Error
- 7：软件设置/无有效最近错误

## 8. 工程注意事项

CAN1 模块采用 STM32 bxCAN 寄存器实现，没有依赖额外的 HAL CAN 源文件。当前 `demo.ioc` 尚未启用 CubeMX 的 CAN 外设配置。如果以后通过 CubeMX 重新生成工程，不要再生成另一套同名 CAN 初始化代码；应先决定继续使用本模块，还是整体迁移到 HAL CAN，然后再统一维护。
