# test_all.cpp 完整参考手册

> TP3057 全参数集成测试程序 — 变量/函数/宏 按测试项分类详解

---

## 目录

1. [编译控制宏](#1-编译控制宏)
2. [物理常量](#2-物理常量)
3. [测试参数宏](#3-测试参数宏)
4. [全局变量](#4-全局变量)
5. [公共函数](#5-公共函数)
6. [Test 01: CON-TEST 连接性测试](#6-test-01-con-test)
7. [Test 02: FUNC-TEST 功能测试](#7-test-02-func-test)
8. [Test 03~05: DC 漏电流测试 (IIL / IIH / IOZ)](#8-test-0305-dc-漏电流测试)
9. [Test 06~07: 电源电流测试 (ICC0IBB0 / ICC1IBB1)](#9-test-0607-电源电流测试)
10. [Test 08: GXA + GXR 发送增益测试](#10-test-08-gxa--gxr)
11. [Test 09: GRA + GRR + GRRL 接收增益测试](#11-test-09-gra--grr--grrl)
12. [Test 10: SFDX 发送失真测试](#12-test-10-sfdx)
13. [Test 11: SFDR 接收失真测试](#13-test-11-sfdr)
14. [Test 12: IMD 互调失真测试](#14-test-12-imd)
15. [BIN 编号对照表](#15-bin-编号对照表)
16. [数据流总图](#16-数据流总图)

---

## 1. 编译控制宏

> 位置：第 394~416 行 | 改为 `0` 跳过对应测试，改为 `1` 启用

### 1.1 测试项开关

| 宏 | 默认值 | 控制范围 |
|------|:---:|------|
| `TEST_CON_ENABLE` | 1 | Test 01 连接性测试 |
| `TEST_FUNC_ENABLE` | 1 | Test 02 功能测试 |
| `TEST_IIL_ENABLE` | 1 | Test 03 IIL 输入低漏电 |
| `TEST_IIH_ENABLE` | 1 | Test 04 IIH 输入高漏电 |
| `TEST_IOZ_ENABLE` | 1 | Test 05 IOZ 三态漏电 |
| `TEST_ICC0IBB0_ENABLE` | 1 | Test 06 掉电电流 |
| `TEST_ICC1IBB1_ENABLE` | 1 | Test 07 工作电流 |
| `TEST_GXA_ENABLE` | 1 | Test 08 GXA 发送绝对增益 |
| `TEST_GXR_ENABLE` | 1 | Test 08 GXR 发送增益-频率 |
| `TEST_GRA_ENABLE` | 1 | Test 09 GRA 接收绝对增益 |
| `TEST_GRR_ENABLE` | 1 | Test 09 GRR 接收增益-频率 |
| `TEST_GRRL_ENABLE` | 1 | Test 09 GRRL 接收增益-电平 |
| `TEST_SFDX_ENABLE` | 1 | Test 10 SFDX 发送失真 |
| `TEST_SFDR_ENABLE` | 1 | Test 11 SFDR 接收失真 |
| `TEST_IMD_ENABLE` | 1 | Test 12 IMD 互调失真 |

### 1.2 分箱开关

| 宏 | 默认值 | 含义 |
|------|:---:|------|
| `BIN_ENABLE` | **0** | 调试时为 `0`（不分箱），生产时为 `1`（正常分箱） |

所有 `DO_BIN(n)` 调用在 `BIN_ENABLE=0` 时展开为空操作 `((void)0)`，编译后不执行。

---

## 2. 物理常量

> 位置：第 29~32 行

| 宏 | 值 | 含义 |
|------|:---:|------|
| `VPEAK_FS` | 2.492 | TP3057 A-law 满量程峰值电压 (V) |
| `LINEAR_MAX` | 4032 | G.711 A-law 13-bit 线性编码最大值 |
| `M_PI` | 3.14159... | 圆周率 |

---

## 3. 测试参数宏

### 3.1 SFDX 参数（第 34~36 行）

| 宏 | 值 | 含义 |
|------|:---:|------|
| `SFDX_FFT_POINTS` | 512 | FFT 点数 = 捕获的 PCM 字节数 |
| `SFDX_SAMPLE_RATE` | 8000.0 | PCM 采样率 (Hz)，硬件固定 |

### 3.2 SFDR 参数（第 38~44 行）

| 宏 | 值 | 含义 |
|------|:---:|------|
| `SFDR_FFT_POINTS` | 512 | FFT 点数（2的幂） |
| `SFDR_FREQ_DIV` | 383 | DVM 时钟分频，实际采样率 = 50MHz/383 ≈ 130.5kHz |
| `SFDR_NAVG` | 4 | FFT 平均次数 |

> ~~`SFDR_DVM_SR`~~ 和 ~~`SFDR_FREQ_RES`~~ 已注释，实际采样率在代码中通过 `actual_sr = 50e6 / SFDR_FREQ_DIV` 计算。

### 3.3 IMD 参数（第 46~63 行）

| 宏 | 值 | 含义 |
|------|:---:|------|
| `IMD_F1` | 300.0 | 测试频率 f1 (Hz) |
| `IMD_F2` | 3400.0 | 测试频率 f2 (Hz) |
| `IMD_TEST_LEVEL_DBM` | -10.0 | 每音电平 (dBm0) |
| `IMD_V_TOTAL_RMS` | — | 双音总 RMS 电压，由公式 `1.2276×10^(-10/20)` 计算 |
| `IMD_V_SINGLE_PEAK` | =IMD_V_TOTAL_RMS | 每音峰值电压 |
| `IMD_AS_POINTS` | 1024 | AS 波形点数 |
| `IMD_AS_SAMPLE_RATE` | 100000.0 | AS 波形采样率 (Hz) |
| `IMD_AS_FREQ_DIV` | 1000 | AS 播放分频，100MHz/1000=100kHz |
| `IMD_FFT_POINTS` | 2048 | DVM 采样点数 = FFT 点数 |
| `IMD_FREQ_DIV` | 488 | DVM 时钟分频，50MHz/488≈102.5kHz |
| `IMD_NAVG` | 4 | FFT 平均次数 |

---

## 4. 全局变量

> 位置：第 65~68 行 | 定义在 head0710.cpp 中，`extern` 声明跨测试共享

| 变量 | 类型 | 写入者 | 读取者 | 含义 |
|------|------|:---:|:---:|------|
| `gxa1020` | `double` | GXA | GXR | 1020Hz 0dBm0 发送增益 (dB)，用作 GXR 参考 |
| `gra1020` | `double` | GRA | GRR, GRRL | 1020Hz 0dBm0 接收增益 (dB)，用作 GRR/GRRL 参考 |
| `vra_ref` | `double` | GRA | GRR, GRRL | GRA 测得的 VFRO 参考电压 (V)，备用于 GRRL 算法 |

### 数据依赖关系

```
GXA ──→ gxa1020 ──→ GXR
GRA ──→ gra1020 ──→ GRR
         gra1020 ──→ GRRL
         vra_ref ──→ GRR, GRRL (可选算法)
```

---

## 5. 公共函数

> 声明位置：第 70~124 行 | 实现在 head0710.cpp 中

### 5.1 `void SETUP(void)` — 系统初始化

上电 + 配置时序 + 运行图形3（取消 DX 高阻态）。**每个测试项开始前需调用**。

### 5.2 `int capture_one_pcm_byte(start_idx, dx_channel, offset)` — 单字节 PCM 捕获

从 fail memory 提取**1 个** A-law 字节。现在基本不用，被 FSX 版替代。

### 5.3 `int capture_multi_pcm_bytes_fsx(...)` — 多字节 PCM 捕获（FSX 动态检测）

| 参数 | 类型 | SFDX 调用值 | 含义 |
|------|------|:---:|------|
| `start_idx` | `int` | 7 | 图形索引 |
| `dx_channel` | `int` | 46 | DX 引脚通道号 |
| `fsx_channel` | `int` | 43 | FSX 监测通道号 |
| `offset` | `int` | 0 | 搜索起始周期 |
| `pcm_bytes` | `unsigned char*` | 输出缓冲区 | 存放捕获的 PCM 字节 |
| `num_bytes` | `int` | 512 | 期望捕获的字节数 |
| **返回** | `int` | — | 实际捕获的字节数 |

### 5.4 `double pcm_to_voltage(tp3057_byte)` — A-law 解码 → 瞬时电压

```c
输入: A-law 码字 (带偶数位反转 XOR 0x55)
步骤: ① 偶数位反转恢复标准 A-law
      ② G.711 A-law 解码 → 13-bit 线性值
      ③ 线性值 × (VPEAK_FS / LINEAR_MAX) → 瞬时电压
返回: 瞬时峰值电压 (V)
```

### 5.5 `double compute_vrms(pcm_bytes, count)` — PCM 数组 → RMS 电压

对 `count` 个 PCM 字节逐个解码取瞬时电压，求均方根：`Vrms = sqrt(avg(v²))`

### 5.6 `void apply_hanning(data, n)` — 汉宁窗

```c
w[i] = 0.5 × (1 - cos(2π×i/(n-1)))
data[i] = data[i] × w[i]
```

### 5.7 `int freq_to_bin(freq, freq_res, max_bin)` — 频率 → bin 号

```c
bin = round(freq / freq_res)   // 四舍五入
若 bin >= max_bin → 返回 -1
```

### 5.8 `double calc_distortion_db(A_fund, A_harm1, A_harm2)` — 失真计算

```c
若 A_fund ≤ 0 → 返回 1 (sentinel，表示基波丢失)
否则 → 20 × log10( max(A_harm1, A_harm2) / A_fund )
```

---

## 6. Test 01: CON-TEST

> 连接性测试 | 第 428~453 行 | 失败 → `return` 立即终止全部测试

### 测试原理

所有模拟引脚加 -0.1mA 恒流，测二极管正向压降。

### 变量

| 变量 | 类型 | 含义 |
|------|------|------|
| — | — | 无局部变量，直接调用 PMU |

### 分箱

| BIN | 条件 |
|:---:|------|
| 1 | 任何引脚二极管压降不在 -0.1V ~ -1.9V |

---

## 7. Test 02: FUNC-TEST

> 功能测试 (Loopback) | 第 460~488 行

### 测试原理

AS 输出 1020Hz → TX → DX→DR (继电器7环回) → RX → AVM 测 VFRO。

### 变量

| 变量 | 类型 | 含义 |
|------|------|------|
| `Vout` | `double` | AVM 测得的 VFRO 电压 (V) |
| `backloop_db` | `double` | `20×log10(Vout/1.2276)`，应接近 0dB |

### 分箱

无单独分箱，失败时显示 `FUNC_FAIL`。

---

## 8. Test 03~05: DC 漏电流测试

> IIL / IIH / IOZ

### 8.1 Test 03: IIL — 输入低电平漏电流（第 495~516 行）

| 变量 | 类型 | 含义 |
|------|------|------|
| `result_iil_1` | `double` | PMU 测量结果（已注释不用） |

| 分箱 | 条件 |
|:---:|------|
| 2 | IIL 超过 ±10μA |

### 8.2 Test 04: IIH — 输入高电平漏电流（第 523~544 行）

| 变量 | 类型 | 含义 |
|------|------|------|
| `result_iih_1` | `double` | PMU 测量结果（已注释不用） |

| 分箱 | 条件 |
|:---:|------|
| 3 | IIH 超过 ±10μA |

### 8.3 Test 05: IOZ — 三态漏电流（第 551~581 行）

| 变量 | 类型 | 含义 |
|------|------|------|
| `result_iozl_1` | `double` | IOZL 测量结果（已注释不用） |
| `result_iozh_1` | `double` | IOZH 测量结果（已注释不用） |

| 分箱 | 条件 |
|:---:|------|
| 4 | IOZL 超过 ±10μA |
| 5 | IOZH 超过 ±10μA |

---

## 9. Test 06~07: 电源电流测试

> ICC0/IBB0 / ICC1/IBB1

### 9.1 Test 06: ICC0IBB0 — 掉电电流（第 588~615 行）

| 变量 | 类型 | 含义 |
|------|------|------|
| `fail` | `int` | 累加失败计数，≠0 表示有项目失败 |

| 分箱 | 条件 |
|:---:|------|
| 6 | ICC0>1.5mA 或 IBB0>0.3mA |

### 9.2 Test 07: ICC1IBB1 — 工作电流（第 622~665 行）

| 变量 | 类型 | 含义 |
|------|------|------|
| `fail` | `int` | 总失败计数 |
| `icc1_fail` | `int` | ICC1 是否失败标志 |
| `ibb1_fail` | `int` | IBB1 是否失败标志 |

| 分箱 | 条件 |
|:---:|------|
| 7 | ICC1 > 9mA |
| 8 | IBB1 > 9mA |

---

## 10. Test 08: GXA + GXR

> 发送增益测试 | 第 674~764 行

### 测试原理

```
GXA: AS 1020Hz → TX → DX → 捕获 PCM → decode → Vrms → 20log(Vrms/1.2276)
GXR: 同流程遍历 300/2000/3000Hz，计算 Gxf - gxa1020
```

### 10.1 GXA 变量（第 677~702 行）

| 变量 | 类型 | 含义 |
|------|------|------|
| `vout_rms_gxa` | `double` | PCM 解码后的 RMS 电压 (V) |
| `gain_db` | `double` | `20×log10(vout_rms/1.2276)`，即 GXA 值 (dB) |
| `pcm_buf_gxa[512]` | `unsigned char[]` | PCM 字节缓冲区（512 字节） |
| `num_byte` | `int` | 实际捕获的字节数 |

| 分箱 | 条件 |
|:---:|------|
| 27 | 捕获失败 (num_byte==0) |
| 28 | GXA 超出 ±0.15dB |

### 10.2 GXR 变量（第 706~758 行）

| 变量 | 类型 | 含义 |
|------|------|------|
| `pcm_buf_gxr[512]` | `unsigned char[]` | PCM 字节缓冲区 |
| `num_byte_gxr` | `int` | 实际捕获的字节数 |
| `freq_list_gxr[3]` | `double[]` | 频率列表 = {300, 2000, 3000} Hz |
| `num_freq_gxr` | `int` | 频率个数 = 3 |
| `fail_count` | `int` | 失败计数 |
| `i` | `int` | 循环变量 |
| `f` | `double` | 当前频率 (Hz) |
| `vout_rms_gxr` | `double` | 当前频率的 PCM 解码 RMS 电压 (V) |
| `Gxf` | `double` | 当前频率的发送增益 `20×log10(vout/1.2276)` |
| `GXRf` | `double` | **GXR 值** = Gxf - gxa1020（相对增益偏差） |
| `name[32]` | `char[]` | 结果显示标签缓冲区 |

| 分箱 | 条件 |
|:---:|------|
| 29 | 某频率捕获失败 |
| 30 | 任一频率 GXRf 超出 ±0.15dB |

### 公式

```
GXA:  gain_db = 20 × log10(vout_rms / 1.2276)        → 存为 gxa1020
GXR:  GXRf    = 20 × log10(vout_rms / 1.2276) - gxa1020
     = Gxf - GXA 参考
```

---

## 11. Test 09: GRA + GRR + GRRL

> 接收增益测试 | 第 773~911 行

### 测试原理

```
GRA:  图形10 (1020Hz PCM) → RX → AVM 测 VFRO → 20log(Vout/1.2276)
GRR:  图形8/12 (300/3000Hz PCM) → AVM → Grf - gra1020
GRRL: 图形9/11 (-40/+3dBm0 PCM) → AVM → Gabs - L - gra1020
```

### 11.1 共享变量（第 775~777 行）

| 变量 | 类型 | 含义 |
|------|------|------|
| `gra1020_local` | `double` | 局部副本 = gra1020，供同一作用域内 GRR/GRRL 使用 |

### 11.2 GRA 变量（第 781~797 行）

| 变量 | 类型 | 含义 |
|------|------|------|
| `vout_rms_gra` | `double` | AVM 测得的 VFRO RMS 电压 (V) |

| 分箱 | 条件 |
|:---:|------|
| 11 | GRA 超出 ±0.15dB |

### 11.3 GRR 变量（第 803~839 行）

| 变量 | 类型 | 含义 |
|------|------|------|
| `start_idx_list[2]` | `int[]` | 图形索引 = {8, 12} (300Hz, 3000Hz) |
| `freq_list_grr[2]` | `double[]` | 频率列表 = {300, 3000} Hz |
| `num_freq_grr` | `int` | 频率个数 = 2 |
| `fail` | `int` | 失败计数 |
| `i` | `int` | 循环变量 |
| `vout_rms_grr` | `double` | 当前频率 AVM 测得的 RMS 电压 (V) |
| `Grf` | `double` | 当前频率的接收增益 `20×log10(Vout/1.2276)` |
| `GRR_val` | `double` | **GRR 值** = Grf - gra1020_local |
| `name[32]` | `char[]` | 结果显示标签 |

| 分箱 | 条件 |
|:---:|------|
| 12 | 任一频率 GRR_val 超出 ±0.15dB |

### 11.4 GRRL 变量（第 845~906 行）

| 变量 | 类型 | 含义 |
|------|------|------|
| `level_dbm0[2]` | `double[]` | 输入电平 = {-40, 3} dBm0 |
| `index[2]` | `int[]` | 图形索引 = {9, 11} |
| `num` | `int` | 电平个数 = 2 |
| `fail_grrl` | `int` | 失败计数 |
| `i` | `int` | 循环变量 |
| `idx` | `int` | 当前图形索引 |
| `L` | `double` | 当前输入电平 (dBm0) |
| `vout_grrl` | `double` | AVM 测得的 RMS 电压 (V)，-40dBm0 用 mV 量程后 /1000 |
| `Gabs` | `double` | 绝对增益 `20×log10(Vout/1.2276)` |
| `delta` | `double` | **GRRL 值** = Gabs - L - gra1020_local |
| `name[32]` | `char[]` | 结果显示标签 |

| 分箱 | 条件 |
|:---:|------|
| 13 | 任一电平 delta 超出 ±0.2dB |

### GRRL 三种等价算法

```
① delta = 20×log10(Vout/1.2276) - L - gra1020_local
② delta = 20×log10(Vout/vra_ref) - L
③ delta = 20×log10(Vout/(1.2276×10^(L/20))) - gra1020_local
```

---

## 12. Test 10: SFDX

> 发送单频失真测试 | 第 920~1002 行

### 信号链路

```
AS 1020Hz 0dBm0 → TX → DX → fail memory → 捕获 512 PCM 字节
→ A-law 解码 → 瞬时电压序列 → 汉宁窗 → DFT → 查找谐波 → 计算失真
```

### 变量

| 变量 | 类型 | 大小 | 含义 |
|------|------|:---:|------|
| `pcm_data[]` | `unsigned char` | N | 捕获的原始 PCM 字节（时域） |
| `captured` | `int` | — | 实际捕获的字节数，≠N 则失败 |
| `voltage[]` | `double` | N | A-law 解码后的瞬时电压序列（时域） |
| `fft_mag[]` | `double` | N/2 | DFT 输出的幅度谱（频域），每个 bin 一个幅度值 |
| `freq_res` | `double` | — | 频率分辨率 = 8000/N = 15.625 Hz |
| `max_bin` | `int` | — | 最大 bin 号 = N/2 = 256 |
| `bin_fund` | `int` | — | 1020Hz 对应 bin 65 |
| `bin_2nd` | `int` | — | 2040Hz 对应 bin 130 |
| `bin_3rd` | `int` | — | 3060Hz 对应 bin 195 |
| `SFDX_db` | `double` | — | **失真结果** = 20×log10(max(H2,H3)/H1) |
| `i` | `int` | — | 循环变量 |

### FFT 参数

```
Fs = 8000 Hz (PCM 硬件固定)
N  = 512
Δf = 15.625 Hz
1020Hz → bin 65 (偏差 ~4.4Hz，非相干采样)
```

### 分箱

| BIN | 条件 |
|:---:|------|
| 14 | PCM 捕获数量不足 |
| 15 | DFT 计算失败 |
| 16 | 2次/3次谐波 bin 超出范围 |
| 17 | 基波丢失 (SFDX_db ≥ 0.9) |
| 18 | SFDX_db > -46dB (不合格) |

### 流程图

```
pcm_data[] ──[pcm_to_voltage]──→ voltage[]
                                   │
                            [apply_hanning]
                                   │
                              [DFT]
                                   │
                                   ↓
                               fft_mag[]
                                   │
                      ┌────────────┼────────────┐
                      ↓            ↓            ↓
                  bin_fund     bin_2nd      bin_3rd
                      │            │            │
                      └────────────┼────────────┘
                                   ↓
                         calc_distortion_db
                                   ↓
                               SFDX_db
```

---

## 13. Test 11: SFDR

> 接收单频失真测试 | 第 1012~1098 行

### 信号链路

```
图形10 (1020Hz PCM) → RX → VFR+ → MAT_DVM_MEASURE → 电压序列
→ 汉宁窗 → DFT → 4次平均 → 查找谐波 → 计算失真
```

### 变量

| 变量 | 类型 | 大小 | 含义 |
|------|------|:---:|------|
| `voltage[]` | `double` | N | DVM 采样的瞬时电压序列（时域） |
| `fft_mag[]` | `double` | N/2 | **单次** DFT 幅度谱（每轮被覆盖） |
| `fft_mag_sum[]` | `double` | N/2 | **累加** 幅度谱（4 轮求和），初始全零 |
| `actual_sr` | `double` | — | 实际采样率 = 50×10⁶ / 383 ≈ 130548 Hz |
| `freq_res` | `double` | — | 频率分辨率 = actual_sr/N ≈ 255 Hz |
| `max_bin` | `int` | — | 最大 bin 号 = N/2 = 256 |
| `bin_fund` | `int` | — | 1020Hz 对应 bin 4（完美对齐） |
| `bin_2nd` | `int` | — | 2040Hz 对应 bin 8 |
| `bin_3rd` | `int` | — | 3060Hz 对应 bin 12 |
| `SFDR_db` | `double` | — | **失真结果** = 20×log10(max(H2,H3)/H1) |
| `run` | `int` | — | 测量轮次 (0~3) |
| `i` | `int` | — | 循环变量 |

### FFT 参数

```
FREQ_DIV = 383 → Fs = 50e6/383 ≈ 130548 Hz
N  = 512
Δf = Fs/N ≈ 255 Hz
1020Hz → bin 4 (偏差 <0.1Hz，完美相干采样)
```

### 数据流

```
第1轮: DVM采样 → voltage[] → 加窗 → DFT → fft_mag[] ─┐
第2轮: DVM采样 → voltage[] → 加窗 → DFT → fft_mag[] ─┤
第3轮: DVM采样 → voltage[] → 加窗 → DFT → fft_mag[] ─┼→ fft_mag_sum[] → ÷4 → fft_mag[] → 查bin → SFDR_db
第4轮: DVM采样 → voltage[] → 加窗 → DFT → fft_mag[] ─┘
```

### 分箱

| BIN | 条件 |
|:---:|------|
| 31 | RUN_PATTERN(10) 失败（原 BIN 18，与 SFDX 冲突） |
| 19 | DFT 计算失败 |
| 20 | 2次/3次谐波 bin 超出范围 |
| 21 | 基波丢失 (SFDR_db ≥ 0.9) |
| 22 | SFDR_db > -46dB (不合格) |

---

## 14. Test 12: IMD

> 互调失真测试 | 第 1108~1225 行

### 信号链路

```
AS 播放 300+3400Hz 双音 → TX → relay7 环回 → RX → VFR+
→ DVM 2048 点 × 4 次 → DFT 平均 → 查找 4 分量 → IMD
```

### 变量

| 变量 | 类型 | 大小 | 含义 |
|------|------|:---:|------|
| `as_waveform[]` | `WORD` | 1024 | AS 自定义波形数据 |
| `voltage[]` | `double` | 2048 | DVM 采样电压序列 |
| `fft_mag[]` | `double` | 1024 | 单次/平均后 DFT 幅度谱 |
| `fft_mag_sum[]` | `double` | 1024 | 累加幅度谱（4 轮求和） |
| `actual_sr` | `double` | — | 实际采样率 = 50e6/488 ≈ 102.5kHz |
| `freq_res` | `double` | — | 频率分辨率 ≈ 50 Hz |
| `max_bin` | `int` | — | 最大 bin 号 = 1024 |
| `bin_f1` | `int` | — | 300Hz bin |
| `bin_f2` | `int` | — | 3400Hz bin |
| `bin_sum` | `int` | — | f1+f2 = 3700Hz bin |
| `bin_diff` | `int` | — | f2-f1 = 3100Hz bin |
| `A_f1` | `double` | — | f1 分量幅度 |
| `A_f2` | `double` | — | f2 分量幅度 |
| `A_sum` | `double` | — | 和频分量幅度 |
| `A_diff` | `double` | — | 差频分量幅度 |
| `A_imd` | `double` | — | = max(A_sum, A_diff) |
| `A_fund` | `double` | — | = max(A_f1, A_f2) |
| `IMD_db` | `double` | — | **IMD 结果** = 20×log10(A_imd/A_fund) |
| `run` | `int` | — | 测量轮次 |
| `i` | `int` | — | 循环变量 |
| `t` | `double` | — | 波形生成时间戳 |
| `v` | `double` | — | 波形瞬时电压 |

### FFT 参数

```
FREQ_DIV = 488 → Fs = 50e6/488 ≈ 102459 Hz
N  = 2048
Δf ≈ 50 Hz
```

### 分箱

| BIN | 条件 |
|:---:|------|
| 23 | DFT 计算失败 |
| 24 | 四分量任一 bin 超出范围 |
| 25 | 基波丢失 (A_fund ≤ 0) |
| 26 | IMD_db > -41dB (不合格) |

---

## 15. BIN 编号对照表

| BIN | 测试项 | 失败原因 |
|:---:|------|------|
| 1 | CON-TEST | 二极管压降异常 |
| 2 | IIL | 输入低漏电超限 |
| 3 | IIH | 输入高漏电超限 |
| 4 | IOZ | IOZL 超限 |
| 5 | IOZ | IOZH 超限 |
| 6 | ICC0IBB0 | 掉电电流超限 |
| 7 | ICC1IBB1 | ICC1 超限 |
| 8 | ICC1IBB1 | IBB1 超限 |
| 11 | GRA | 接收绝对增益超限 |
| 12 | GRR | 接收频响超限 |
| 13 | GRRL | 接收电平响应超限 |
| 14 | SFDX | PCM 捕获不足 |
| 15 | SFDX | DFT 失败 |
| 16 | SFDX | 谐波 bin 越界 |
| 17 | SFDX | 基波丢失 |
| 18 | SFDX | 失真超限 |
| 19 | SFDR | DFT 失败 |
| 20 | SFDR | 谐波 bin 越界 |
| 21 | SFDR | 基波丢失 |
| 22 | SFDR | 失真超限 |
| 23 | IMD | DFT 失败 |
| 24 | IMD | 分量 bin 越界 |
| 25 | IMD | 基波丢失 |
| 26 | IMD | IMD 超限 |
| 27 | GXA | PCM 捕获失败 |
| 28 | GXA | 发送增益超限 |
| 29 | GXR | PCM 捕获失败 |
| 30 | GXR | 发送频响超限 |
| 31 | SFDR | RUN_PATTERN 失败 |

> BIN 7/8 重复问题已在整合时解决，GXA 用 27/28，SFDR 用 31。

---

## 16. 数据流总图

```
                ┌──────────────────────────────────────┐
                │           TP3057 测试程序              │
                └──────────────────────────────────────┘

┌──── DC 测试 ────────────────────────────────────────────────┐
│                                                              │
│  CON-TEST ── PMU FIMV -0.1mA → 测 V → 二极管压降            │
│  IIL/IIH  ── PMU FVMI 加电压 → 测 I → 漏电流                │
│  IOZ      ── PMU FVMI 加电压 → 测 I → 三态漏电              │
│  ICC/IBB  ── DPS_MEASURE → 电源电流                          │
│                                                              │
│  数据类型: 单个 double 值                                     │
└──────────────────────────────────────────────────────────────┘

┌──── 增益测试 ───────────────────────────────────────────────┐
│                                                              │
│  GXA/GXR ── AS → TX → PCM捕获 → compute_vrms → 20log       │
│  GRA/GRR/GRRL ── PCM输入 → RX → AVM_MEASURE → 20log       │
│                                                              │
│  PCM数据流:                                                  │
│  fail memory → capture_* → pcm_buf[] → pcm_to_voltage       │
│                                      → compute_vrms → gain  │
│                                                              │
│  数据类型: 字节数组 → 电压数组 → 单个 RMS → 单个 dB          │
└──────────────────────────────────────────────────────────────┘

┌──── 失真测试 ───────────────────────────────────────────────┐
│                                                              │
│  SFDX ── PCM捕获 → 解码 → 汉宁窗 → DFT → 谐波分析            │
│  SFDR ── DVM采样 → 汉宁窗 → DFT ×4 → 累加平均 → 谐波分析     │
│  IMD  ── AS双音 → DVM采样 → DFT ×4 → 累加平均 → 4分量分析    │
│                                                              │
│  时域 → 频域转换:                                            │
│  voltage[N] ──[DFT]──→ fft_mag[N/2]                         │
│  采样点                    频域bin                            │
│  间隔 = 1/Fs 秒            间隔 = Fs/N Hz                    │
│                                                              │
│  失真公式: 20×log10( max(谐波) / 基波 )                      │
└──────────────────────────────────────────────────────────────┘
```
