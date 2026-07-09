# PCM 捕获验证程序 — 执行流程说明

本文档按代码执行顺序，逐行讲解 `test_pcm_capture.cpp` 中 PCM 数据捕获的完整流程。涉及 `shared.h` 和 `shared.cpp` 中的函数时同步展开。每个 ST3020 系统 API 均标注手册页数。

> **手册**：`第8版（数字+模拟）系统手册（含补充说明）新增安全注意事项(1).pdf`

---

## 一、文件概述

**文件**：`test_pcm_capture.cpp`
**编译**：`shared.cpp` + `test_pcm_capture.cpp` 一同加入工程
**用途**：上机调试第一步，在跑 AC 测试之前验证 PCM 数据读取链路是否正常。

**验证目标**：

1. `GET_PATTERN_RESULT` 能否正确获取 fail memory
2. PCM 字节的位提取逻辑是否正确
3. 多字节连续捕获是否稳定

**配置宏**：

```c
#define PCM_TEST_PATTERN    10   // 图形 10 = "1020Hz 0dBm0 预存 PCM 码流"
#define CAPTURE_COUNT        32   // 捕获字节数
#define USE_AS_SOURCE        0    // 0=SET_AS, 1=LOAD_AS_PATTERN
```

---

## 二、编译预处理 — `#include "shared.h"`

```c
#include "shared.h"
```

编译器将 `shared.h` 全部内容插入。关键内容包括：

### 系统头文件（shared.h 第 12-16 行）

```c
#include "StdAfx.h"    // VC6 预编译头，必须第一个包含
#include "testdef.h"   // ST3020 平台类型：BOOL, BYTE, WORD 等
#include "data.h"      // ST3020 常量：DPS1, DPS2, V, MA, HZ, NRZ0 等
#include "math.h"      // sqrt, sin, cos, pow, log10
```

### 物理常量（shared.h 第 17-19 行）

```c
#define VPEAK_FS     2.492       // TP3057 满量程峰值电压 (V)
#define LINEAR_MAX   8031        // 13-bit 线性编码最大值
#define M_PI         3.14159265358979323846
```

### 公共函数声明（shared.h 第 54-96 行）

本文件使用的函数：

| 函数                          | 位置（定义） | 用途            |
| ----------------------------- | :-----------: | --------------- |
| `SETUP()`                   | shared.cpp:15 | 系统初始化      |
| `capture_one_pcm_byte()`    | shared.cpp:31 | 单字节 PCM 捕获 |
| `capture_multi_pcm_bytes()` | shared.cpp:60 | 多字节 PCM 捕获 |

---

## 三、程序入口与变量声明

```c
void PASCAL TP3057()
```

`PASCAL` 是 `__stdcall` 宏，`TP3057` 是 ST3020 平台约定的入口函数名。

### C95 变量声明块（第 49-66 行）

所有变量在块首声明（C95 要求）：

```c
int pcm_single;                    // 单字节捕获结果（>=0 成功，<0 失败）
double vrms_single;                // 单字节解码后电压值
unsigned char pcm_buf[CAPTURE_COUNT];  // 多字节缓冲区（32 = CAPTURE_COUNT）
int captured_multi_pcm;            // 实际捕获到的字节数
int show_count;                    // 逐字节显示数量（最多 16）
int i;                             // 循环变量
double v_min, v_max, v_rms_sum;    // 统计变量
double v_rms_total, v_peak_max;    // 统计结果
int zero_or_ff, d5_count;          // 异常字节计数器
char name_hex[32], name_v[32];     // sprintf 标签缓冲区
BYTE* fb;                          // fail memory 指针（诊断用）
int dp;                            // fail memory 深度（诊断用）
double v_by;                       // 循环中的临时电压值
```

`pcm_single` 类型为 `int`，与 `capture_one_pcm_byte` 的 `int` 返回值对应：**≥0 表示成功**（值为 PCM 字节 0~255），**<0 表示失败**。

---

## 四、系统初始化 — `SETUP()`（第 68 行）

```c
SETUP();
```

展开到 [shared.cpp:15-28]：

### 4.1 双电源配置（shared.cpp:17-18）

```c
SET_DPS(DPS1, 5.0, V, 100, MA);    // VCC = +5V, 限流 100mA
SET_DPS(DPS2, -5.0, V, 100, MA);   // VBB = -5V, 限流 100mA
```

> 📖 **`SET_DPS` — 手册 第 40 页**
>
> ```c
> void SET_DPS(BYTE No, double Vdd, unsigned int Vdd_Unit,
>              double Iclamp, unsigned int Iclamp_Unit);
> ```
>
> - `No`：通道号（`DPS1` / `DPS2`）
> - `Vdd`：输出电压（范围 ±15V）
> - `Iclamp`：限流值（最大 250mA）

TP3057 需要双电源 ±5V。

### 4.2 稳定延时（shared.cpp:19）

```c
Delay(10);
```

> 📖 **`Delay` — 手册 第 51 页**
>
> ```c
> void Delay(double fMs);
> ```
>
> - `fMs`：延时时长（ms），`double` 类型，`Delay(0.1)` = 100μs

### 4.3 周期设置（shared.cpp:20）

```c
SET_PERIOD(488);   // 488ns ≈ 2.048MHz
```

> 📖 **`SET_PERIOD` — 手册 第 47 页**
>
> ```c
> void SET_PERIOD(unsigned int period);
> ```
>
> - `period`：测试周期（ns），范围 50ns ~ 10ms
>
> TP3057 主时钟 2.048MHz → 周期 = 1÷2.048MHz ≈ 488.28ns，取整 488ns。

### 4.4 时序设置（shared.cpp:21）

```c
SET_TIMING(100, 350, 450);
```

> 📖 **`SET_TIMING` — 手册 第 47 页**
>
> ```c
> void SET_TIMING(double LeadEdge, double EndEdge, double Ctg);
> ```
>
> - `LeadEdge`（100ns）：比较沿，ATE 在此刻采样输入引脚电平
> - `Ctg`（350ns）：锁存沿
> - `EndEdge`（450ns）：输出驱动切换时刻

一个 488ns 测试周期内的时序：

```
0ns    100ns         350ns  450ns   488ns
|------|=============|------|=======|
       比较沿         锁存沿  终止沿
```

### 4.5 输入输出电平（shared.cpp:22-23）

```c
SET_INPUT_LEVEL(2.2, 0.6);   // VIH=2.2V, VIL=0.6V
SET_OUTPUT_LEVEL(2.4, 0.4);  // VOH=2.4V, VOL=0.4V
```

> 📖 **`SET_INPUT_LEVEL` — 手册 第 44 页**
>
> ```c
> void SET_INPUT_LEVEL(double Vih, double Vil);
> ```
>
> - `Vih`：高于此电压判为逻辑 1
> - `Vil`：低于此电压判为逻辑 0

> 📖 **`SET_OUTPUT_LEVEL` — 手册 第 44-45 页**
>
> ```c
> void SET_OUTPUT_LEVEL(double Voh, double Vol);
> ```
>
> - `Voh` / `Vol`：ATE 驱动输出的高低电平

