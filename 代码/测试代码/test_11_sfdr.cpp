/* ============================================================
   test_11_sfdr.cpp — 接收单频失真测试 (SFDR)
   编译：head0710.cpp + 本文件 复制进入工程
   语言标准：C95

   测试流程：
     1. SETUP -> 运行图形10(1020Hz 0dBm0 PCM 码流)
     2. MAT_DVM_MEASURE 连续采样 2048 点 (采样率 ~130.5kHz, FREQ_DIV=383)
     3. 加汉宁窗 -> DFT → 4次测量累加幅度谱求平均
     4. 查找基波 + 2次/3次谐波 -> 计算失真比
   限值：<= -46dB

   FFT 参数：Fs=50e6/383≈130548Hz  N=2048
            Δf=130548/2048≈63.74Hz  1020Hz→bin16(1019.9Hz) 偏差0.1Hz
   ============================================================ */








#define TEST_ENABLE  1

#define SFDR_FREQ_DIV  383         /* 50MHz/383≈130.5kHz, 1020Hz 对齐 bin16 */
#define SFDR_NAVG      4           /* 测量平均次数 */

void PASCAL TP3057()
{
#if TEST_ENABLE
    double voltage[SFDR_FFT_POINTS];              /* 2048 */
    double fft_mag[SFDR_FFT_POINTS / 2];          /* 1024 */
    double fft_mag_sum[SFDR_FFT_POINTS / 2];      /* 1024, 累加用 */
    double actual_sr, freq_res;
    int max_bin;
    int bin_fund, bin_2nd, bin_3rd;
    double SFDR_db;
    int run, i;

    /* 1. 初始化 */
    SETUP();

    /* 2. 清零累加数组 */
    for (i = 0; i < SFDR_FFT_POINTS / 2; i++) {
        fft_mag_sum[i] = 0.0;
    }

    /* 3. 多次测量，累加幅度谱 */
    for (run = 0; run < SFDR_NAVG; run++) {
        /* 3a. 运行接收图形10 */
        if (!RUN_PATTERN(10, 0, 0, 0)) {
            BIN(18);
            SHOW_RESULT("SFDR_PAT_FAIL", 1, "", 1, 0);
            goto END_SFDR;
        }
        Delay(10);

        /* 3b. DVM 连续采样 */
        MAT_DVM_MEASURE(1, 2.0, V, 10, SFDR_FFT_POINTS, SFDR_FREQ_DIV, voltage);

        /* 3c. 加窗 */
        apply_hanning(voltage, SFDR_FFT_POINTS);

        /* 3d. DFT */
        if (!DFT(voltage, SFDR_FFT_POINTS, fft_mag)) {
            BIN(19);
            goto END_SFDR;
        }

        /* 3e. 累加幅度谱 */
        for (i = 0; i < SFDR_FFT_POINTS / 2; i++) {
            fft_mag_sum[i] += fft_mag[i];
        }
    }

    /* 5. 求平均幅度谱 */
    for (i = 0; i < SFDR_FFT_POINTS / 2; i++) {
        fft_mag[i] = fft_mag_sum[i] / (double)SFDR_NAVG;
    }

    /* 6. 查找基波和谐波 bin */
    actual_sr = 50e6 / SFDR_FREQ_DIV;
    freq_res  = actual_sr / SFDR_FFT_POINTS;
    max_bin   = SFDR_FFT_POINTS / 2;               /* 1024 */

    bin_fund = freq_to_bin(1020.0, freq_res, max_bin);       /* bin 16 */
    bin_2nd  = freq_to_bin(2.0 * 1020.0, freq_res, max_bin); /* bin 32 */
    bin_3rd  = freq_to_bin(3.0 * 1020.0, freq_res, max_bin); /* bin 48 */

    if (bin_2nd < 0 || bin_3rd < 0) {
        BIN(20);
        goto END_SFDR;
    }

    /* 7. 计算失真 */
    SFDR_db = calc_distortion_db(fft_mag[bin_fund],
                                  fft_mag[bin_2nd],
                                  fft_mag[bin_3rd]);

    if (SFDR_db >= 0.9) {
        /* 基波异常，失真无法计算 */
        BIN(21);
        SHOW_RESULT("SFDR_FUND_LOST", 1, "", 1, 0);
    } else {
        SHOW_RESULT("SFDR", SFDR_db, "dB", -46, No_LoLimit);
        if (SFDR_db > -46) {
            BIN(22);
        } else {
            SHOW_RESULT("SFDR=>PASS", 1, "", 1, 1);
        }
    }

END_SFDR:
    DPS_OFF(DPS1);
    DPS_OFF(DPS2);
#endif
}
