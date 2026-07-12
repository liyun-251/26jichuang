/* ============================================================
   test_10_sfdx.cpp — 发送单频失真测试 (SFDX)
   编译：head0710.cpp + 本文件 复制进入工程
   语言标准：C95

   测试流程：
     1. SETUP + AS 输出 1020Hz 0dBm0 正弦波
     2. 运行图形7，从 fail memory 捕获 512 字节 PCM
     3. A-law 解码 -> 峰值电压序列
     4. 加汉宁窗 -> DFT -> 查找基波 + 2次/3次谐波
     5. 计算失真比 = 20*log10(max(H2,H3)/H1)
   限值：<= -46dB

   FFT 分辨率 = 8000/512 = 15.625Hz, 1020Hz 落在 bin 65(1015.6Hz)
   偏差 ~4.4Hz
   ============================================================ */


   




#define TEST_ENABLE  1

void PASCAL TP3057()
{
#if TEST_ENABLE
    unsigned char pcm_data[SFDX_FFT_POINTS];
    int captured;
    double voltage[SFDX_FFT_POINTS];
    double fft_mag[SFDX_FFT_POINTS / 2];
    double freq_res;
    int max_bin;
    int bin_fund, bin_2nd, bin_3rd;
    double SFDX_db;
    int i;

    /* 1. 初始化 */
    SETUP();
    SET_AS(1.2276, V, 1020, HZ);
    Delay(20);

    /* 2. 运行图形7 */
    RUN_PATTERN(7, 0, 0, 0);

    /* 3. 捕获 512 字节 PCM */
    captured = capture_multi_pcm_bytes_fsx(7, 46, 43, 0, pcm_data, SFDX_FFT_POINTS);
    if (captured != SFDX_FFT_POINTS) {
        SHOW_RESULT("SFDX_CAP_FAIL", (double)captured, "bytes", SFDX_FFT_POINTS, 0);
        BIN(14);
        goto END_SFDX;
    }

    /* 4. 解码为瞬时电压序列 */
    for (i = 0; i < SFDX_FFT_POINTS; i++) {
        double v_inst;
        v_inst = pcm_to_voltage(pcm_data[i]);
        voltage[i] = v_inst;
    }

    /* 5. 加窗 */
    apply_hanning(voltage, SFDX_FFT_POINTS);

    /* 6. FFT */
    if (!DFT(voltage, SFDX_FFT_POINTS, fft_mag)) {
        BIN(15);
        goto END_SFDX;
    }

    /* 7. 查找基波和谐波 bin */
    freq_res = SFDX_SAMPLE_RATE / SFDX_FFT_POINTS;   /* 每个bin频率 15.625 Hz */
    max_bin = SFDX_FFT_POINTS / 2; //最大 bin 号 = 256

    bin_fund = freq_to_bin(1020.0, freq_res, max_bin);//把目标频率转换为 bin 索引，bin_fund = 65
    bin_2nd  = freq_to_bin(2.0 * 1020.0, freq_res, max_bin);//bin_2nd = 130
    bin_3rd  = freq_to_bin(3.0 * 1020.0, freq_res, max_bin);//bin_3rd = 195

    if (bin_2nd < 0 || bin_3rd < 0) {
        BIN(16);
        goto END_SFDX;
    }

    /* 8. 计算失真 */
    SFDX_db = calc_distortion_db(fft_mag[bin_fund],
                                  fft_mag[bin_2nd],
                                  fft_mag[bin_3rd]);


    if (SFDX_db >= 0.9) {
    /* 基波异常，失真无法计算 */
        BIN(17);
        SHOW_RESULT("SFDX_bin_fund_LOST", 1, "", 1, 0);
    } 
    else {
        SHOW_RESULT("SFDX", SFDX_db, "dB", -46, No_LoLimit);
        if (SFDX_db > -46) {
            BIN(18);
        } 
        else {
            SHOW_RESULT("SFDX=>PASS", 1, "", 1, 1);
        }
    }

END_SFDX:
    DPS_OFF(DPS1);
    DPS_OFF(DPS2);
#endif
}