### 4.6 引脚格式（shared.cpp:24-25）

```c
FORMAT(NRZ0, "48,4,47,46,1,2");   // 不归零，驱动输出
FORMAT(RZ, "45,44,3");            // 归零格式
```

> 📖 **`FORMAT` — 手册 第 43 页**
>
> ```c
> void FORMAT(BYTE fmt, CString csChannel);
> ```
>
> - `fmt`：`NRZ`=不归零，`RZ`=归零，`RO`=反相归零，`NRZ0`=不归零全部屏蔽
> - `csChannel`：引脚列表，逗号分隔

引脚分配：

| 引脚 | 信号  | 功能                               |
| :--: | ----- | ---------------------------------- |
|  48  | TSX   | 发送时隙                           |
|  47  | FSX   | 发送帧同步                         |
|  46  | DX    | 发送数据输出（**PCM 数据**） |
|  45  | MCLKX | 发送主时钟                         |
|  44  | BCLKX | 发送位时钟                         |
|  4  | PDN   | 掉电控制                           |
|  3  | BCLKR | 接收位时钟                         |
|  2  | DR    | 接收数据输入                       |
|  1  | FSR   | 接收帧同步                         |

### 4.7 上电图形（shared.cpp:26）

```c
RUN_PATTERN(3, 1, 0, 0);   // 图形 3: 上电，取消 DX 高阻态
```

> 📖 **`RUN_PATTERN` — 手册 第 45-46 页**
>
> ```c
> BOOL RUN_PATTERN(int start_idx, int get_fail, int apgen, int time_range);
> ```
>
> - `start_idx`：图形索引
> - `get_fail`：`0`=GO（只跑不记录），`1`=NOGO（记录 fail memory）
> - `apgen`：APG 使能（0=NOAPG）
> - `time_range`：超时时间（ms，0=默认 30ms）
> - **返回值**：`BOOL`，1=PASS，0=FAIL

```c
Delay(10);
```

`SETUP()` 完成后返回，再等 20ms：

```c
Delay(20);   // 回到 test_pcm_capture.cpp:69
```

---

## 五、激励源设置（第 95-96 行）

`USE_AS_SOURCE=0`，走传统 `SET_AS` 分支：

```c
SET_AS(1.2276, V, 1020, HZ);
```

> 📖 **`SET_AS` — 手册 第 90 页**
>
> ```c
> void SET_AS(double fVol, int iVolUnit, double freq, int iUnit);
> ```
>
> - `fVol`：输出电压幅度 Vrms（0~4Vrms）
> - `freq`：频率（1Hz~200kHz）

- **1.2276 Vrms** = TP3057 的 0 dBm0 参考电平
- **1020 Hz** = ITU-T G.712 标准测试频率

### dBm0 电平参考表

|    dBm0    | 信号大小（相对 0dBm0） | Vrms              | Vpeak            | 含义               |
| :---------: | :--------------------: | ----------------- | ---------------- | ------------------ |
|     +3     |          2 倍          | ~2.45V            | ~3.47V           | 接近满量程（clip） |
| **0** |     **基准**     | **1.2276V** | **1.736V** | 标称满度输入       |
|     -10     |         1/√10         | ~0.388V           | ~0.549V          | 小信号             |
|     -40     |         1/100         | ~0.0123V          | ~0.0174V         | 极弱信号           |
|     -∞     |           0           | 0V                | 0V               | 无信号             |

> **满量程**：VPEAK_FS = 2.492V → Vrms = 1.762V → +3.14 dBm0。0dBm0 之上留有约 3dB 余量（headroom），防止瞬时峰值削波。
>
> **注意**：dBm0 描述整个信号的 RMS 功率，不描述单个采样点的瞬时电压。零码（0xD5，另有 0x55，均解码为 0V）代表物理 0V，但"0V"作为信号整体电平是 -∞ dBm0。

```c
Delay(20);   // 等待 AS 输出稳定
```

---

## 六、运行 PCM 捕获图形（第 103 行）

```c
RUN_PATTERN(PCM_TEST_PATTERN, 0, 0, 0);  // 即 RUN_PATTERN(10, 0, 0, 0)
```

> 📖 **`RUN_PATTERN` — 手册 第 45-46 页**

参数 `(10, 0, 0, 0)`：

- `start_idx=10`：图形 10 = "1020Hz 0dBm0 预存 PCM 码流"
- `get_fail=0`（GO 模式）：ATE 执行图形并在每个测试周期（488ns）记录比较结果到 **fail memory**

**图形 10 的作用**：ATE 把预存的 A-law PCM 码流通过 DR 引脚发送给 TP3057，TP3057 解码输出模拟信号；同时 ATE 在 DX 引脚比较输出 PCM。每个周期的比较结果写入 fail memory。

```c
Delay(10);
```

---

## 七、单字节 PCM 捕获（第 107 行）

```c
pcm_single = capture_one_pcm_byte(PCM_TEST_PATTERN, 46, 4);
```

**展开到** [shared.cpp:31-57]。参数含义：

- `PCM_TEST_PATTERN`（10）：图形编号
- `46`：DX 引脚号（TP3057 的 PCM 数据输出引脚）
- `4`：FSX 脉冲所在的测试周期号（数据位从周期 5 开始）

### 7.1 获取 fail memory（shared.cpp:39-41）

```c
fail_bits = NULL;
depth = 0;
fail_bits = GET_PATTERN_RESULT(start_idx, dx_channel, &depth);
```

> 📖 **`GET_PATTERN_RESULT` — 手册 第 99 页**
>
> ```c
> BYTE* GET_PATTERN_RESULT(int start_idx, int pin, int* length);
> ```
>
> - `start_idx`：图形起始索引（需与 `RUN_PATTERN` 中一致）
> - `pin`：要读取的引脚号（1~128）
> - `length`：输出参数，返回 fail memory 深度，**单位：字节**
> - **返回值**：指向 fail memory 的 `BYTE*` 指针，每个 bit 对应一个测试周期的比较结果（**0=PASS，1=FAIL**）；`NULL` 表示无记录

**为什么 `depth` 的单位是字节**：`GET_PATTERN_RESULT` 返回的 `length` 直接对应 `BYTE*` 数组的长度（字节数），这是 C 语言的常规设计——指针+字节长度，调用者用 `len` 遍历 `ret[0]` 到 `ret[len-1]`。硬件上，ST3020 每引脚配备 1M×4bit fail memory，按字节组织存储，`length` 的取值范围为 0 到约 524,287（4Mbit÷8）。

### 7.2 有效性检查（shared.cpp:43-45）

```c
if (fail_bits == NULL || depth * 8 < offset + 9) {
    return -1;
}
```

**两个条件**：

| 条件                       | 含义                                                       |
| -------------------------- | ---------------------------------------------------------- |
| `fail_bits == NULL`      | 该引脚无 fail memory 记录，API 返回空指针                  |
| `depth * 8 < offset + 9` | fail memory 深度（bit）不足以覆盖获取 PCM 字节所需的周期数 |

