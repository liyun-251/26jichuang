# IMD 互调失真测试详解

> 对应代码：[test_all.cpp](test_all.cpp) 第 46~82 行（宏）、第 1121~1255 行（测试逻辑）
> 平台：ST3020 ATE | 芯片：TP3057 A-law PCM CODEC | 语言标准：C95

---

## 一、测试概述

### 1.1 什么是 IMD

互调失真（Intermodulation Distortion）：当两个不同频率的信号 $f_1$、$f_2$ 同时通过非线性系统时，输出端除了原始频率外，还会产生**和频** $f_1+f_2$ 与**差频** $|f_2-f_1|$ 的互调产物。IMD 衡量这些产物的相对大小：

$$\text{IMD} = 20\cdot\log_{10}\frac{\max(A_{sum},\;A_{diff})}{\max(A_{f_1},\;A_{f_2})}\quad\text{(dB)}$$

越负越好，合格标准 **≤ -41 dB**（datasheet）。

### 1.2 信号链路（Loop Around Measurement）

```
┌──────────────────┐     ┌──────────────────────────────────┐     ┌──────────┐
│  Audio Source    │     │          TP3057 芯片              │     │  DVM     │
│  (DAC 播放波形)   │────→│ VFXI+ (pin 12)                   │     │          │
│                  │     │   ↓ TX 带通滤波 (300-3400Hz)      │     │          │
│  双音 1024 点    │     │   ↓ A-law 编码                    │     │          │
│  Fs=125kHz       │     │   ↓ DX 引脚 (pin 16)              │     │          │
│                  │     │   ↓ relay7 环回 ──────────┐       │     │          │
│                  │     │   ↓ DR 引脚 (pin 10)       │       │     │          │
│                  │     │   ↓ A-law 解码              │       │     │          │
│                  │     │   ↓ RX 低通滤波 + sinx/x    │       │     │          │
│                  │     │ VFRO (pin 2) ───────────────┼───────→│ 采样 2048 点│
│                  │     │                             │       │ Fs=62.5kHz  │
└──────────────────┘     └─────────────────────────────┼───────└──────────────┘
                                                          relay7
```

### 1.3 继电器配置

| 继电器 | 功能 | 说明 |
|:------:|------|------|
| relay7 | DX ↔ DR 环回 | TP3057 编码输出直连解码输入 |
| relay1 | VFRO → DVM1 | 模拟输出电压接入 DVM 测量总线 |

### 1.4 数据手册依据

| 项目 | 手册值 | 代码实现 |
|------|------|------|
| 测试方法 | Loop Around Measurement | ✓ relay7 环回 |
| 频率范围 | 两音均在 300-3400 Hz | f1=366.2, f2=3295.9 ✓ |
| 输入电平 | -4 ~ -21 dBm0 | 每音 -10 dBm0，总约 -7 dBm0 ✓ |
| 限值 | ≤ -41 dB | `if (IMD_db > -41.0) BIN(26)` ✓ |

---

## 二、测试方案

### 2.1 双对齐原则

为获得干净的 FFT 频谱（无泄漏），频率选择必须同时满足两项约束：

| 约束 | 条件 | 目的 |
|------|------|------|
| **AS 循环对齐** | $f / (F_{s\_AS} / 1024) = \text{整数}$ | 1024 点波形首尾相位连续，循环播放无跳变 |
| **FFT bin 对齐** | $f / (F_{s\_DVM} / 2048) = \text{整数}$ | 频率落在 bin 中心，加汉宁窗后无泄漏 |

所有符合约束的频率都是 $F_{s\_AS} / 1024$ 的整数倍。通过遍历 $F_{s\_AS}$ 和 $F_{s\_DVM}$ 参数组合得到三个可行方案。

### 2.2 方案 B（当前激活）

```c
#define IMD_F1                366.211       /* f1 = Fs_AS/1024 × 3  */
#define IMD_F2                3295.898      /* f2 = Fs_AS/1024 × 27 */
#define IMD_AS_SAMPLE_RATE    125000.0      /* AS 波形采样率 (Hz) */
#define IMD_FREQ_DIV          800           /* DVM 时钟分频, Fs=50MHz/800=62.5kHz */
```

