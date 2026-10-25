# LCD12864 模块设计与上板调试

## 1. 设计依据

板卡原理图和 QC12864B 手册表明，本板 LCD 使用 ST7920 兼容的三线串行模式：

| LCD 信号 | MCU 引脚 | LCD 引脚功能 |
|---|---|---|
| `LCD_CLK` | PA5 | E/SCLK |
| `LCD_SID` | PA6 | RW/SID |
| `LCD_CS` | PA7 | RS/CS |

- PSB 由板上电阻选择为串行模式，不需要 MCU 控制。
- LCD RST 由板上电阻拉高，没有连接 MCU。
- LCD 供电、背光和信号均在板内连接，上板测试不需要外接杜邦线。
- 串行写入一个命令或数据需要三个字节：同步字节、数据高四位、数据低四位。
- 命令同步字节为 `0xF8`，显示数据同步字节为 `0xFA`。
- 文本 DDRAM 四行起始地址为 `0x80`、`0x90`、`0x88`、`0x98`。

## 2. 软件分层

### 底层串行驱动

文件：`Core/Src/lcd12864.c`

负责：

- GPIO 初始化；
- 发送串行同步字节和两个半字节；
- 满足 CLK 建立、保持时间；
- 使用固定延时等待指令执行完成。

串行模式没有从 LCD 读取忙标志，因此驱动按照手册的 72 us 指令时间留出 1 ms，清屏指令额外等待 10 ms。所有 LCD 操作都在主循环上下文执行，禁止在中断中调用。

### 设备接口层

对外接口：

```c
uint8_t LCD12864_Init(void);
void LCD12864_Clear(void);
void LCD12864_WriteLine(uint8_t row, const char *text);
void LCD12864_TestPattern(void);
void lcd12864_display(uint8_t row, uint8_t col,
                      const uint8_t *buffer, uint8_t length);
```

`lcd12864_display()` 满足课题要求：

- `row` 范围 0～3；
- `col` 范围 0～15；
- 超出行末的内容自动截断；
- 支持 ASCII `0x20～0x7E`；
- 不支持的字符显示为 `?`；
- 空指针、非法行列和零长度不会访问 LCD。

### 4×16 影子缓冲区

LCD 每个 DDRAM 地址对应两个 ASCII 字符，直接从奇数列开始写入不方便。驱动在 RAM 中维护 `4×16` 字符缓冲区：

1. 修改指定行列的字符；
2. 将该行完整 16 字符重新发送；
3. 因此 `col=1`、`col=3` 等奇数列也能稳定工作。

### Shell 测试层

新增命令：

| 命令 | 功能 |
|---|---|
| `lcdtest` | 显示固定四行测试图案 |
| `lcdclear` | 清屏并清空影子缓冲区 |
| `lcdinit` | 重新发送完整初始化序列 |
| `lcdshow row col text` | 在指定行列显示 ASCII 文本 |

## 3. 烧录前检查

1. 确认 LCD 模块实际焊装在板上。
2. 给开发板正常供电，不要只依赖 J-Link VTref。
3. USB-TTL 保持连接 UART4，参数为 115200、8N1、无流控。
4. 在 CubeIDE 中右键工程执行 `Refresh`，应看到：

   ```text
   Core/Inc/lcd12864.h
   Core/Src/lcd12864.c
   ```

5. 执行 `Clean Project` 和 `Build Project`，然后下载 `Debug/demo.elf`。
6. 按 F8/Resume 运行，不要让 MCU 停在断点上。

## 4. 启动预期结果

串口应显示：

```text
CANBox UART Shell V1.4.0
LCD12864 initialization sequence sent
CAN1 ready: 500K extended frame, normal mode
Type help or h for commands.
canbox>
```

LCD 应显示：

```text
CANBox V1.4.0
UART4 115200
LCD INIT SENT
Type lcdtest
```

## 5. 分阶段测试

### 测试 1：背光和供电

开发板上电后先观察背光：

- 背光不亮：优先检查板卡电源、LCD 是否焊装、背光电源；此时不应先怀疑显示代码。
- 背光亮但没有字符：继续检查对比度、串行模式和 CLK/SID/CS 波形。
- 有黑块但字符错误：通常是初始化、对比度或串行时序问题。

### 测试 2：固定测试图案

串口发送：

```text
lcdtest
```

预期显示：

```text
CANBox LCD Test
0123456789ABCDEF
Cviauto_6
LCD PASS?
```

这一项同时检查四行地址映射、数字、大小写字母和下划线。

### 测试 3：指定位置显示

依次发送：

```text
lcdclear
lcdshow 0 0 ABCD
lcdshow 1 3 1234
lcdshow 2 1 Cviauto_6
lcdshow 3 15 Z
```

检查内容是否出现在正确的行列，尤其验证奇数列 1、3、15。

### 测试 4：边界条件

```text
lcdshow 0 15 ABC
```

只能显示 `A`，后面的字符被截断。

```text
lcdshow 4 0 ERROR
lcdshow 0 16 ERROR
```

串口应返回 usage 错误，不应写越界内存。

### 测试 5：重复初始化

```text
lcdinit
lcdtest
```

用于判断上电初始化时序问题。如果启动时无显示、执行 `lcdinit` 后恢复，说明 LCD 上电稳定时间还需要继续加长。

## 6. 常见故障定位

### 背光亮，完全没有字符

按以下顺序排查：

1. 执行 `lcdinit` 后再执行 `lcdtest`；
2. 检查 LCD 底部 VO 对比度电阻/可调位置；
3. 确认 PSB 为低电平串行模式；
4. 用示波器或逻辑分析仪检查 PA5、PA6、PA7；
5. PA5 应有时钟，PA6 应有数据，PA7 每组三字节期间为高电平。

### 第一、二行正常，第三、四行错位

检查行地址必须为：

```text
row 0 -> 0x80
row 1 -> 0x90
row 2 -> 0x88
row 3 -> 0x98
```

不能简单地按 `0x80/0x90/0xA0/0xB0` 递增。

### 字符只显示一半或乱码

检查每个值是否按以下三个字节发送：

```text
同步字节
原数据高四位（低四位补 0）
原数据低四位（移动到高四位）
```

### LCD 操作导致 Shell 短暂停顿

当前驱动为了首次上板稳定，使用了保守的 1 ms 固定等待。刷新整行约需要 17 ms，属于预期现象。确认硬件稳定后，可以改为基于微秒定时器的 72 us 等待，提高刷新速度。

## 7. 验收记录建议

保存以下证据：

- LCD 四行测试图案照片；
- 串口执行 `lcdtest`、`lcdshow` 的截图；
- 非法 row/col 被拒绝的截图；
- 工程 `0 errors, 0 warnings` 编译结果；
- 若有示波器，保存 CLK/SID/CS 三线时序截图。

完成这些测试后，可以认为 LCD 基础驱动、统一接口和 Shell 调试入口已经通过，再进入按键扫描与页面切换开发。