第二个条件详解：

- `depth` 单位是**字节**，`depth * 8` 转换为 bit
- `offset + 9` = 4 + 9 = 13，即需要覆盖周期 0~12 共 13 个周期
- 数据位位于周期 5~12，加上 FSX 脉冲（周期 4），共需要 13 bit

**返回值**：失败返回 `-1`（`int` 类型），调用处以 `if (pcm_single < 0)` 判断失败。

### 7.3 按位提取 PCM 字节（shared.cpp:47-55）★核心算法

```c
pcm_byte = 0;
for (bit = 0; bit < 8; bit++) {
    cycle    = offset + 1 + bit;        // 绝对周期号
    byte_ofs = cycle / 8;               // 在 fail_bits 数组中的字节索引
    bit_ofs  = cycle % 8;               // 在该字节内的位偏移
    level    = (fail_bits[byte_ofs] >> bit_ofs) & 1;  // 提取该 bit 的电平值
    pcm_byte |= (unsigned char)(level << (7 - bit));   // 拼入 PCM 字节的正确位置
}
```

#### 逐行解释

**`cycle = offset + 1 + bit`**：计算每个数据位对应的绝对周期号。

| `bit`（循环） | `cycle = 4+1+bit` | 含义                     |
| :-------------: | :-----------------: | ------------------------ |
|        0        |          5          | PCM bit 7（MSB，最高位） |
|        1        |          6          | PCM bit 6                |
|        2        |          7          | PCM bit 5                |
|        3        |          8          | PCM bit 4                |
|        4        |          9          | PCM bit 3                |
|        5        |         10         | PCM bit 2                |
|        6        |         11         | PCM bit 1                |
|        7        |         12         | PCM bit 0（LSB，最低位） |

`+1` 跳过了 FSX 所在的周期 4，从周期 5 开始取第一个数据位。

**`byte_ofs = cycle / 8`**：整数除法，周期号 → fail_bits 数组的字节索引。

| `cycle` | `cycle / 8` |     所在字节     |
| :-------: | :-----------: | :--------------: |
|     5     |       0       | `fail_bits[0]` |
|     6     |       0       | `fail_bits[0]` |
|     7     |       0       | `fail_bits[0]` |
|     8     |       1       | `fail_bits[1]` |
|     9     |       1       | `fail_bits[1]` |
|    10    |       1       | `fail_bits[1]` |
|    11    |       1       | `fail_bits[1]` |
|    12    |       1       | `fail_bits[1]` |

8 个数据位横跨了两个字节：3 bit 在 `fail_bits[0]` 高位，5 bit 在 `fail_bits[1]` 低位。

**`bit_ofs = cycle % 8`**：确定在该字节内部的位置。

| `cycle` | `cycle % 8` | 字节内位置 |
| :-------: | :-----------: | :--------: |
|     5     |       5       |   bit 5   |
|     6     |       6       |   bit 6   |
|     7     |       7       |   bit 7   |
|     8     |       0       |   bit 0   |
|     9     |       1       |   bit 1   |
|    10    |       2       |   bit 2   |
|    11    |       3       |   bit 3   |
|    12    |       4       |   bit 4   |

**`level = (fail_bits[byte_ofs] >> bit_ofs) & 1`**：提取单个 bit 的电平值。

以 `cycle=5` 为例：

```
假设 fail_bits[0] = 1 0 1 1 0 1 0 1  (bit7.....bit0)
                  bit7...............bit0

fail_bits[0] >> 5:  0 0 0 0 0 1 0 1  (bit5 被移到 bit0)
                  & 0 0 0 0 0 0 0 1  (= 1，按位与掩码)
                  ─────────────────
                    0 0 0 0 0 0 0 1  → level = 1
```

`>>` **不会修改** `fail_bits` 的内容——右侧操作数先被读入 CPU 寄存器形成临时副本，在寄存器中移位，原内存数据保持不变。只有 `>>=` 才会写回。

**`pcm_byte |= (unsigned char)(level << (7 - bit))`**：将 bit 放到 PCM 字节的正确位权位置。

PCM 串行数据是 **MSB-first**：第一个输出的是最高位（bit 7），最后一个是 LSB（bit 0）。`(7 - bit)` 实现时间顺序 → 位权顺序的逆映射：

| `bit`（循环） |      时间顺序      | `7 - bit`（左移位数） | PCM 中的位 |
| :-------------: | :----------------: | :---------------------: | :--------: |
|        0        | 第 1 个 bit（MSB） |            7            |   bit 7   |
|        1        |    第 2 个 bit    |            6            |   bit 6   |
|        2        |    第 3 个 bit    |            5            |   bit 5   |
|        3        |    第 4 个 bit    |            4            |   bit 4   |
|        4        |    第 5 个 bit    |            3            |   bit 3   |
|        5        |    第 6 个 bit    |            2            |   bit 2   |
|        6        |    第 7 个 bit    |            1            |   bit 1   |
|        7        | 第 8 个 bit（LSB） |            0            |   bit 0   |

#### 完整演算示例

假设 DX 输出电平序列为 `1 0 1 0 1 0 1 0`（即 `0xAA`，TP3057 正满量程码）：

```
初始: pcm_byte = 0000 0000

bit=0: cycle=5, byte_ofs=0, bit_ofs=5, level=1
       level << 7 = 1000 0000
       pcm_byte = 0000 0000 | 1000 0000 = 1000 0000

bit=1: cycle=6, byte_ofs=0, bit_ofs=6, level=0
       level << 6 = 0000 0000
       pcm_byte = 1000 0000 | 0000 0000 = 1000 0000

bit=2: cycle=7, byte_ofs=0, bit_ofs=7, level=1
       level << 5 = 0010 0000
       pcm_byte = 1000 0000 | 0010 0000 = 1010 0000

bit=3: cycle=8, byte_ofs=1, bit_ofs=0, level=0
       level << 4 = 0000 0000
       pcm_byte = 1010 0000 | 0000 0000 = 1010 0000

bit=4: cycle=9, byte_ofs=1, bit_ofs=1, level=1
       level << 3 = 0000 1000
       pcm_byte = 1010 0000 | 0000 1000 = 1010 1000

bit=5: cycle=10, byte_ofs=1, bit_ofs=2, level=0
       level << 2 = 0000 0000
       pcm_byte = 1010 1000

bit=6: cycle=11, byte_ofs=1, bit_ofs=3, level=1
       level << 1 = 0000 0010
       pcm_byte = 1010 1000 | 0000 0010 = 1010 1010

bit=7: cycle=12, byte_ofs=1, bit_ofs=4, level=0
       level << 0 = 0000 0000
       pcm_byte = 1010 1010 = 0xAA ✓
```

### 7.4 函数返回值

```c
return pcm_byte;  // shared.cpp:56，成功时返回 0~255
return -1;        // shared.cpp:44，失败时返回 -1
```

返回类型 `int`：≥0 为成功（PCM 字节值），<0 为失败。

### 7.5 调用处判断（第 112-114 行）