### 2.3 完整参数及作用

#### 测试条件宏

| 宏 | 值 | 作用 |
|------|:---:|------|
| `IMD_F1` | 366.211 | 测试音 f1 频率 (Hz)，300-3400Hz 内 |
| `IMD_F2` | 3295.898 | 测试音 f2 频率 (Hz)，300-3400Hz 内 |
| `IMD_TEST_LEVEL_DBM` | -10.0 | 每音相对 0dBm0 的电平 (dB) |

#### 电压宏

| 宏 | 值 | 公式 | 作用 |
|------|:---:|------|------|
| `IMD_V_SINGLE_RMS` | 0.388 | $1.2276 \times 10^{-10/20}$ | 单音 RMS 电压 (V) |
| `IMD_V_SINGLE_PEAK` | 0.549 | `IMD_V_SINGLE_RMS × √2` | 单音峰值电压 (V)，波形生成用 |
| `IMD_AS_FULL_SCALE` | 2.0 | — | AS DAC 满量程 (V)，同时控制波形归一化和 `RUN_AS_PATTERN` 的 fVol |

> 双音最坏情况峰值 = 2 × 0.549 = 1.098 V，远小于 2.0 V 满量程，不会触发限幅。

#### 时序宏

| 宏 | 值 | 公式 | 作用 |
|------|:---:|------|------|
| `IMD_AS_POINTS` | 1024 | — | AS 自定义波形点数，须为 2 的幂 |
| `IMD_AS_SAMPLE_RATE` | 125000.0 | — | AS DAC 播放采样率 (Hz) |
| `IMD_AS_FREQ_DIV` | 800 | `100e6 / 125000` | AS 播放分频比，`RUN_AS_PATTERN` 第3参数 |
| `IMD_FFT_POINTS` | 2048 | — | DVM 采样点数 = FFT 点数，须为 2 的幂 |
| `IMD_FREQ_DIV` | 800 | — | DVM 时钟分频，`MAT_DVM_MEASURE` 第6参数 |
| `IMD_NAVG` | 1 | — | 测量平均次数（幅度谱累加求平均） |

#### FFT 参数（运行时计算）

| 变量 | 公式 | 值 | 含义 |
|------|------|:---:|------|
| `actual_sr` | $50\times10^6 / 800$ | 62500 Hz | DVM 实际采样率 |
| `freq_res` | $62500 / 2048$ | ≈30.5176 Hz | 频率分辨率，每个 bin 的宽度 |
| `max_bin` | $2048 / 2$ | 1024 | 有效 bin 数（实 FFT 对称性） |

### 2.4 Bin 对齐验证

| 频率分量 | 频率 (Hz) | 周期数 (2048/Fs) | bin 号 | 实际频率 (Hz) | 偏差 |
|------|------:|:---:|:---:|------|:---:|
| $f_1$ | 366.211 | 12 | 12 | 366.211 | 0 ✓ |
| $f_2$ | 3295.898 | 108 | 108 | 3295.898 | 0 ✓ |
| $f_1+f_2$ | 3662.109 | 120 | 120 | 3662.109 | 0 ✓ |
| $f_2-f_1$ | 2929.687 | 96 | 96 | 2929.687 | 0 ✓ |

AS 波形端：1024 点 @ 125kHz → 周期 8.192 ms → $f_1$ 完成 **3** 周期、$f_2$ 完成 **27** 周期 → 无相位跳变。

### 2.5 三方案对比

