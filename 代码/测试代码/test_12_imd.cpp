/* ============================================================
   test_12_imd.cpp — 互调失真测试 (IMD) — Loop Around Measurement
   编译：head0710.cpp + 本文件 复制进入工程
   语言标准：C95

   测试流程：
     1. SETUP + 闭合 relay7 (DX↔DR 环回)
     2. 生成 300Hz + 3400Hz 双音波形（总电平 -10dBm0）
     3. LOAD_AS_PATTERN + RUN_AS_PATTERN 通过音频源播放
     4. MAT_DVM_MEASURE 采样 VFR+ 模拟输出 (2048点, ~102.5kHz)
     5. 加汉宁窗 → DFT → 4次平均幅度谱
     6. 查找 f1(300), f2(3400), sum(3700), diff(3100) 四个分量
     7. IMD = 20*log10(max(A_sum,A_diff) / max(A_f1,A_f2))
   限值：<= -41dB

   信号路径（Loop Around）：
     AS → VFXI+ → [TX A-law编码] → DX → relay7 → DR → [RX A-law解码] → VFR+ → DVM

   FFT 参数：Fs=50e6/488≈102459Hz  N=2048  Δf≈50.03Hz
             300Hz→bin6(300.2Hz)    3100Hz→bin62(3101.8Hz)
             3400Hz→bin68(3402.0Hz) 3700Hz→bin74(3702.1Hz)
   ============================================================ */






#define TEST_ENABLE  1

void PASCAL TP3057()
{
#if TEST_ENABLE
    WORD as_waveform[IMD_AS_POINTS];              /* 1024 */
    double voltage[IMD_FFT_POINTS];               /* 2048 */
    double fft_mag[IMD_FFT_POINTS / 2];           /* 1024 */
    double fft_mag_sum[IMD_FFT_POINTS / 2];       /* 1024, 累加用 */
    double actual_sr, freq_res;
    int max_bin;
    int bin_f1, bin_f2, bin_sum, bin_diff;
    double A_f1, A_f2, A_sum, A_diff;
    double A_imd, A_fund, IMD_db;
    int run, i;
    double t, v;

    /* 1. 初始化 + 闭合环回继电器 */
    SETUP();
    CLOSE_RELAY("7");          /* DX ↔ DR 环回 */
    Delay(10);



    /* 2. 生成双音波形 (300Hz + 3400Hz) */
    for (i = 0; i < IMD_AS_POINTS; i++) {
        t = (double)i / IMD_AS_SAMPLE_RATE;
        v = IMD_V_SINGLE_PEAK * (sin(2.0 * M_PI * IMD_F1 * t)
                               + sin(2.0 * M_PI * IMD_F2 * t));
        if (v > 10.0)  v = 10.0;     /* 限幅保护，实际峰值~0.78V 不触发 */
        if (v < -10.0) v = -10.0;
        as_waveform[i] = (WORD)(((v / 10.0) + 1.0) / 2.0 * 65535.0 + 0.5);
    }

    /* 3. 加载波形到音频源（只加载一次） */
    LOAD_AS_PATTERN(7, 1, as_waveform);

    /* 4. 清零累加数组 */
    for (i = 0; i < IMD_FFT_POINTS / 2; i++) {
        fft_mag_sum[i] = 0.0;
    }

    /* 5. 多次测量，累加幅度谱 */
    for (run = 0; run < IMD_NAVG; run++) {
        /* 5a. 播放双音波形 */
        RUN_AS_PATTERN(7, 1, IMD_AS_FREQ_DIV, 10);
        RUN_PATTERN(13, 0, 0, 0);//运行图形13，给时钟和脉冲，持续4096*256个周期
        Delay(5);

        /* 5b. DVM 采样 VFR+ 模拟输出 */
        MAT_DVM_MEASURE(1, 2.0, V, 10, IMD_FFT_POINTS, IMD_FREQ_DIV, voltage);

        /* 5c. 加窗 */
        apply_hanning(voltage, IMD_FFT_POINTS);

        /* 5d. DFT */
        if (!DFT(voltage, IMD_FFT_POINTS, fft_mag)) {
            BIN(23);
            goto END_IMD;
        }

        /* 5e. 累加幅度谱 */
        for (i = 0; i < IMD_FFT_POINTS / 2; i++) {
            fft_mag_sum[i] += fft_mag[i];
        }
    }

    /* 6. 求平均幅度谱 */
    for (i = 0; i < IMD_FFT_POINTS / 2; i++) {
        fft_mag[i] = fft_mag_sum[i] / (double)IMD_NAVG;
    }

    /* 7. 查找四个频率分量 */
    actual_sr = 50e6 / IMD_FREQ_DIV;
    freq_res  = actual_sr / IMD_FFT_POINTS;
    max_bin   = IMD_FFT_POINTS / 2;                              /* 1024 */

    bin_f1   = freq_to_bin(IMD_F1, freq_res, max_bin);           /* 300 Hz  → bin 6 */
    bin_f2   = freq_to_bin(IMD_F2, freq_res, max_bin);           /* 3400 Hz → bin 68 */
    bin_sum  = freq_to_bin(IMD_F1 + IMD_F2, freq_res, max_bin);  /* 3700 Hz → bin 74 */
    bin_diff = freq_to_bin(IMD_F2 - IMD_F1, freq_res, max_bin);  /* 3100 Hz → bin 62 */

    if (bin_f1 < 0 || bin_f2 < 0 || bin_sum < 0 || bin_diff < 0) {
        BIN(24);
        goto END_IMD;
    }

    /* 8. 计算 IMD */
    A_f1   = fft_mag[bin_f1];
    A_f2   = fft_mag[bin_f2];
    A_sum  = fft_mag[bin_sum];
    A_diff = fft_mag[bin_diff];

    A_imd  = (A_sum > A_diff) ? A_sum : A_diff;
    A_fund = (A_f1 > A_f2) ? A_f1 : A_f2;

    if (A_fund <= 0.0) {
        /* 基波丢失，失真无法计算 */
        BIN(25);
        SHOW_RESULT("IMD_FUND_LOST", 1, "", 1, 0);
        goto END_IMD;
    }

    IMD_db = 20.0 * log10(A_imd / A_fund);

    SHOW_RESULT("IMD", IMD_db, "dB", -41.0, No_LoLimit);
    if (IMD_db > -41.0) {
        BIN(26);
    } else {
        SHOW_RESULT("IMD=>PASS", 1, "", 1, 1);
    }

END_IMD:
    CLEAR_RELAY("7");
    DPS_OFF(DPS1);
    DPS_OFF(DPS2);
#endif
}