```c
if (pcm_single < 0) {
    SHOW_RESULT("SINGLE_CAP_FAIL", 1, "", 1, 0);
}
```

---

## 八、多字节 PCM 捕获（第 117-131 行）

```c
captured_multi_pcm = capture_multi_pcm_bytes(PCM_TEST_PATTERN, 46, 4,
                                    pcm_buf, CAPTURE_COUNT);
```

**展开到** [shared.cpp:60-88]。参数：

- `PCM_TEST_PATTERN`（10）：图形编号
- `46`：DX 引脚
- `4`：FSX 周期偏移
- `pcm_buf`：输出缓冲区
- `CAPTURE_COUNT`（32）：要捕获的字节数

### 8.1 获取 fail memory（shared.cpp:69-71）

```c
fail_bits = NULL;
depth = 0;
fail_bits = GET_PATTERN_RESULT(start_idx, dx_channel, &depth);
```

> 📖 **`GET_PATTERN_RESULT` — 手册 第 99 页**

### 8.2 深度检查（shared.cpp:73）

```c
if (fail_bits == NULL || depth * 8 < offset + (num_bytes - 1) * 256) {
    return 0;
}
```

检查 fail memory 深度是否足以覆盖全部 `num_bytes` 个 PCM 字节。`depth * 8` 将字节深度转为 bit。失败返回 `0`（表示捕获了 0 个字节）。

**所需深度的计算**：最后一个数据位所在的周期号 = `offset + 8 + 256×(num_bytes - 1)`。要覆盖到此周期，至少需要 `⌈(offset + 9 + 256×(num_bytes - 1)) / 8⌉` 字节。

| `num_bytes` | 最大周期号 | 所需 fail memory 深度 |
| :-----------: | :--------: | :-------------------: |
|      32      |    7948    |      ≥994 字节      |
|      512      |   130828   |     ≥16,354 字节     |

ST3020 每引脚 fail memory 为 1M×4bit（约 524KB），通常远超需求。

### 8.3 逐行详解（shared.cpp:76-87）★

```c
76    for (byte_idx = 0; byte_idx < num_bytes; byte_idx++) {
77        pcm_byte = 0;
78        for (bit = 0; bit < 8; bit++) {
79            cycle    = offset + 1 + byte_idx * 256 + bit;  /* 每字节 256 周期 */
80            byte_ofs = cycle / 8;
81            bit_ofs  = cycle % 8;
82            level    = (fail_bits[byte_ofs] >> bit_ofs) & 1;
83            pcm_byte |= (unsigned char)(level << (7 - bit));
84        }
85        pcm_bytes[byte_idx] = pcm_byte;
86    }
87    return num_bytes;
```

#### 第 76 行 — 外层循环：遍历每个 PCM 字节

```c
for (byte_idx = 0; byte_idx < num_bytes; byte_idx++) {
```

`byte_idx` 从 0 到 `num_bytes - 1`，每次循环提取一个完整的 8-bit PCM 字节。`byte_idx` 本质是**帧序号**——每帧（256 个 BCLK 周期）传输一个 PCM 字节。

#### 第 77 行 — 清零累加器

```c
pcm_byte = 0;
```

每个新字节从 `0000 0000`（二进制）开始，内层循环逐 bit 填入。

#### 第 78 行 — 内层循环：提取 8 个 bit

```c
for (bit = 0; bit < 8; bit++) {
```

与单字节捕获的内层相同，`bit=0` 对应 MSB，`bit=7` 对应 LSB。

#### 第 79 行 — 计算绝对周期号 ★ 与单字节的唯一区别

```c
cycle = offset + 1 + byte_idx * 256 + bit;
```

**`byte_idx * 256`** 是多字节捕获的核心。以 `num_bytes=32, offset=4` 为例：

| `byte_idx` | `cycle` 范围 |          fail_bits 中的实际位置          |
| :----------: | :------------: | :---------------------------------------: |
|      0      |     5 ~ 12     |   fail_bits[0].bit5 ~ fail_bits[1].bit4   |
|      1      |   261 ~ 268   |  fail_bits[32].bit5 ~ fail_bits[33].bit4  |
|      2      |   517 ~ 524   |  fail_bits[64].bit5 ~ fail_bits[65].bit4  |
|     ...     |      ...      |                    ...                    |
|      31      |  7941 ~ 7948  | fail_bits[992].bit5 ~ fail_bits[993].bit4 |

**为什么是 `* 256`**：

PCM 总线协议：帧同步频率 8kHz × 每帧 256 bit = 2.048MHz BCLK。FSX 每 256 个 BCLK 周期产生一个脉冲，每次脉冲后紧跟 8 个 PCM 数据位，其余 247 个周期为空闲（或下一帧 FSX）：

```
 FSX脉冲              8个数据位           空闲/下一帧FSX
   │               ├─────────┤
   ▼               ▼         ▼
[offset] [offset+1 .. offset+8] [offset+9 .. offset+255]
                           ▲                    ▲
                      byte_idx=0           byte_idx=1
                      的数据位            从offset+1+256开始
```

#### 第 80-83 行 — 位提取逻辑

```c
byte_ofs = cycle / 8;                                // 第几字节
bit_ofs  = cycle % 8;                                // 字节内第几位
level    = (fail_bits[byte_ofs] >> bit_ofs) & 1;      // 提取该 bit
pcm_byte |= (unsigned char)(level << (7 - bit));      // 拼入 PCM 字节
```

这四行与 `capture_one_pcm_byte` 完全相同。以 `byte_idx=31` 的最后一个 bit（`bit=7`）为例：

```
byte_idx=31, bit=7
cycle    = 4 + 1 + 31×256 + 7 = 12 + 7936 = 7948
byte_ofs = 7948 / 8 = 993
bit_ofs  = 7948 % 8 = 4
level    = (fail_bits[993] >> 4) & 1  → 提取 fail_bits[993] 的 bit 4
pcm_byte |= (level << (7-7)) = (level << 0) → 放到 bit 0 (LSB)
```

fail_bits 的存储结构（1D 字节数组 → 逻辑 2D 行列矩阵）：

```
fail_bits[0]:  bit7 bit6 bit5 bit4 bit3 bit2 bit1 bit0  ← 周期 0~7
fail_bits[1]:  bit7 bit6 bit5 bit4 bit3 bit2 bit1 bit0  ← 周期 8~15
fail_bits[2]:  bit7 bit6 bit5 bit4 bit3 bit2 bit1 bit0  ← 周期 16~23
...
```

`cycle / 8` 和 `cycle % 8` 将线性的周期号映射到这个结构中。每个 PCM 字节的 8 个数据位恰好横跨 fail_bits 两个字节的边界：

```
字节 0 (byte_idx=0):
  cycle 5→bit5, cycle 6→bit6, cycle 7→bit7  ← fail_bits[0] 高 3 位
  cycle 8→bit0, cycle 9→bit1, ..., cycle 12→bit4  ← fail_bits[1] 低 5 位

字节 1 (byte_idx=1):
  cycle 261→bit5, cycle 262→bit6, cycle 263→bit7  ← fail_bits[32] 高 3 位
  cycle 264→bit0, cycle 265→bit1, ..., cycle 268→bit4  ← fail_bits[33] 低 5 位
```

