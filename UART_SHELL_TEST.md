# UART4 Shell 上板测试

## 本次新增功能

- UART4 中断接收，使用 128 字节环形缓冲区。
- 串口输入回显、退格和回车处理。
- 单行最大 63 个可见字符，超长输入会报告错误。
- 支持 UART、CAN、LCD 和 ADC 按键调试命令；完整列表输入 `help` 查看。
- 主循环不再连续输出 `welcome to cviauto`。

## 烧录文件

`Debug/demo.hex`

## 串口参数

```text
波特率：115200
数据位：8
停止位：1
校验位：None
流控：None
发送格式：ASCII
```

接线：板卡 UART4_TXD 接 USB-TTL RXD，板卡 UART4_RXD 接 USB-TTL TXD，GND 接 GND。板卡已经单独供电时，不连接 USB-TTL 的 VCC。

## 预期启动信息

```text
CANBox UART Shell V1.4.0
LCD12864 initialization sequence sent
Type help or h for commands.
canbox>
```

## 测试步骤

1. 输入 `help` 并发送回车，应显示全部命令。
2. 输入 `h`，结果应与 `help` 相同。
3. 输入 `checkVer`，应返回 `V1.4.0`。
4. 输入 `canstat`，应返回 CAN1 状态和收发计数。
5. 输入 `abc`，应返回 `ERR unknown command: abc`。
6. 测试退格键，串口显示和最终命令内容都应正确。
7. 输入超过 63 个字符，应返回 `ERR line too long`，系统不应死机。
8. 最后输入 `reset`，MCU 应复位，并重新显示启动信息。

## 下一阶段

CAN1 与 LCD12864 模块均已加入工程；CAN 上板步骤见 `CAN1_DEBUG_TEST.md`，LCD 上板步骤见 `LCD12864_DESIGN_AND_TEST.md`。
