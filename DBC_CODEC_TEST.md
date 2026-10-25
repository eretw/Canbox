# DBC 编解码模块与串口测试说明

## 1. 模块作用

DBC 编解码模块负责在“业务信号值”和“CAN 的 8 字节数据”之间转换：

```text
业务信号值 -> DBC编码 -> CAN扩展帧数据
CAN扩展帧数据 -> ID/DLC检查 -> DBC解码 -> 业务信号值
```

本阶段不自动周期发送 CAN，只完成报文定义、编码、解码、多帧重组和串口离线验证。这样在没有 USB-CAN 软件时，也能先证明位位置、边界截断和校验算法正确。

## 2. 新增文件

```text
Core/Inc/dbc_codec.h
Core/Src/dbc_codec.c
```

固件版本更新为 `V1.4.0`。

## 3. PC 发给 CANBox 的报文

| 报文 | 扩展帧ID | 周期 | 解码内容 |
|---|---:|---:|---|
| PC_Packet_1 | 0x0CFFD221 | 500 ms | 多帧数据包、逐帧checksum |
| PC_Packet_2 | 0x0CFFD222 | 20 ms | Byte1，8 bit |
| PC_Packet_3 | 0x0CFFD223 | 50 ms | Byte2，8 bit |
| PC_Packet_4 | 0x0CFFD224 | 100 ms | Byte3，8 bit |
| PC_Packet_5 | 0x0CFFD225 | 500 ms | Byte4，8 bit |
| PC_Packet_6 | 0x0CFFD226 | 1500 ms | Byte5，8 bit |

PC_Packet_2～6 必须满足：扩展帧、正确ID、DLC等于8。成功解码后会保存数值、有效标志和最后更新时间。尚未收到的数据在 `dbcshow` 中显示 `#`。

## 4. CANBox 发给 PC 的报文

| 报文 | 扩展帧ID | 发送方式 | 信号 | DBC位置 | 范围 |
|---|---:|---|---|---|---:|
| CanBox_Packet_1 | 0x18FF2517 | 500 ms | DevIx | 1.1，2 bit | 0～3 |
| CanBox_Packet_2 | 0x18FF2518 | 1000 ms | UnitSysSet | 3.3，2 bit | 0～3 |
| CanBox_Packet_3 | 0x18FF2519 | OnRequest | CallFromLogIx | 5.1，4 bit | 0～15 |
| CanBox_Packet_4 | 0x18FF251A | OnRequest | MutePrioReq | 6.1，3 bit | 0～7 |
| CanBox_Packet_5 | 0x18FF251B | 1000 ms和OnRequest | AudioMute | 8.1，2 bit | 0～3 |
| CanBox_Packet_6 | 0x18FF251C | 300 ms和OnRequest | KeyACC | 8.7，2 bit | 0～3 |

表格中的位置是1基“字节.位”，代码内部统一转换为0基 Intel start bit。未定义的数据位编码为0。

如果输入超过DBC最大值，编码器会截断到最大值并打印 `(clamped)`，不会让高位覆盖相邻信号。

## 5. PC_Packet_1 多帧规则

首帧：

```text
Byte1 = 0x01
Byte2 = 总数据长度
Byte3～Byte7 = 前5个数据
Byte8 = checksum
```

后续帧：

```text
Byte1 = 帧序号，从0x02递增
Byte2～Byte7 = 后续6个数据
Byte8 = checksum
```

校验算法：

```c
temp = Byte1 + Byte2 + Byte3 + Byte4 + Byte5 + Byte6 + Byte7;
checksum = (((temp >> 6) & 0x03) + (temp >> 3) + temp) & 0x07;
```

状态机检查：

- DLC必须为8；
- checksum必须完全一致；
- 首帧序号必须为1；
- 后续帧必须按顺序递增；
- 相邻帧间隔超过1000 ms时拒绝；
- 最大重组数据长度255字节；
- 异常时终止当前数据包，等待新的首帧。

## 6. Shell命令

### 查看DBC定义

```text
dbcinfo
```

### 离线编码CANBox报文

```text
dbcencode <packet 1..6> <value>
```

示例：

```text
dbcencode 2 3
UnitSysSet requested=3 encoded=3
ID=18FF2518 DLC=8 DATA=00 00 0C 00 00 00 00 00
```

边界测试：

```text
dbcencode 4 99
MutePrioReq requested=99 encoded=7 (clamped)
ID=18FF251A DLC=8 DATA=00 00 00 00 00 07 00 00
```

### 离线解码PC报文

```text
dbcdecode <packet 2..6> <8个十六进制字节>
```

示例：

```text
dbcdecode 4 00 00 5A 00 00 00 00 00
DBC decoded PC4.Data=90
```

### 计算多帧checksum

```text
dbccrc <7个十六进制字节>
```

示例：

```text
dbccrc 01 08 01 02 03 04 05
```

返回的值放在PC_Packet_1的Byte8。

### 查看当前接收状态

```text
dbcshow
```

未接收时：

```text
PC2.Data=#
PC3.Data=#
PC4.Data=#
PC5.Data=#
PC6.Data=#
```

### 运行内置自测

```text
dbctest
```

预期：

```text
DBC self-test: passed=7 failed=0 PASS
```

自测覆盖：Intel位编码、跨位置取值、最大值截断、DLC拒绝、普通报文解码、多帧顺序和多帧重组。

## 7. 实际CAN接收时的作用

现有 `CAN_ProcessReceived()` 已接入DBC解析：

- 收到0x0CFFD222～0x0CFFD226时自动提取对应Data值；
- 收到0x0CFFD221时自动进入多帧重组；
- ID不在DBC表中时只作为普通CAN帧处理；
- DLC错误、checksum错误、错序或超时时输出拒绝原因；
- 执行 `canrx off` 只关闭串口打印，不会停止DBC状态更新。

## 8. 推荐验收步骤

1. 烧录V1.4.0固件。
2. 输入 `dbctest`，确认7项全部通过。
3. 输入 `dbcinfo`，核对12个扩展帧ID。
4. 对6条CANBox报文分别执行最小值、最大值和越界编码。
5. 对PC_Packet_2～6分别构造数据并执行 `dbcdecode`。
6. 输入 `dbcshow`，确认有效值与未接收的 `#` 状态正确。
7. 后续取得USB-CAN软件后，再验证真实总线收发和周期精度。
