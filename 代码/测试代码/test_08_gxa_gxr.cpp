/* ============================================================
   test_08_gxa_gxr.cpp — 发送增益测试 (GXA + GXR)
   编译：head0710.cpp + 本文件 复制进入工程
   依赖：GXR 依赖 GXA 测得的 1020Hz 参考增益值（存在全局变量 gxa1020 中）

   GXA：1020Hz 0dBm0 -> TX -> 捕获 DX PCM -> 计算绝对增益
        限值 +-0.15dB
   GXR：遍历 300/1000/2000/3000 Hz，分别计算相对 GXA 的增益偏差
        限值 +-0.15dB
   ============================================================ */





#define TEST_GXA  1    /* 1=执行, 0=跳过 */
#define TEST_GXR  1

void PASCAL TP3057()
{
    double vout_rms_gxa, gain_db;
    unsigned char pcm_buf_gxa[512];
    unsigned char pcm_buf_gxr[512];
    int num_byte;//capture_multi_pcm_bytes的返回值，捕获的pcm_byte个数
    int num_byte_gxr;
    double freq_list_gxr[4];
    int num_freq_gxr;
    int fail_count;
    int i;
    double f;
    double vout_rms_gxr, Gxf, GXRf;
    char name[32];

    gxa1020 = 0.0;

    /* ==================== GXA ==================== */
#if TEST_GXA
    SETUP();
    SET_AS(1.2276, V, 1020, HZ);          /* 音频源: 1020Hz, 0dBm0 */
/*
    RUN_PATTERN(7, 0, 0, 0);
    pcm_byte = capture_one_pcm_byte(7, 46, 4);
    if (pcm_byte == 0xFF) {
        BIN(7);                            //捕获失败 
        SHOW_RESULT("GXA_CAP_FAIL", 1, "", 1, 0);
        goto END_GXA;
    }
*/


    RUN_PATTERN(7, 0, 0, 0);
    num_byte = capture_multi_pcm_bytes_fsx(7, 46, 43, 0, pcm_buf_gxa, 512);
    if (num_byte == 0) {
        BIN(7);                            //捕获失败 
        SHOW_RESULT("GXA_CAP_FAIL", 1, "", 1, 0);
        goto END_GXA;
    }

/*
    vout_rms_gxa = pcm_to_voltage(pcm_buf_gxa);
    gain_db = 20.0 * log10(vout_rms_gxa / 1.2276);

    gxa1020 = gain_db;                     //存入全局，供 GXR 使用 
*/

    vout_rms_gxa = compute_vrms(pcm_buf_gxa, num_byte);
    gain_db = 20.0 * log10(vout_rms_gxa / 1.2276);
    gxa1020 = gain_db;



    SHOW_RESULT("GXA", gxa1020 , "dB", 0.15, -0.15);
    if (gxa1020  <= -0.15 || gxa1020  >= 0.15) {
        BIN(8);
    }
#endif
END_GXA:



    /* ==================== GXR ==================== */
#if TEST_GXR
    SETUP();

    freq_list_gxr[0] = 300.0;
    freq_list_gxr[1] = 1000.0;
    freq_list_gxr[2] = 2000.0;
    freq_list_gxr[3] = 3000.0;
    num_freq_gxr = 4;
    fail_count = 0;

    for (i = 0; i < num_freq_gxr; i++) {
        f = freq_list_gxr[i];
        SET_AS(1.2276, V, f, HZ);
        Delay(10);

        RUN_PATTERN(7, 0, 0, 0);
        num_byte_gxr = capture_multi_pcm_bytes_fsx(7, 46, 43, 0, pcm_buf_gxr, 512);
        if (num_byte_gxr == 0) {
            BIN(9);
            fail_count++;
            SHOW_RESULT("GXR_CAP_FAIL", f, "Hz", 0, 0);
            continue;
        }

        vout_rms_gxr = compute_vrms(pcm_buf_gxr, num_byte_gxr);
        Gxf  = 20.0 * log10(vout_rms_gxr / 1.2276);
        GXRf = Gxf - gxa1020;

        sprintf(name, "GXR_%dHz", (int)f);
        SHOW_RESULT(name, GXRf, "dB", 0.15, -0.15);

        if (GXRf <= -0.15 || GXRf >= 0.15) {
            fail_count++;
        }
    }

    if (fail_count != 0)
        {BIN(10);}
    else
        {SHOW_RESULT("GXR=>PASS", 1, "", 1, 1);}
#endif

    DPS_OFF(DPS1);
    DPS_OFF(DPS2);
}
