# DBC真实发送与周期调度测试（V1.5.0）

## 新增功能

V1.5.0增加以下Shell命令：

```text
dbcsend <packet 1..6> <value>
dbcset <packet 1..6> <value>
dbctx on
dbctx off
dbctx stat
```

- `dbcsend`：DBC编码后立即发送一帧。
- `dbcset`：设置CanBox业务信号值，超范围时自动截断。
- `dbctx`：控制并查看自动周期发送。

CAN1初始化成功后，以下报文默认自动发送：

| 报文 | ID | 周期 |
|---|---:|---:|
| CB1 | 18FF2517 | 500 ms |
| CB2 | 18FF2518 | 1000 ms |
| CB5 | 18FF251B | 1000 ms |
| CB6 | 18FF251C | 300 ms |

CB3和CB4保持请求发送，使用`dbcsend`触发。周期任务使用系统毫秒时基，不使用阻塞延时，也不会持续打印串口。

## PCAN-View验收

1. 烧录V1.5.0，确认启动信息出现`DBC periodic TX ready`。
2. PCAN-View确认自动收到18FF2517、18FF2518、18FF251B、18FF251C。
3. 输入`dbcset 1 3`，确认18FF2517的数据变为`03 00 00 00 00 00 00 00`。
4. 输入`dbcset 2 3`，确认18FF2518的数据变为`00 00 0C 00 00 00 00 00`。
5. 输入`dbcset 5 3`，确认18FF251B最后一个字节变为`03`。
6. 输入`dbcset 6 3`，确认18FF251C最后一个字节变为`C0`。
7. 输入`dbcsend 3 15`和`dbcsend 4 7`，确认收到18FF2519和18FF251A。
8. 执行`canstat`和`dbctx stat`，确认无发送失败或邮箱忙。

## 控制命令

```text
dbctx off
```

停止四条周期报文；再次输入`dbctx on`后从当前时刻重新计时发送。