#### 第 85 行 — 存入输出缓冲区

```c
pcm_bytes[byte_idx] = pcm_byte;
```

内层循环 8 次结束后，`pcm_byte` 拼接完成，写入调用者的 `pcm_buf[byte_idx]`。

#### 第 87 行 — 返回

```c
return num_bytes;
```

所有字节提取完毕，返回字节数。调用者通过返回值判断：

|  返回值  | 含义                                     |
| :------: | ---------------------------------------- |
| `> 0` | 成功，值为实际捕获字节数                 |
| `== 0` | 失败（`fail_bits == NULL` 或深度不足） |

#### 完整执行轨迹（`num_bytes=32`）

```
capture_multi_pcm_bytes(10, 46, 4, pcm_buf, 32)
│
├─ fail_bits = GET_PATTERN_RESULT(10, 46, &depth)
│    → 获取图形10、引脚46的fail memory
│
├─ 检查: fail_bits==NULL? depth*8 < 4+31*256?
│    → 通过
│
├─ byte_idx=0:
│   ├─ bit 0..7: cycle=5~12, 提取 fail_bits[0].bit5 ~ fail_bits[1].bit4
│   └─ pcm_buf[0] = 第1个PCM字节
│
├─ byte_idx=1:
│   ├─ bit 0..7: cycle=261~268, 提取 fail_bits[32].bit5 ~ fail_bits[33].bit4
│   └─ pcm_buf[1] = 第2个PCM字节
│
├─ ... (共32次)
│
├─ byte_idx=31:
│   ├─ bit 0..7: cycle=7941~7948, 提取 fail_bits[992].bit5 ~ fail_bits[993].bit4
│   └─ pcm_buf[31] = 第32个PCM字节
│
└─ return 32
```

### 8.4 诊断模式（第 121-131 行）

当 `captured_multi_pcm == 0` 时进入诊断：

```c
fb = NULL;
dp = 0;
fb = GET_PATTERN_RESULT(PCM_TEST_PATTERN, 46, &dp);
SHOW_RESULT("DIAG_depth",    (double)dp,               "bytes", 0, 0);
SHOW_RESULT("DIAG_failbits", (fb == NULL) ? 0.0 : 1.0, "ptr",  0, 0);
SHOW_RESULT("MULTI_CAP_FAIL", 1, "", 1, 0);
goto END_PCM_TEST;
```

> 📖 **`GET_PATTERN_RESULT` — 手册 第 99 页**，**`SHOW_RESULT` — 手册 第 49 页**

三个诊断输出：

| 输出                         | 含义                    | 排查方向                          |
| ---------------------------- | ----------------------- | --------------------------------- |
| `DIAG_depth = 0`           | fail memory 完全为空    | 图形未正确运行，或 `get_fail=0` |
| `DIAG_depth > 0`（但不够） | 深度不足以覆盖 PCM 范围 | 图形循环次数不足                  |
| `DIAG_failbits = 0.0`      | `fb == NULL`，无指针  | 引脚未定义 fail 记录              |
| `DIAG_failbits = 1.0`      | 指针有效                | 问题在深度层面                    |

---

## 九、逐字节打印（第 143-151 行）

多字节捕获成功后（`captured_multi_pcm > 0`），进入逐字节打印。

### 9.1 限制显示数量（第 144 行）

```c
show_count = (captured_multi_pcm < 16) ? captured_multi_pcm : 16;
```

三元运算符：`captured_multi_pcm` 正常为 32。`32 < 16` 为 FALSE → `show_count = 16`，只打印前 16 个。若捕获数不足 16（异常），则有多少打多少，避免越界访问 `pcm_buf[]`。

### 9.2 逐字节循环（第 145-151 行）

```c
for (i = 0; i < show_count; i++) {
    v_by = pcm_to_voltage(pcm_buf[i]);
    sprintf(name_hex, "PCM[%02d]_hex", i);
    sprintf(name_v,   "PCM[%02d]_V",   i);
    SHOW_RESULT(name_hex, (double)(pcm_buf[i]), "hex", 0, 0);
    SHOW_RESULT(name_v,   v_by,                 "V",   0, 0);
}
```

每字节产生两行输出：原始 hex 值 + 解码后电压值。

#### `sprintf` 标签生成

`sprintf`（C 标准库 `<stdio.h>`）将格式化字符串写入字符数组：

```
i = 0  → name_hex = "PCM[00]_hex", name_v = "PCM[00]_V"
i = 5  → name_hex = "PCM[05]_hex", name_v = "PCM[05]_V"
i = 15 → name_hex = "PCM[15]_hex", name_v = "PCM[15]_V"
```

`%02d` 拆解：`%`=格式开头，`0`=不足宽度时补零，`2`=最小宽度 2 位，`d`=十进制整数。标签必须动态生成——否则 32 字节 × 2 行 = 64 个 `SHOW_RESULT` 会互相覆盖。

> 📖 **`SHOW_RESULT` — 手册 第 49 页**。上下限为 0 表示不判定仅显示。

---

## 十、pcm_to_voltage 函数详解（shared.cpp:91-118）

```c
double pcm_to_voltage(unsigned char tp3057_byte)
```

将 TP3057 的 A-law PCM 码字转换为 **瞬时电压值**（该采样点在波形上的实际电压，单位 V）。分三步：