| | 方案 A | **方案 B (当前)** | 方案 C |
|------|:---:|:---:|:---:|
| $f_1$ (Hz) | 390.625 | **366.211** | 305.176 |
| $f_2$ (Hz) | 3320.313 | **3295.898** | 3356.934 |
| $F_{s\_AS}$ (kHz) | 100 | **125** | 156.25 |
| $F_{s\_DVM}$ (kHz) | 100 | **62.5** | 78.125 |
| FREQ_DIV | 500 | **800** | 640 |
| $\Delta f$ (Hz) | ≈48.83 | **≈30.52** | ≈38.15 |
| AS 周期 $f_1$ / $f_2$ | 4 / 34 | **3 / 27** | 2 / 22 |
| FFT 周期 $f_1$ / $f_2$ | 8 / 68 | **12 / 108** | 8 / 88 |
| $f_1+f_2$ (Hz) | 3710.938 | **3662.109** | 3662.109 |
| $f_2-f_1$ (Hz) | 2929.688 | **2929.687** | 3051.758 |
| **特点** | 改代码最少 | **bin 最密，分辨最好** | f1/f2 最贴近 300/3400 |
| `IMD_AS_FREQ_DIV` | 1000 | **800** | 640 |

> 切换方案：修改 `IMD_F1`、`IMD_F2`、`IMD_AS_SAMPLE_RATE`、`IMD_FREQ_DIV` 四个宏（`IMD_AS_FREQ_DIV` 由公式自动计算）。

---

## 三、测试流程分步详解

### 步骤 1：初始化

```c
SETUP();                     // DPS 上电 ±5V、配置时序格式、运行图形3(上电解高阻)
CLOSE_RELAY("1,7");          // 闭合环回继电器
Delay(10);
```

调用 `SETUP()` 建立芯片工作条件（VCC=+5V, VBB=-5V, 2.048MHz 时序），闭合 relay1 (VFRO→DVM1) 和 relay7 (DX↔DR)。

### 步骤 2：生成双音 AS 波形 (1024 点)

```c
for (i = 0; i < IMD_AS_POINTS; i++) {
    t = (double)i / IMD_AS_SAMPLE_RATE;
    v = IMD_V_SINGLE_PEAK * (sin(2π·f₁·t) + sin(2π·f₂·t));
    // 限幅保护
    if (v >  IMD_AS_FULL_SCALE) v =  IMD_AS_FULL_SCALE;
    if (v < -IMD_AS_FULL_SCALE) v = -IMD_AS_FULL_SCALE;
    // 归一化映射到 DAC 码 (0x0000=+满度, 0xFFFF=-满度)
    as_waveform[i] = (WORD)(((v/IMD_AS_FULL_SCALE + 1.0)/2.0) * 65535.0 + 0.5);
}
```

| 参数 | 值 | 说明 |
|------|:---:|------|
| 波形点数 | 1024 | LOAD_AS_PATTERN 片段大小 |
| 采样间隔 | 8 μs | 1/125000 |
| 总时长 | 8.192 ms | f₁ 3 周期、f₂ 27 周期 |
| 双音峰值 | ≤1.098 V | 2×IMD_V_SINGLE_PEAK |

#### DAC 码映射

```
瞬时电压 v      归一化           DAC 码          AS 输出
─────────────────────────────────────────────────────────
+IMD_AS_FULL_SCALE  →  +1.0  →  65535  →  -fVol (反相)
       0            →   0    →  32768  →   0V
-IMD_AS_FULL_SCALE  →  -1.0  →      0  →  +fVol (反相)
```

> 注：DAC 输出极性是反相的（0x0000 = 正满度），但对 IMD 测试无影响——两个频率同时反相，相对相位关系不变。

### 步骤 3：加载波形

```c
LOAD_AS_PATTERN(7, 1, as_waveform);
```

将 1024 个 WORD 加载到音频源 Segment 7、Bank 1，后续 `RUN_AS_PATTERN` 引用同一位置。

### 步骤 4：清零累加数组

```c
for (i = 0; i < IMD_FFT_POINTS / 2; i++)
    fft_mag_sum[i] = 0.0;
```

### 步骤 5：测量循环（`IMD_NAVG` 轮，当前 1 轮）

每轮包含四个子步骤：

#### 5a. 播放波形 + 运行数字图形

```c
RUN_AS_PATTERN(7, 1, IMD_AS_FREQ_DIV, IMD_AS_FULL_SCALE);
// freq = 100MHz / 800 = 125kHz
// fVol = 2.0 (峰值电压)
if (!RUN_PATTERN(13, 0, 0, 0)) {
    DO_BIN(32);     // 图形13运行失败
    goto END_IMD;
}
```

