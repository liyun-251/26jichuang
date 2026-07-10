/* ============================================================
   test_pcm_capture.cpp — PCM 读取验证程序
   编译：head0710.cpp + 本文件 复制进入工程
   语言标准：C95

   用途：上机调试第一步！在所有 AC 测试之前运行，验证：
     1. GET_PATTERN_RESULT 是否能正确获取 fail memory
     2. PCM 字节的位提取逻辑是否正确
     3. A-law 解码是否正确（电压值是否合理）
     4. 多字节连续捕获是否稳定

   激励可选：
     USE_AS_SOURCE=0 -> 用预存图形 + SET_AS
     USE_AS_SOURCE=1 -> 用 AS 自定义波形

   输出：前 16 字节的 hex/Vrms 值 + 统计摘要
   ============================================================ */




   
/* ==================== 调试配置 ==================== */
/*
    用哪个图形来验证 PCM？
    7  -> AS 输出已知正弦波（需先 LOAD_AS_PATTERN + RUN_AS_PATTERN）
    10 -> 1020Hz 0dBm0 预存 PCM（接收方向）
    8  -> 300Hz  0dBm0 预存 PCM



    更改PCM码数（）如32改为512
    1 修改 #define CAPTURE_COUNT 32 -> 512
    2 修改 unsigned char pcm_buf[32]; -> unsigned char pcm_buf[512];

 */
#define PCM_TEST_PATTERN    7 //运行图形编号 图形 10 = "1020Hz 0dBm0 预存 PCM 码流"

/* 捕获字节数（建议 32-128，太多会刷屏） */
#define CAPTURE_COUNT        32

/* 调试输出：读取fail memory的前N个字节，每组K个字节合并为一个值输出 */
#define DBG_BYTE_COUNT       32    /* 每个通道输出的总字节数 */
#define DBG_BYTES_PER_ROW    1     /* 每个SHOW_RESULT合并几个字节 (1=8bit, 2=16bit, 4=32bit) */

/* 是否用 AS 源生成波形？ 0=SET_AS, 1=LOAD_AS_PATTERN */
#define USE_AS_SOURCE        0 //不需要 恒为0

/* AS 波形参数（仅 USE_AS_SOURCE=1 时生效） */
#define AS_FREQ_HZ           1020.0
#define AS_LEVEL_DBM         0.0
#define AS_POINTS            1024
#define AS_SAMPLE_RATE       100000.0

/* 期望电压范围（用于自动判定；0dBm0 -> 1.2276Vrms, Vpeak~1.736V） */
#define EXPECT_VPEAK_MIN     1.3
#define EXPECT_VPEAK_MAX     2.0
/* ================================================= */

