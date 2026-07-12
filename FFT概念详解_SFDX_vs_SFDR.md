# SFDX / SFDR FFT 概念详解

## 一、核心公式

```
Δf = Fs / N
      ↑    ↑
      │    └── FFT 点数
      └─────── 采样率
```

**三个量绑定在一起，改任意一个，另外两个必受影响。**

---

## 二、SFDX 侧（发送失真）

### 信号链路

```
Audio Source ──→ TP3057(TX) ──→ DX 引脚 ──→ fail memory 捕获 PCM 字节
  1020Hz                              ↑
  0dBm0                         每字节 = 一个采样点
```

### 各个量的物理意义和变量对应

| 概念 | 变量/宏 | SFDX 的值 | 物理含义 |
|------|------|:---:|------|
| **采样率 Fs** | `SFDX_SAMPLE_RATE` | **8000 Hz** | **PCM 硬件时钟固定**，每秒出 8000 个 PCM 字节，改不了 |
| **FFT 点数 N** | `SFDX_FFT_POINTS` | **512** | 从 fail memory 里抓 512 个 PCM 字节，就是 512 个采样点 |
| **频率分辨率 Δf** | `freq_res` | **15.625 Hz** | `8000 / 512`，每个 bin 代表 15.625 Hz 宽 |

```
pcm_data[0]   ← 第 0 个采样点 (t = 0)
pcm_data[1]   ← 第 1 个采样点 (t = 1/8000 = 125μs)
pcm_data[2]   ← 第 2 个采样点 (t = 2/8000 = 250μs)
  ...
pcm_data[511] ← 第 511 个采样点 (t = 511/8000 ≈ 63.9ms)
                                        └── 总共 64ms 的波形
```

解码成电压后存到 `voltage[0..511]`。

---

## 三、SFDR 侧（接收失真）

### 信号链路

```
图形10 (1020Hz PCM) ──→ TP3057(RX) ──→ VFR+ 引脚 ──→ DVM 逐点采样
                                                 ↑
                                          MAT_DVM_MEASURE
```

### 各个量的物理意义

| 概念 | 变量/宏 | SFDR 的值 | 物理含义 |
|------|------|:---:|------|
| **DVM 分频** | `SFDR_FREQ_DIV` | **383** | DVM 时钟 = 50MHz / 383 ≈ 130548 Hz |
| **采样率 Fs** | `actual_sr` | **≈130548 Hz** | 每秒采 130548 个电压点 |
| **FFT 点数 N** | `SFDR_FFT_POINTS` | **512** | 每次采 512 个点 |
| **频率分辨率 Δf** | `freq_res` | **≈254.98 Hz** | `130548 / 512` |

```c
actual_sr = 50e6 / SFDR_FREQ_DIV;          // 130548 Hz
freq_res  = actual_sr / SFDR_FFT_POINTS;   // ≈255 Hz
```

```
voltage[0]   ← t = 0
voltage[1]   ← t = 1/130548 ≈ 7.66μs
  ...
voltage[511] ← t = 511/130548 ≈ 3.92ms
                              └── 只采了 3.92ms！因为 Fs 高了很多
```

---

## 四、DFT 之后：bin 是什么

### `fft_mag[i]` — 把时间域变成频率域

```
DFT 之前:                                DFT 之后:
voltage[0..511]  ——→  fft_mag[0..255]
时间序列                 频率幅度谱
(512 个实数)             (256 个实数)
```

`fft_mag[i]` 的第 `i` 个元素 = **bin i** = 频率为 `i × Δf` 的分量的**幅度**。

> 为什么是 256 个而不是 512 个？因为实数 FFT 有对称性，后半段 (bin 256~511) 是前半段的镜像，FFT 函数只返回前 N/2 个有效 bin。

### bin → 频率 换算

```
频率 = bin号 × Δf
```

| bin 号 | SFDX 对应频率 (Δf=15.625Hz) | SFDR 对应频率 (Δf≈255Hz) |
|:------:|:---:|:---:|
| 0 | 0 Hz (DC) | 0 Hz (DC) |
| 1 | 15.6 Hz | 255 Hz |
| 4 | 62.5 Hz | **1020 Hz** (基波 ✅) |
| 8 | 125 Hz | **2040 Hz** (2次谐波 ✅) |
| 12 | 187.5 Hz | **3060 Hz** (3次谐波 ✅) |
| 65 | **1015.6 Hz** (基波 ⚠️) | — |
| 130 | **2031.3 Hz** (2次谐波) | — |
| 195 | **3046.9 Hz** (3次谐波) | — |
| 255 | 3984.4 Hz | ≈65025 Hz |
| 256 | Nyquist = Fs/2 = 4000 Hz | Nyquist ≈ 65274 Hz |

