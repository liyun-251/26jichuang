/* ============================================================
   test_09_gra_grr_grrl.cpp — 接收增益测试 (GRA + GRR + GRRL)
   编译：head0710.cpp + 本文件 复制进入工程
   依赖：GRR/GRRL 依赖 GRA 测得的 1020Hz 参考增益值（gra1020）

   GRA： 播放图形10(1020Hz 0dBm0) -> AVM 测 VFRO -> 计算绝对增益
         限值 +-0.15dB
   GRR： 遍历 300/3000Hz 0dBm0，分别计算相对 GRA 的增益偏差
         限值 +-0.15dB
   GRRL：遍历 -40/+3 dBm0，计算增益随电平的变化
         限值 +-0.2dB
   ============================================================ */





#define TEST_GRA   1
#define TEST_GRR   1
#define TEST_GRRL  1

void PASCAL TP3057()
{
    double gra1020_local;
    double vout_rms_gra;
    int start_idx_list[2];
    double freq_list_grr[2];
    int num_freq_grr;
    int fail;
    int i;
    double vout_rms_grr, Grf, GRR_val;
    char name[32];
    double level_dbm0[2];
    int index[2];
    int num;
    double Vref, Gref;
    int fail_grrl;
    int idx;
    double L;
    double vout_grrl, Gabs, delta;

    gra1020_local = 0.0;

    /* ==================== GRA ==================== */
#if TEST_GRA
    SETUP();

    vout_rms_gra = 0.0;
    RUN_PATTERN(10, 0, 0, 0);              /* 1020Hz 0dBm0 PCM 输入 */
    SET_AVM_PATH(LP20, BPPASS);            /* 20kHz 低通滤波 */
    vout_rms_gra = AVM_MEASURE(1, 2.0, V, 10);
    gra1020_local = 20.0 * log10(vout_rms_gra / 1.2276);
    gra1020 = gra1020_local;               /* 存入全局 */

    SHOW_RESULT("GRA", gra1020_local, "dB", 0.15, -0.15);
    if (gra1020_local <= -0.15 || gra1020_local >= 0.15) {
        BIN(11);
    }
#endif



    /* ==================== GRR ==================== */
#if TEST_GRR
    SETUP();

    start_idx_list[0]   = 8;     /* 8=300Hz */
    start_idx_list[1]   = 12;    /* 12=3000Hz (均为 0dBm0) */
    freq_list_grr[0]    = 300.0;
    freq_list_grr[1]    = 3000.0;
    num_freq_grr = 2;
    fail = 0;

    for (i = 0; i < num_freq_grr; i++) {
        RUN_PATTERN(start_idx_list[i], 0, 0, 0);
        SET_AVM_PATH(LP20, BPPASS);
        vout_rms_grr = AVM_MEASURE(1, 2.0, V, 10);
        Grf = 20.0 * log10(vout_rms_grr / 1.2276);
        GRR_val = Grf - gra1020_local;

        sprintf(name, "GRR_%dHz", (int)freq_list_grr[i]);
        SHOW_RESULT(name, GRR_val, "dB", 0.15, -0.15);

        if (GRR_val <= -0.15 || GRR_val >= 0.15) fail++;
    }

    if (fail != 0)
        {BIN(12);}
    else
        {SHOW_RESULT("GRR=>PASS", 1, "", 1, 1);}
#endif

    /* ==================== GRRL ==================== */
#if TEST_GRRL
    SETUP();

    level_dbm0[0] = -40.0;
    level_dbm0[1] = 3.0;
    index[0] = 9;       /* 9=-40dBm0 */
    index[1] = 11;      /* 11=+3dBm0 */
    num = 2;


    fail_grrl = 0;
    for (i = 0; i < num; i++) {
        idx = index[i];//测试使用的图形
        L   = level_dbm0[i];//测试的电平
        RUN_PATTERN(idx, 0, 0, 0);
        if (i == 0)
        {
            vout_grrl = AVM_MEASURE(1, 20.0, MV, 10) / 1000.0;//返回mv，除以1000.0得到V
        }
        if (i == 1)
        {
            vout_grrl = AVM_MEASURE(1, 2.0, V, 10);
        }
        
        Gabs = 20.0 * log10(vout_grrl / 1.2276);
        delta = Gabs - L - gra1020_local;

        sprintf(name, "GRRL_%+ddBm", (int)L);
        SHOW_RESULT(name, delta, "dB", 0.2, -0.2);

        if (delta < -0.2 || delta > 0.2) fail_grrl++;
    }

    if (fail_grrl != 0)
        {BIN(13);}
    else
        {SHOW_RESULT("GRRL=>PASS", 1, "", 1, 1);}
#endif

    DPS_OFF(DPS1);
    DPS_OFF(DPS2);
}