- `RUN_AS_PATTERN(seg=7, bank=1, freq_div=800, fVol=2.0)` — AS DAC 以 125kHz 循环播放波形，输出峰值 ±2.0V
- `RUN_PATTERN(13, ...)` — 数字图形 13 提供 BCLK/FSX 时序，使 TP3057 持续编码→解码

#### 5b. DVM 采样

```c
MAT_DVM_MEASURE(1, 2.0, V, 10, IMD_FFT_POINTS, IMD_FREQ_DIV, voltage);
```

| 参数 | 值 | 含义 |
|------|:---:|------|
| 通道 | 1 | DVM1 |
| 量程 | 2.0 V | > 双音峰值 1.098V |
| 延迟 | 10 ms | 等待信号稳定 |
| 点数 | 2048 | IMD_FFT_POINTS |
| 分频 | 800 | Fs = 50MHz/800 = 62.5 kHz |
| 采样时长 | 32.768 ms | f₁ 12 周期、f₂ 108 周期 |

#### 5c. 加汉宁窗

```c
apply_hanning(voltage, IMD_FFT_POINTS);
// w[i] = 0.5 × (1 - cos(2π·i/(N-1)))
```

抑制频谱泄漏。所有频率已对齐 bin，加窗后主瓣宽度 ≈ 1.5 bin ≈ 45.8 Hz，相邻分量最近间距 84 bin（f₁→f_diff），远超主瓣范围，无串扰。

#### 5d. DFT + 累加

```c
if (!DFT(voltage, IMD_FFT_POINTS, fft_mag))
    { DO_BIN(23); goto END_IMD; }   // DFT 计算失败

for (i = 0; i < IMD_FFT_POINTS / 2; i++)
    fft_mag_sum[i] += fft_mag[i];    // 幅度谱累加
```

`DFT()` 是 ST3020 内置 FFT 函数。`fft_mag[0..1023]` 是实数幅度谱（DC ~ Nyquist=31.25kHz）。

### 步骤 6：求平均

```c
for (i = 0; i < IMD_FFT_POINTS / 2; i++)
    fft_mag[i] = fft_mag_sum[i] / (double)IMD_NAVG;
```

### 步骤 7：查找四个频率分量

```c
actual_sr = 50e6 / IMD_FREQ_DIV;     // = 62500
freq_res  = actual_sr / IMD_FFT_POINTS; // ≈ 30.5176
max_bin   = IMD_FFT_POINTS / 2;      // = 1024

bin_f1   = freq_to_bin(IMD_F1,          freq_res, max_bin);  // → 12
bin_f2   = freq_to_bin(IMD_F2,          freq_res, max_bin);  // → 108
bin_sum  = freq_to_bin(IMD_F1+IMD_F2,   freq_res, max_bin);  // → 120
bin_diff = freq_to_bin(IMD_F2-IMD_F1,   freq_res, max_bin);  // → 96
```

`freq_to_bin` 内部：`bin = round(freq / freq_res)`，超出 `max_bin` 返回 -1。

### 步骤 8：计算 IMD

```c
A_f1   = fft_mag[bin_f1];    // f1 基波幅度
A_f2   = fft_mag[bin_f2];    // f2 基波幅度
A_sum  = fft_mag[bin_sum];   // 和频幅度
A_diff = fft_mag[bin_diff];  // 差频幅度

A_imd  = max(A_sum, A_diff);   // 取较大互调产物
A_fund = max(A_f1, A_f2);      // 取较大基波

if (A_fund <= 0.0) { DO_BIN(25); goto END_IMD; }  // 基波丢失

IMD_db = 20.0 * log10(A_imd / A_fund);

SHOW_RESULT("IMD", IMD_db, "dB", -41.0, No_LoLimit);
if (IMD_db > -41.0) BIN(26);
else SHOW_RESULT("IMD=>PASS", 1, "", 1, 1);
```