---

## 五、代码中的 bin 查找流程

```c
// 1. 算频率分辨率
freq_res = Fs / N;                     // SFDX: 15.625  SFDR: ~255

// 2. 最大 bin 号（N/2 个有效 bin）
max_bin = N / 2;                       // 256

// 3. 通过 freq_to_bin() 把目标频率换算成 bin 号
bin_fund = freq_to_bin(1020.0,   freq_res, max_bin);  // SFDX:65  SFDR:4
bin_2nd  = freq_to_bin(2 * 1020, freq_res, max_bin);  // SFDX:130 SFDR:8
bin_3rd  = freq_to_bin(3 * 1020, freq_res, max_bin);  // SFDX:195 SFDR:12
```

`freq_to_bin` 内部就是 `freq / freq_res + 0.5`（四舍五入取最近的 bin）。

---

## 六、失真计算

```c
SFDX_db = calc_distortion_db(fft_mag[bin_fund],   // 基波幅度 (1020Hz)
                              fft_mag[bin_2nd],    // 2次谐波幅度 (2040Hz)
                              fft_mag[bin_3rd]);   // 3次谐波幅度 (3060Hz)

// 内部: 20 × log10( max(H2, H3) / H1 )
```

**失真 = 谐波 ÷ 基波，取 dB**。越负越好，≤-46dB 合格。

---

## 七、SFDX vs SFDR 一张图对比

| | SFDX | SFDR |
|------|------|------|
| 信号方向 | TP3057 编码输出 | TP3057 解码输出 |
| 数据来源 | fail memory | DVM (`MAT_DVM_MEASURE`) |
| 原始数据 | `pcm_data[512]` (字节) | `voltage[512]` (伏) |
| 采样率 Fs | 8000 Hz (硬件固定) | 130548 Hz (可调 `FREQ_DIV`) |
| FFT 点数 N | 512 | 512 |
| Δf | 15.625 Hz | 255 Hz |
| 基波 bin | 65 (≈1015.6Hz) | 4 (=1020Hz) |
| 对齐精度 | 偏差 ~4.4Hz ⚠️ | 偏差 <0.1Hz ✅ |
| 测量次数 | 1 次 | 4 次平均 |
| 失真结果变量 | `SFDX_db` | `SFDR_db` |

---

## 八、为什么 SFDR 对齐好但 SFDX 对不齐

```
SFDX: Fs = 8000 (固定) → Δf = 8000/N
      要找 N 使得 1020/Δf = 1020×N/8000 = 整数
      → N 必须是 400 的倍数
      → 但 N 必须是 2 的幂 (DFT 要求)
      → 不存在既是 400 的倍数又是 2 的幂的 N ❌

SFDR: Fs = 50e6/FREQ_DIV → 可以调 FREQ_DIV
      → 选 FREQ_DIV = 383, 使 Fs ≈ 130548, 1020/Δf = 4.0 ✅
```

---

## 九、速查表

| 变量/宏 | 含义 | 三者关系 |
|------|------|------|
| `SFDX_FFT_POINTS` / `SFDR_FFT_POINTS` | **N** — FFT 点数 | |
| `SFDX_SAMPLE_RATE` / `actual_sr` | **Fs** — 采样率 | |
| `freq_res` | **Δf** — 频率分辨率 = 每个 bin 宽度 | **Δf = Fs / N** |
| `max_bin` | **N/2** — 有效 bin 数 (0 ~ N/2-1) | |
| `bin_fund` | 基波 1020Hz 对应的 bin 号 | **bin = f / Δf** |
| `bin_2nd` | 2次谐波 2040Hz 对应的 bin 号 | |
| `bin_3rd` | 3次谐波 3060Hz 对应的 bin 号 | |
| `pcm_data[]` | SFDX 原始 PCM 字节 (时域) | |
| `voltage[]` | 解码后的电压 / DVM 采的电压 (时域) | |
| `fft_mag[]` | DFT 输出的幅度谱 (频域) | |
| `fft_mag_sum[]` | SFDR 多次测量累加幅度谱 | |

```
voltage[] ──[DFT]──→ fft_mag[]
时域                    频域
N 个点                  N/2 个 bin
每个点间隔 1/Fs 秒      每个 bin 间隔 Δf Hz
```