> **函数名变更**：原名为 `pcm_to_voltage_rms`，于 2026-06-24 修正为 `pcm_to_voltage`。修正原因见下方 [§10.4 纠错说明](#104-纠错说明)。

### 10.1 偶数位反转恢复（第 99 行）

```c
alaw_std = tp3057_byte ^ 0x55;
```

TP3057 在标准 G.711 A-law 基础上做了偶数位反转（bit 0/2/4/6 取反）。异或 `0x55`（`01010101`）恢复为标准 A-law。

| TP3057 码 | XOR 0x55        | 结果 (标准 A-law) | 含义                       |
| --------- | --------------- | ----------------- | -------------------------- |
| `0xAA`  | `0xAA ^ 0x55` | `0xFF`          | **正满量程** +2.492V |
| `0xD5`  | `0xD5 ^ 0x55` | `0x80`          | **零码** 0V          |
| `0x55`  | `0x55 ^ 0x55` | `0x00`          | **零码** 0V（负零）  |
| `0x2A`  | `0x2A ^ 0x55` | `0x7F`          | **负满量程** -2.492V |

> 与 datasheet 对照一致：`0xAA` = 正满量程输入 `10101010`。

### 10.2 G.711 A-law 解码为线性值（第 102-113 行）

标准 A-law 码字结构（8 bit）：

| Bit  | 7      | 6 5 4              | 3 2 1 0              |
| ---- | ------ | ------------------ | -------------------- |
| 含义 | 符号 S | 段码 segment (0~7) | 段内步阶 step (0~15) |

```c
sign      = alaw_std & 0x80;          // bit7，0=正 0x80=负
magnitude = alaw_std & 0x7F;          // bit6~0
segment   = (magnitude >> 4) & 0x07;  // bit6~4，段编号 0~7
step      = magnitude & 0x0F;         // bit3~0，段内步阶 0~15
```

A-law 是非均匀量化——共 8 段，每段 16 步阶，段号越大步长越大（对数压缩）：

```c
if (segment == 0) {
    linear = (short)((step << 4) + 8);              // step×16 + 8
} else {
    linear = (short)(((0x10 | step) << (segment + 2)) + 8);
}
```

`0x10 | step` 拼成 5 bit（bit4=1），左移 `segment+2` 位——段号每 +1，步长翻倍。

```c
if (sign) linear = (short)(-linear);    // 符号位为 1 → 取反
```

### 10.3 线性值 → 瞬时电压（第 116-117 行）

```c
vpeak = (double)linear * (VPEAK_FS / LINEAR_MAX);
return vpeak;
```

`VPEAK_FS = 2.492`（满量程峰值），`LINEAR_MAX = 8031`（13 位线性最大值）。按比例映射到物理电压。

**返回值含义**：该 PCM 字节在采样瞬时的模拟电压值（V），不是 RMS 也不是峰值。例如：

| PCM 码 | 采样时刻       | 返回值    |
|--------|:----------:|:------:|
| 0xD5   | 过零点     | ~0 V   |
| 0xAA   | 正峰值     | +2.492 V |
| 任意   | 波形中间   | -1.736 ~ +1.736 V |

> **正半周返回正值，负半周返回负值**。统计分析中 `v_max`（正）和 `v_min`（负）一正一负正是因为瞬时电压保留了符号信息。

### 10.4 纠错说明

**旧版代码**（2026-06-24 之前）：

```c
return vpeak / sqrt(2.0);   // ← 错误！
```

旧版在 A-law 解码后又除以 √2，隐含假设"输入的 PCM 字节恰好是正弦波峰值"。但实际上：

- 一个 PCM 字节只是波形上某个采样点的**瞬时值**，不是峰值
- AS 音源与 FSX 触发之间没有锁相，捕获的字节可能在波形的任意位置
- 对过零点附近的采样值除以 √2，得到的是无意义的数值

**修正**：去掉 `/ sqrt(2.0)`，让函数回归本意——**A-law 解码 → 瞬时电压**。真 RMS 的计算交给 `compute_vrms` 函数（见下一节）。

> **影响范围**：所有调用 `pcm_to_voltage` 的代码均需注意返回值语义变化。旧代码中 `voltage = pcm_to_voltage(x) * sqrt(2.0)` 的可去掉 `* sqrt(2.0)`（因为旧版额外除了 √2，旧代码用乘 √2 来还原，现已冗余）。

---

## 十一、compute_vrms 函数详解（shared.cpp:121-132）

```c
double compute_vrms(unsigned char* pcm_bytes, int count)
```

将 **PCM 字节数组** 转换为 **真 RMS 电压值**（单位 Vrms）。GXA 增益测试、统计分析等场景均调用此函数。

### 11.1 为什么需要这个函数

`pcm_to_voltage` 只能解码单个 PCM 字节，得到的是波形上一个采样点的**瞬时电压**。但瞬时电压 ≠ 信号的有效值：

| 采样位置 | 瞬时电压 | 如果误当 RMS 用 |
|:---:|:---:|------|
| 过零点 | ~0 V | 增益 = −∞ dB |
| 波形中点 | ~1.2 V | 增益偏低 |
| 恰好峰值 | ~1.736 V | 碰巧正确 |

**正确的做法**：取足够多的采样点（覆盖多个完整周期），用均方根公式计算真 RMS。

### 11.2 核心公式

$$V_{rms} = \sqrt{\frac{1}{N}\sum_{i=1}^{N} v_i^{2}}$$

其中 $v_i$ 为第 $i$ 个 PCM 字节的瞬时电压（由 `pcm_to_voltage` 解码），$N$ 为字节总数。

**原理**：正弦波瞬时值 $v(t) = V_p \sin(\omega t)$，其有效值定义为：

$$V_{rms} = \sqrt{\frac{1}{T}\int_0^T v^2(t)\,dt}$$

对 $N$ 个离散采样点做数值近似，即为 $\sqrt{\frac{1}{N}\sum v_i^{2}}$。当采样点覆盖整数个周期时，离散 RMS 精确等于连续 RMS。

### 11.3 代码逐行解析

```c
double compute_vrms(unsigned char* pcm_bytes, int count)
{
    int i;
    double sum_sq, v_inst;

    sum_sq = 0.0;                              // (1) 平方和累加器清零
    for (i = 0; i < count; i++) {
        v_inst = pcm_to_voltage(pcm_bytes[i]); // (2) PCM 码 → 瞬时电压
        sum_sq += v_inst * v_inst;             // (3) 累加 v²
    }
    return sqrt(sum_sq / count);               // (4) 均方根 = √(Σv²/N)
}
```

| 步骤 | 操作 | 说明 |
|:---:|------|------|
| (1) | `sum_sq = 0.0` | 累积器，求和所有 v² |
| (2) | `pcm_to_voltage()` | 单字节 A-law 解码 → 瞬时电压 V |
| (3) | `sum_sq += v²` | 平方后累加，负电压平方后为正，自动处理 |
| (4) | `sqrt(sum_sq / N)` | 除以总数再开方 → 真 RMS |

### 11.4 数值验证

以 32 字节捕获 0dBm0 / 1020Hz 正弦波为例：

| 步骤 | 计算 | 值 |
|------|------|:---:|
| 瞬时电压范围 | $v_i \in [-1.736, +1.736]$ | — |
| 32 个 v² 累加 | $\sum v_i^2$ | ≈ 48.2 |
| 均方根 | $\sqrt{48.2 / 32}$ | **1.2276 Vrms** |
| 增益 | $20 \cdot \log_{10}(1.2276 / 1.2276)$ | **0 dB** |

> 32 个采样点 ≈ 4 个完整周期（8000Hz / 1020Hz ≈ 7.84 点/周期），RMS 结果稳定可靠。

### 11.5 典型调用方式

```c
unsigned char pcm_buf[32];
int captured = capture_multi_pcm_bytes(7, 46, 4, pcm_buf, 32);
if (captured == 0) { /* 捕获失败处理 */ }

double vrms = compute_vrms(pcm_buf, captured);         // 得到真 RMS
double gain = 20.0 * log10(vrms / 1.2276);             // 计算增益
```

---

## 十二、统计分析（第 153-176 行）

### 12.1 初始化统计变量（第 154-158 行）

```c
v_min = 100.0;       // 初始化为极大值（任何真实电压都小于它）
v_max = -100.0;      // 初始化为极小值（任何真实电压都大于它）
v_rms_sum = 0.0;     // Vrms² 累加器
zero_or_ff = 0;      // 0x00 或 0xFF 异常字节计数
d5_count = 0;        // 0xD5 零码计数
```

### 12.2 遍历全部捕获字节（第 160-167 行）

```c
for (i = 0; i < captured_multi_pcm; i++) {
    v_by = pcm_to_voltage(pcm_buf[i]);
    if (v_by < v_min) v_min = v_by;           // 更新最小电压
    if (v_by > v_max) v_max = v_by;           // 更新最大电压
    v_rms_sum += v_by * v_by;                 // 累加 Vrms²
    if (pcm_buf[i] == 0x00 || pcm_buf[i] == 0xFF) zero_or_ff++;
    if (pcm_buf[i] == 0xD5) d5_count++;       // TP3057 零码
}
```

遍历全部 `captured_multi_pcm`（= 32）个字节——不仅限于打印的 16 个。

#### `0x00` / `0xFF` 异常检测

`0x00`（全 0）和 `0xFF`（全 1）的共同特征是**所有 8 个 bit 完全相同**：

| 捕获值   | raw bits     | XOR 0x55 | 标准 A-law 解码                 | 可能成因                      |
| -------- | ------------ | -------- | ------------------------------- | ----------------------------- |
| `0x00` | `00000000` | `0x55` | seg=5 step=5, +0.84V（合法值）  | DX 引脚一直输出 0（电气故障） |
| `0xFF` | `11111111` | `0xAA` | seg=2 step=10, -0.13V（合法值） | DX 引脚一直输出 1（电气故障） |

两者作为 A-law 码字都是合法的中间值。但**正常 PCM 码流是多样化的**（零码 `0xD5`、小信号、大信号穿插出现），8 个 bit 完全相同暗示 DX 引脚被钳位在固定电平，没有真正输出 PCM 数据。因此统计 `zero_or_ff` 作为"数据可能无效"的警报指标。

> **注意**：当前 32 字节测试中偶发 1~2 个 `0x00`/`0xFF` 是正常量化巧合（0dBm0 正弦波范围内），不应直接判 FAIL。这就是为什么 PASS 用 `zero_or_ff == 0` 严格把关、CHECK 用 `zero_or_ff < 16` 宽松容忍。

### 12.3 计算统计结果（第 168-169 行）

```c
v_rms_total = sqrt(v_rms_sum / captured_multi_pcm);  // 均方根 (Root Mean Square)
v_peak_max  = v_max * sqrt(2.0);                     // 从最大 RMS 估算峰值
```

| 变量            | 公式                | 含义                  | 0dBm0 预期 |
| --------------- | ------------------- | --------------------- | ---------- |
| `v_rms_total` | `√(Σvᵢ² / N)` | 整体信号的 RMS 有效值 | ≈ 1.2276V |
| `v_peak_max`  | `v_max × √2`    | 信号的估计峰值        | ≈ 1.736V  |

> **注意**：`v_by` 现在由 `pcm_to_voltage` 返回**瞬时电压**（含符号，正半周为正、负半周为负）。`v_rms_sum` 累加 `v_by²`，平方后符号消失，均方根公式 `√(Σvᵢ²/N)` 仍然正确。

### 12.4 显示统计结果（第 171-176 行）

```c
SHOW_RESULT("STAT_Vrms_min",  v_min,         "V",   0, 0);   // 最小 RMS
SHOW_RESULT("STAT_Vrms_max",  v_max,         "V",   0, 0);   // 最大 RMS
SHOW_RESULT("STAT_Vrms_rss",  v_rms_total,   "V",   0, 0);   // 总 RMS
SHOW_RESULT("STAT_Vpeak_est", v_peak_max,    "V",   0, 0);   // 估计峰值
SHOW_RESULT("STAT_0xFF_cnt",  (double)zero_or_ff, "cnt", 0, 0); // 异常字节数
SHOW_RESULT("STAT_0xD5_cnt",  (double)d5_count,   "cnt", 0, 0); // 零码个数
```

> 📖 **`SHOW_RESULT` — 手册 第 49 页**。全部上下限为 0，仅显示不判定。

---

## 十三、自动判定（第 178-192 行）

三级分级判定，而非简单的 PASS/FAIL：

### 13.1 PASS（第 180-183 行）

```c
if (v_peak_max > EXPECT_VPEAK_MIN && v_peak_max < EXPECT_VPEAK_MAX
    && zero_or_ff == 0)
{
    SHOW_RESULT("RESULT=>PASS", 1, "", 1, 1);
}
```

三个条件必须同时满足：

| 条件                  | 要求             | 含义               |
| --------------------- | ---------------- | ------------------ |
| `v_peak_max > 1.3V` | 峰值不低于下限   | 信号强度达标       |
| `v_peak_max < 2.0V` | 峰值不超过上限   | 信号未饱和/过载    |
| `zero_or_ff == 0`   | 无全 0/全 1 字节 | 无不正常的固定电平 |

验证 PCM 数据能正确解码出合理电压、信号没有被静音/饱和。

### 13.2 CHECK（第 185-187 行）

```c
else if (v_peak_max > 0.05 && zero_or_ff < captured_multi_pcm / 2)
{
    SHOW_RESULT("RESULT=>CHECK", 0.5, "", 1, 0);
}
```

| 条件                   | 要求         | 含义         |
| ---------------------- | ------------ | ------------ |
| `v_peak_max > 0.05V` | 有微弱信号   | 芯片"活着"   |
| `zero_or_ff < 16`    | 异常少于半数 | 数据部分有效 |

显示 `0.5`（介于 0 和 1 之间），需人工查看。**0.05V 门槛的用意**：

```
0.05V ÷ 1.736V ≈ 2.9%（只有预期的不到 3%）
20 × log₁₀(0.05/1.736) ≈ -30.8 dBm0
```

这个值唯一用途是判断 **"芯片有没有在发出任何声音"**。如 v_peak = 0→死寂直接 FAIL；v_peak = 0.06V→在工作但严重衰减（图形错误、电源异常、时钟偏移），值得人工排查。它不是精确幅度判定——精确判定在 PASS 的 1.3~2.0V 完成。

### 13.3 FAIL（第 189-191 行）

```c
else
{
    SHOW_RESULT("RESULT=>FAIL", 0, "", 1, 0);
}
```

PASS 和 CHECK 都不满足时触发。典型场景：

- 完全无信号（芯片从未启动）
- 信号被钳位为固定电平（全是 0x00 或 0xFF）
- 数据完全损坏（`zero_or_ff ≥ 16`）

---

## 十四、清理（第 194-197 行）

```c
END_PCM_TEST:
    DPS_OFF(DPS1);
    DPS_OFF(DPS2);
```

> 📖 **`DPS_OFF` — 手册 第 42 页**
>
> ```c
> void DPS_OFF(BYTE No);
> ```
>
> - `No`：通道号（`DPS1` / `DPS2`），关闭指定 DPS 输出

`goto END_PCM_TEST` 从诊断模式直达此处，关断两路电源。这是 C95 风格的统一资源管理。

---

## 十五、完整执行流程总图

```
程序启动
│
├─ #include "shared.h"            [编译期：常量、类型、函数声明]
│
└─ void PASCAL TP3057()
    │
    ├─ 1. 变量声明块              [C95：全部在函数体开头]
    │
    ├─ 2. SETUP() ────────────→ [shared.cpp:15-28]
    │   ├─ SET_DPS(+5V, -5V)        [📖 p.40]
    │   ├─ Delay(10)                [📖 p.51]
    │   ├─ SET_PERIOD(488)          [📖 p.47]
    │   ├─ SET_TIMING(100,350,450)  [📖 p.47]
    │   ├─ SET_INPUT_LEVEL(2.2,0.6) [📖 p.44]
    │   ├─ SET_OUTPUT_LEVEL(2.4,0.4)[📖 p.44-45]
    │   ├─ FORMAT(NRZ0, ...)        [📖 p.43]
    │   ├─ FORMAT(RZ, ...)          [📖 p.43]
    │   ├─ RUN_PATTERN(3, ...)      [📖 p.45-46]  (上电)
    │   └─ Delay(10)                [📖 p.51]
    │
    ├─ 3. Delay(20)                 [📖 p.51]
    ├─ 4. SHOW_RESULT("===PCM Cap Test===")  [📖 p.49]
    │
    ├─ 5. SET_AS(1.2276V, 1020Hz)   [📖 p.90]
    │   └─ Delay(20)                [📖 p.51]
    │
    ├─ 6. RUN_PATTERN(10, ...)      [📖 p.45-46]
    │   └─ Delay(10)                [📖 p.51]
    │
    ├─ 7. capture_one_pcm_byte() → [shared.cpp:31-57]
    │   ├─ GET_PATTERN_RESULT()     [📖 p.99]
    │   ├─ 深度检查 (depth*8 >= offset+9)
    │   ├─ for bit 0..7: 按位提取 (cycle = offset+1+bit)
    │   └─ 返回 pcm_byte (成功) 或 -1 (失败)
    │
    ├─ 8. 判断 pcm_single < 0 → 失败标记
    │
    ├─ 9. capture_multi_pcm_bytes() → [shared.cpp:60-88]
    │   ├─ GET_PATTERN_RESULT()     [📖 p.99]
    │   ├─ 深度检查 (depth*8 >= offset+(num_bytes-1)*256)
    │   ├─ for byte_idx 0..31:
    │   │     for bit 0..7: 提取 (cycle = offset+1+byte_idx*256+bit)
    │   └─ 返回 num_bytes (成功) 或 0 (失败)
    │
    ├─ 10. 若 captured_multi_pcm==0 → 诊断模式
    │    ├─ GET_PATTERN_RESULT()    [📖 p.99]
    │    ├─ DIAG_depth / DIAG_failbits
    │    └─ goto END_PCM_TEST
    │
    ├─ 11. 逐字节打印 (show_count = min(captured, 16))
    │    ├─ for i 0..show_count-1:
    │    │     ├─ v_by = pcm_to_voltage(pcm_buf[i])  → [shared.cpp:91-118]
    │    │     │   ├─ alaw_std = byte ^ 0x55 (偶数位反转恢复)
    │    │     │   ├─ segment/step 提取 → 非均匀量化解码
    │    │     │   ├─ linear * VPEAK_FS / LINEAR_MAX → 瞬时电压 V
    │    │     │   └─ 返回 瞬时电压 (不做 ÷√2 转换)
    │    │     ├─ sprintf: PCM[xx]_hex, PCM[xx]_V
    │    │     └─ SHOW_RESULT × 2       [📖 p.49]
    │    │
    │    ├─ 12. 统计分析 (遍历全部32字节)
    │    │   ├─ v_min / v_max / v_rms_sum 更新
    │    │   ├─ zero_or_ff (0x00,0xFF) / d5_count (0xD5) 计数
    │    │   ├─ v_rms_total = sqrt(v_rms_sum/N)
    │    │   ├─ v_peak_max  = v_max × √2
    │    │   └─ SHOW_RESULT × 6          [📖 p.49]
    │    │
    │    ├─ 13. 自动判定
    │    │   ├─ PASS:  1.3V<v_peak<2.0V && zero_or_ff==0
    │    │   ├─ CHECK: v_peak>0.05V && zero_or_ff < N/2
    │    │   └─ FAIL:  以上均不满足
    │    │
    │    └─ END_PCM_TEST:
    │       ├─ DPS_OFF(DPS1)             [📖 p.42]
    │       └─ DPS_OFF(DPS2)             [📖 p.42]
```

---

## 十六、更改 PCM 捕获字节数

如需将捕获字节数从 32 改为其他值（如 512），只需修改 `test_pcm_capture.cpp` 中的 **一处**，`shared.cpp` 完全不用改。

### 需要改的

|   位置   | 说明   | 改前                          | 改后                           |
| :------: | ------ | ----------------------------- | ------------------------------ |
| 第 38 行 | 宏定义 | `#define CAPTURE_COUNT  32` | `#define CAPTURE_COUNT  512` |

`pcm_buf[CAPTURE_COUNT]` 会自动跟随宏展开为正确大小，无需手动改数组。

### 不需要改的

- **`capture_multi_pcm_bytes` 函数**（shared.cpp:60-88）：所有参数都是动态的——循环次数由 `num_bytes` 控制、深度检查公式自动适应、周期计算 `byte_idx * 256` 不受影响
- **`show_count`**（test_pcm_capture.cpp:144）：`(captured_multi_pcm < 16) ? captured_multi_pcm : 16` 始终最多显示前 16 字节
- **`capture_one_pcm_byte`**：捕获单个字节，与字节数无关

### 深度要求

改为 512 字节后，图形 fail memory 必须足够深：

```
所需深度 = ⌈(offset + 9 + (num_bytes - 1) × 256) / 8⌉
        = ⌈(4 + 9 + 511 × 256) / 8⌉
        = ⌈130829 / 8⌉
        = 16,354 字节
```

如果图形 fail memory 深度不足 16,354 字节，`capture_multi_pcm_bytes` 返回 `0` 触发诊断模式。建议在图形编辑器中确认深度。

---

## 附录：系统手册页数速查

| API 函数               |     手册页数     | 首次出现                 |
| ---------------------- | :---------------: | ------------------------ |
| `SET_DPS`            |  **p.40**  | shared.cpp:17            |
| `Delay`              |  **p.51**  | shared.cpp:19            |
| `SET_PERIOD`         |  **p.47**  | shared.cpp:20            |
| `SET_TIMING`         |  **p.47**  | shared.cpp:21            |
| `SET_INPUT_LEVEL`    |  **p.44**  | shared.cpp:22            |
| `SET_OUTPUT_LEVEL`   | **p.44-45** | shared.cpp:23            |
| `FORMAT`             |  **p.43**  | shared.cpp:24            |
| `RUN_PATTERN`        | **p.45-46** | shared.cpp:26            |
| `GET_PATTERN_RESULT` |  **p.99**  | shared.cpp:41            |
| `SHOW_RESULT`        |  **p.49**  | test_pcm_capture.cpp:73  |
| `DPS_OFF`            |  **p.42**  | test_pcm_capture.cpp:185 |
| `SET_AS`             |  **p.90**  | test_pcm_capture.cpp:95  |