void PASCAL TP3057()
{
/*  int pcm_single;  //单字节捕获已注释，对应变量暂不使用
    double vrms_single; */
    unsigned char pcm_buf[CAPTURE_COUNT];        /*多字节缓冲区 CAPTURE_COUNT */
    int captured_multi_pcm;  //用来存放capture_multi_pcm_bytes的返回值
    int show_count;
    int i;
    double v_min, v_max, v_rms_sum;
    double v_rms_total, v_peak_max;
    int zero_or_ff, d5_count;
    char name_hex[32], name_v[32];
#if USE_AS_SOURCE
    double as_amp_rms, as_amp_peak, as_freq_div;
    WORD as_wave[1024];
    double t_ts, v_as;
#endif
    BYTE* fb; //fail memory 指针，用于诊断模式下直接打印深度信息
    int dp;   //fail memory 深度，用于诊断模式下直接打印深度信息
    double v_by;  //循环中的每个pcm字节解码后的电压值（临时）

    SETUP();
    Delay(20);



    SHOW_RESULT("===PCM Cap Test===", 1, "", 1, 1);//显示标题


/*
#if USE_AS_SOURCE
    //---- AS 自定义波形 ---- 
    as_amp_rms  = 1.2276 * pow(10.0, AS_LEVEL_DBM / 20.0);
    as_amp_peak = as_amp_rms * sqrt(2.0);
    as_freq_div = 100e6 / AS_SAMPLE_RATE;

    for (i = 0; i < AS_POINTS; i++) {
        t_ts = (double)i / AS_SAMPLE_RATE;
        v_as = as_amp_peak * sin(2.0 * M_PI * AS_FREQ_HZ * t_ts);
        if (v_as > 10.0) v_as = 10.0;
        if (v_as < -10.0) v_as = -10.0;
        as_wave[i] = (WORD)(((v_as / 10.0) + 1.0) / 2.0 * 65535.0 + 0.5);
    }
    LOAD_AS_PATTERN(7, 1, as_wave);
    RUN_AS_PATTERN(7, 1, (int)as_freq_div, 10);
    Delay(10);
#else
    // ---- 传统 SET_AS ---- 
    SET_AS(1.2276, V, 1020, HZ);
    Delay(20);
#endif

*/


    /* ---- 运行 PCM 捕获图形 ---- */
    RUN_PATTERN(PCM_TEST_PATTERN, 0, 0, 0);//PCM_TEST_PATTERN=10 运行图形10 = "1020Hz 0dBm0 预存 PCM 码流"
    Delay(10);
    SET_AS(1.2276, V, 1020, HZ);
    Delay(20);
/* 单字节捕获后续不会使用，无需测试
    // ========== 1. 单字节捕获 ========== 
    pcm_single = capture_one_pcm_byte(PCM_TEST_PATTERN, 46, 4);//capture_one_pcm_byte输入参数：图形编号、DX通道号、数据位起始周期偏移
    
    if (pcm_single < 0) {//捕获失败
        SHOW_RESULT("SINGLE_CAP_FAIL", 1, "", 1, 0);
    }

    vrms_single = pcm_to_voltage(pcm_single);
    SHOW_RESULT("SINGLE_PCM_hex",  (double)pcm_single, "hex", 0, 0);
    SHOW_RESULT("SINGLE_Vrms", vrms_single,        "V",   0, 0);
    

 */


    /* ========== 2. 多字节捕获 ========== */
    captured_multi_pcm = capture_multi_pcm_bytes_fsx(PCM_TEST_PATTERN, 46, 43, 0, //输入参数：图形编号、DX通道号46、FSX监测通道号43、起始搜索周期
                                        pcm_buf, CAPTURE_COUNT); //pcm_bytes 输出缓冲区指针、num_bytes 要捕获的字节数
    SHOW_RESULT("captured_multi_pcm", (double)captured_multi_pcm, "cnt", CAPTURE_COUNT, 0);

    /* ========== 2.5 调试：FSX/DX 通道原始值（逐字节，一行FSX一行DX） ========== */
    {
        BYTE* fsx_fb_dbg;
        BYTE* dx_fb_dbg;
        int fsx_dp_dbg, dx_dp_dbg;
        int grp, b, byte_end;
        unsigned int fsx_val, dx_val;
        char dbg_name[32];

        fsx_fb_dbg = GET_PATTERN_RESULT(PCM_TEST_PATTERN, 43, &fsx_dp_dbg);
        dx_fb_dbg  = GET_PATTERN_RESULT(PCM_TEST_PATTERN, 46, &dx_dp_dbg);

        if (fsx_fb_dbg != NULL && dx_fb_dbg != NULL) {
            byte_end = DBG_BYTE_COUNT;
            if (byte_end > fsx_dp_dbg) byte_end = fsx_dp_dbg;
            if (byte_end > dx_dp_dbg)  byte_end = dx_dp_dbg;  //确定输出的字节数不能超过任意一个通道的深度

            for (grp = 0; grp < byte_end; grp += DBG_BYTES_PER_ROW) {//大循环 grp为每行的起始字节索引，步长为 DBG_BYTES_PER_ROW

                /* FSX 行：合并 DBG_BYTES_PER_ROW 个字节 */
                fsx_val = 0;
                for (b = 0; b < DBG_BYTES_PER_ROW && (grp + b) < byte_end; b++) {//小循环 b为每行的每个字节索引，步长为1 为了合并 DBG_BYTES_PER_ROW 个字节
                    fsx_val = (fsx_val << 8) | fsx_fb_dbg[grp + b];//将每个字节左移8位后与当前字节按位或，合并为一个整数
                }
                sprintf_s(dbg_name, sizeof(dbg_name), "FSX_B%02d", grp);
                SHOW_RESULT(dbg_name, (double)fsx_val, "", 0, 0);

                /* DX 行：合并 DBG_BYTES_PER_ROW 个字节 */
                dx_val = 0;
                for (b = 0; b < DBG_BYTES_PER_ROW && (grp + b) < byte_end; b++) {
                    dx_val = (dx_val << 8) | dx_fb_dbg[grp + b];
                }
                sprintf_s(dbg_name, sizeof(dbg_name), "DX_B%02d", grp);
                SHOW_RESULT(dbg_name, (double)dx_val, "", 0, 0);
            }
        }
    }

    if (captured_multi_pcm == 0) {
        /* ---- 诊断模式：直接打印 depth 信息 ---- */
        fb = NULL;//诊断模式下获取的fail memory指针
        dp = 0;//诊断模式下获取的fail memory的深度
        fb = GET_PATTERN_RESULT(PCM_TEST_PATTERN, 46, &dp);
        SHOW_RESULT("DIAG_depth",    (double)dp,               "bytes", 0, 0);//输出深度 dp=0：fail memory 无效；dp>0：fail memory 有效，且深度为 dp 字节
                                                                              //512周期需要 130828 bit，约 16354 字节； 因此 dp/depth 应该大于 16354 才能完整覆盖 512 周期的 PCM 数据
        SHOW_RESULT("DIAG_failbits", (fb == NULL) ? 0.0 : 1.0, "ptr",  0, 0); //输出指针有效性 fb=NULL：输出0 fail memory 无效；fb!=NULL：输出1 fail memory 有效
        SHOW_RESULT("MULTI_CAP_FAIL", 1, "", 1, 0); //显示多字节捕获失败
        goto END_PCM_TEST;
    }

    /* ========== 3. 逐字节打印 ========== */
    show_count = (captured_multi_pcm < 16) ? captured_multi_pcm : 16;//限制captured数量，最多显示前16字节
    for (i = 0; i < show_count; i++) {//遍历前 show_count 个捕获的 PCM 字节
        v_by = pcm_to_voltage(pcm_buf[i]);
        sprintf_s(name_hex, sizeof(name_hex), "PCM[%02d]_hex", i);
        sprintf_s(name_v,   sizeof(name_v),   "PCM[%02d]_V",   i);
        SHOW_RESULT(name_hex, (double)(pcm_buf[i]), "hex", 0, 0);
        SHOW_RESULT(name_v,   v_by,                 "V",   0, 0);
    }

    /* ========== 4. 统计分析 ========== */
    v_min = 100.0;
    v_max = -100.0;
    v_rms_sum = 0.0;
    zero_or_ff = 0; //统计0x00或0xFF 异常的字节数
    d5_count = 0;

    for (i = 0; i < captured_multi_pcm; i++) {
        v_by = pcm_to_voltage(pcm_buf[i]);
        if (v_by < v_min) v_min = v_by;//找出最小电压
        if (v_by > v_max) v_max = v_by;//找出最大电压
        v_rms_sum += v_by * v_by;//累加平方和，用于计算均方根
        if (pcm_buf[i] == 0x00 || pcm_buf[i] == 0xFF) zero_or_ff++;// 统计0x00或0xFF 异常的字节数 0x00：每个bit全0 0xFF：每个bit全1
        if (pcm_buf[i] == 0xD5) d5_count++;   //过零点的个数
    }
    v_rms_total = sqrt(v_rms_sum / captured_multi_pcm);//均方根电压 应接近 1.2276Vrms
    v_peak_max  = v_max * sqrt(2.0);   /* RMS -> 峰值估计 */

    SHOW_RESULT("STAT_Vrms_min",  v_min,         "V",   0, 0);//最小电压
    SHOW_RESULT("STAT_Vrms_max",  v_max,         "V",   0, 0);//最大电压
    SHOW_RESULT("STAT_Vrms_rss",  v_rms_total,   "V",   0, 0);//均方根电压 应接近 1.2276Vrms
    SHOW_RESULT("STAT_Vpeak_est", v_peak_max,    "V",   0, 0);//峰值电压估计 应接近 1.736V
    SHOW_RESULT("STAT_0xFF_cnt",  (double)zero_or_ff, "cnt", 0, 0);//0x00或0xFF异常字节数
    SHOW_RESULT("STAT_0xD5_cnt",  (double)d5_count,   "cnt", 0, 0);//过零点个数

    /* ========== 5. 自动判定 ========== */
    /* 如果捕获的是 0dBm0 正弦波，预期 peak 约 1.736V */
    if (v_peak_max > EXPECT_VPEAK_MIN && v_peak_max < EXPECT_VPEAK_MAX
        && zero_or_ff <= 2)
    {
        SHOW_RESULT("RESULT=>PASS", 1, "", 1, 1);
        BIN(1);
    }
    else if (v_peak_max > 0.05 && zero_or_ff < captured_multi_pcm / 2)
    {
        SHOW_RESULT("RESULT=>CHECK", 0.5, "", 1, 0);
        BIN(2);
    }
    else
    {
        SHOW_RESULT("RESULT=>FAIL", 0, "", 1, 0);
        BIN(3);
    }

END_PCM_TEST:
    DPS_OFF(DPS1);
    DPS_OFF(DPS2);
}