### 步骤 9：清理

```c
END_IMD:
    CLEAR_RELAY("1,7");
    DPS_OFF(DPS1);
    DPS_OFF(DPS2);
```

---

## 四、用到的 ST3020 API

| API | 调用 | 功能 |
|------|------|------|
| `LOAD_AS_PATTERN(seg, bank, WORD[])` | seg=7, bank=1 | 加载 1024 点自定义波形到音频源 |
| `RUN_AS_PATTERN(seg, bank, freq_div, fVol)` | freq_div=800, fVol=2.0 | 100MHz/800=125kHz 循环播放，峰值 ±2.0V |
| `RUN_PATTERN(idx, a, b, c)` | idx=13 | 运行数字图形 13 提供时钟和帧同步 |
| `MAT_DVM_MEASURE(ch, range, unit, delay, N, freq_div, buf[])` | ch=1, range=2.0V, N=2048, freq_div=800 | DVM 采样，Fs=50MHz/800=62.5kHz |
| `DFT(dataRe[], N, fftRe[])` | N=2048 | ST3020 内置 FFT，`fftRe[0..N/2-1]` |
| `CLOSE_RELAY("pins")` / `CLEAR_RELAY("pins")` | "1,7" | 控制继电器矩阵 |
| `SHOW_RESULT(name, val, unit, hi, lo)` | — | 显示测试结果 |
| `BIN(n)` / `DO_BIN(n)` | 23-26, 32 | 分 bin |

---

## 五、BIN 分配

| BIN | 条件 | 说明 |
|:---:|------|------|
| 23 | `DFT()` 返回 FALSE | DFT 计算失败 |
| 24 | 任一 bin 索引返回 -1 | 频率分量超出 FFT 范围 |
| 25 | `A_fund ≤ 0.0` | 基波幅度为零，信号丢失 |
| 26 | `IMD_db > -41.0` | IMD 超限，芯片不合格 |
| 32 | `RUN_PATTERN(13, ...)` 返回 0 | 图形 13 运行失败 |

---

## 六、关键技术细节

### 6.1 为什么用 DVM 而不用 AVM

AVM 只能给出一个 RMS 值，无法分离四个频率分量（f₁、f₂、f_sum、f_diff）。必须用 DVM 采样时域波形 → DFT 变换到频域，才能分别获取每个分量的幅度。

### 6.2 为什么 FFT 点数不能减少

2048 → 512 会导致 $\Delta f$ 从 30.5 Hz 增大到 122 Hz。汉宁窗主瓣宽度 ≈ 1.5×122 = 183 Hz。f₁ (366Hz) 和 f_diff (2930Hz) 间距足够，但 f₂ (3296Hz) 和 f_sum (3662Hz) 间距仅 366 Hz = 3 bin，在 512 点下仅 3×122 = 366 Hz，刚好相邻，加窗后的主瓣展宽会导致两个分量泄漏到对方 bin 中。

### 6.3 电平

- 0 dBm0 = 1.2276 Vrms（TP3057 参考电平）
- 每音 -10 dBm0 → $1.2276 \times 10^{-0.5} = 0.388$ Vrms → 0.549 Vpeak
- 双音总 RMS = $\sqrt{0.388^2 + 0.388^2} = 0.549$ Vrms → 约 -7 dBm0
- 在 datasheet 规定的 -4 ~ -21 dBm0 范围内

### 6.4 限幅

`IMD_AS_FULL_SCALE = 2.0` V，双音最坏峰值 1.098 V，远低于限幅门限。限幅代码存在但实际不触发。

### 6.5 测量时间估算

| 阶段 | 操作 | 耗时 |
|------|------|:---:|
| AS 波形生成 | 1024 次 sin 计算 | <1 ms |
| LOAD_AS_PATTERN | 上传到 AS 内存 | <1 ms |
| MAT_DVM_MEASURE | 10ms 延迟 + 2048/62500≈32.8ms | ≈43 ms |
| DFT | ST3020 硬件 FFT | <1 ms |
| **单轮总计** | | **≈45 ms** |
