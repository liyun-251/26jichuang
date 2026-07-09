//TP3057

#include "StdAfx.h"
#include "testdef.h"
#include "data.h"
#include "math.h"




#define VPEAK_FS        2.492           // TP3057 A-law 满量程峰值 (V)
#define LINEAR_MAX      8031            // 线性编码最大值
//sfdx
const double M_PI = 3.14159265358979323846;
#define SFDX_FFT_POINTS    512         // FFT 点数（同时是捕获的 PCM 字节数）
#define SFDX_SAMPLE_RATE   8000.0      // PCM 采样率 (Hz)
// SFDR 测试参数
#define SFDR_FFT_POINTS    1024        // FFT 点数（必须是2的幂）
#define SFDR_DVM_SR        100000.0    // DVM 采样率 (Hz), 100 kHz
#define SFDR_FREQ_RES      (SFDR_DVM_SR / SFDR_FFT_POINTS)  // 频率分辨率
// 双音参数
// ---------- 本地常量 ----------
const double IMD_F1 = 300.0;
const double IMD_F2 = 3400.0;
const double IMD_TEST_LEVEL_DBM = -10.0;
const double IMD_V_TOTAL_RMS = 1.2276 * pow(10.0, IMD_TEST_LEVEL_DBM / 20.0); // 0.388 Vrms
const double IMD_V_SINGLE_PEAK = IMD_V_TOTAL_RMS; // 每个单音峰值
const int IMD_PCM_BYTES = 512;
const int IMD_FFT_POINTS = IMD_PCM_BYTES;
const double IMD_FS_PCM = 8000.0;
const int IMD_AS_POINTS = 1024;
const double IMD_AS_SAMPLE_RATE = 100000.0;
const int IMD_AS_FREQ_DIV = (int)(50e6 / IMD_AS_SAMPLE_RATE); // 500
// 图形7专用参数
const int IMD_FIRST_FSX = 4;      // 第一个FSX脉冲周期索引




double gxa1020 = 0.0;
double gra1020 = 0.0;


unsigned char capture_one_pcm_byte(int start_idx, int dx_channel, int offset);
double pcm_to_voltage_rms(unsigned char tp3057_byte);
int capture_multi_pcm_bytes(int start_idx, int dx_channel, int offset,
    unsigned char* pcm_bytes, int num_bytes);
void apply_hanning(double* data, int n);


void SETUP(void)
{
    SET_DPS(DPS1, 5.0, V, 100, MA);
    SET_DPS(DPS2, -5.0, V, 100, MA);
    Delay(10);
    SET_PERIOD(488);
    SET_TIMING(100, 350, 450);
    SET_INPUT_LEVEL(2.2, 0.6);
    SET_OUTPUT_LEVEL(2.4, 0.4);
    FORMAT(NRZ0, "48,4,47,46,1,2");   // FSR-1;DR-2;BCLKR-3;PDN-4;BCLKX-44;MCLKX-45;DX-46；FSX-47；TSX-48
    FORMAT(RZ, "45,44,3");
    RUN_PATTERN(3, 1, 0, 0);   // 运行图形3，产生 FSX 脉冲，使芯片上电
    Delay(10);
}


void PASCAL TP3057()
{


    /****************************************
     本测试程序，样片 TP3057
     分箱说明：
     连接性测试失败------------BIN(1)
     IIL 超过限定值------------BIN(2)
     IIH 超过限定值------------BIN(3)
     IOZ 超过限定值------------BIN(4)
     ICC0IBB0 超过限定值-------BIN(5)
     ICC1IBB1 超过限定值-------BIN(6)
     GXA 失效存储器读取失败-----BIN(7)
     GXA 超过限定值------------BIN(8)
     GXR 失效存储器读取失败-----BIN(9)
     GXR 超过限定值------------BIN(10)
     GRA 超过限定值------------BIN(11)
     GRR 超过限定值------------BIN(12)
     GRR 超过限定值------------BIN(13)
     SFDX PCM码捕获失败--------BIN(14)
     SFDX FFT失败--------------BIN(15)
     SFDX 频率超出范围---------BIN(16)
     SFDX 超过限定值-----------BIN(17)
     SFDR PCM码捕获失败--------BIN(18)
     SFDR FFT失败--------------BIN(19)
     SFDR 频率超出范围---------BIN(20)
     SFDR 超过限定值-----------BIN(21)
     IDM PCM码捕获失败---------BIN(22)
     IDM FFT失败---------------BIN(23)
     IDM 频率超出范围----------BIN(24)
     IDM 超过限定值------------BIN(25)
     功能测试失败--------------BIN(26)



     图形说明：
     0--------功能测试
     1--------IIL拉高输入引脚
     2--------IIH拉低输入
     3--------上电取消DX高阻态
     4--------上电 DX高阻态
     5--------掉电
     6--------工作状态，用于测IBBICC
     7--------抓取PCM通用
     8--------300Hz 0dBm0 PCM输入
     9--------1020Hz -40dBm0 PCM输入
     10-------1020Hz 0dBm0 PCM输入
     11-------1020Hz 3dBm0 PCM输入
     12-------3000Hz 0dBm0 PCM输入
    ****************************************/




    //CON-TEST 
    CLOSE_RELAY("3,4,5,6"); //闭合继电器，使模拟脚连接pmu
    SET_DPS(1, 0, V, 20, MA); //选用1号电源通道DPS1，将VCC电压设为0
    SET_DPS(2, 0, V, 20, MA); //选用2号电源通道DPS2，将VBB电压设为0
    PMU_CONDITIONS(FIMV, -0.1, MA, 2, V); //加流测压，施加电流为-0.1 mA，电压箝位值设为2 V
    if (!PMU_MEASURE("1,2,3,4,5,44,45,46,47,48", 20, "CON_", V, -0.1, -1.9)) //依次测量适配器1-5，44-48通道的电压，判断是否在-0.1 V至-1.9 V之间
    {
        BIN(1);
        goto END_TEST;               // 连接性测试失败，直接跳到函数末尾退出
    }
    CLEAR_RELAY("3,4,5,6");          // 测试通过，也需清除继电器
    Delay(10);


    //系统初始化（为后续图形运行做准备）
    CLEAR_ALL();


    //FUN-TEST-回环
    SETUP();
    
    SET_RELAY("7");
    Delay(10);
    SET_VS1(2.5,V);
    Delay(10);
    RUN_PATTERN(13,0,0,0);
    double Vout=AVM_MEASURE(1,5,V,5);
    CLEAR_RELAY("7");
    double backloop_db = 20.0 * log10(Vout / 1.2276);
    if (backloop_db < -0.3225 || backloop_db > 0.3225) {
        SHOW_RESULT("FUNC_FAIL",backloop_db,"dB",0.3225,-0.3225);
        BIN(26);
    }
    else{
        SHOW_RESULT("FUNC_PASS",backloop_db,"dB",0.3225,-0.3225);
    }

/*
    //FUN-TEST-初赛版
    SETUP();

    SET_RELAY("1,2");
    Delay(10);
    SET_VS1(2.5, V);
    Delay(10);
    RUN_PATTERN("FUNC_T", 0, 1, 0, 0);
    Delay(10);
    double Vout = DVM_MEASURE(1, 3, V, 10);
    if (Vout <= 2.7 && Vout >= 2.3) {
        SHOW_RESULT("FUNC_R", Vout, "V", 2.7, 2.3);

    }
    else {
        SHOW_RESULT("FUNC_R", Vout, "V", 2.7, 2.3);
        BIN(26);

    }
    CLEAR_RELAY("1,2");

*/

    //IIL-TEST  
    SETUP();
    RUN_PATTERN(1, 1, 0, 0); //运行1号图形文件，使所有输入端置“1”
    PMU_CONDITIONS(FVMI, 0.4, V, 100, UA); //加压测流
    if (!PMU_MEASURE("1,2,3,4,44,45,47,48", 5, "IIL", UA, 10, -10)) //测量并判断输入管脚的电流
        BIN(2);


    //IIH-TEST  
    SETUP();
    RUN_PATTERN(2, 1, 0, 0); //运行2号图形文件，使所有输入端置“0”
    PMU_CONDITIONS(FVMI, 3, V, 100, UA); //加压测流
    if (!PMU_MEASURE("1,2,3,4,44,45,47,48", 5, "IIH", UA, 10, -10)) //测量并判断输入管脚的电流
        BIN(3);


    //IOZ-TEST   
    SETUP();
    RUN_PATTERN(4, 1, 0, 0); //运行4号图形文件，使Dx进入三态高阻
    PMU_CONDITIONS(FVMI, 0, V, 100, UA);//加压测流，测IOZL（对Vcc漏电）
    if (!PMU_MEASURE("46", 5, "IOZL", UA, 10, -10))//测量并判断Dx的电流
        BIN(4);
    PMU_CONDITIONS(FVMI, 5, V, 100, UA); //加压测流，测IOZH（对gnd漏电）
    if (!PMU_MEASURE("46", 5, "IOZH", UA, 10, -10)) //测量并判断Dx的电流
        BIN(4);




    //ICC0IBB0-TEST   
    SETUP();
    RUN_PATTERN(5, 1, 0, 0); //使芯片进入掉电状态，测量ICC0IBB0
    Delay(50);
    if (!DPS_MEASURE(DPS1, R20MA, 5, "ICC0", MA, 1.5, No_LoLimit)) {//这里量程也可以使用2ma
        BIN(5);
    }
    if (!DPS_MEASURE(DPS2, R2MA, 5, "IBB0", MA, 0.3, No_LoLimit)) {
        BIN(5);
    }


    //ICC1IBB1-TEST
    SETUP();
    SET_DPS(1, 5, V, 100, MA); //选用1号电源通道DPS1，将VCC电压设为5
    SET_DPS(2, -5, V, 100, MA); //选用2号电源通道DPS2，将VBB电压设为5
    RUN_PATTERN(6, 1, 0, 0); //运行6号图形文件，使芯片进入上电状态，测量ICC1IBB1
    Delay(50);

    if (!DPS_MEASURE(DPS1, R20MA, 5, "ICC1", MA, 9, No_LoLimit)) {
        BIN(6);
    }
    if (!DPS_MEASURE(DPS2, R20MA, 5, "IBB1", MA, 9, No_LoLimit)) {
        BIN(6);
    }




    //GXA
    SETUP();
    SET_AS(1.2276, V, 1020, HZ);       // 音频源输出1020 Hz, 1.2276 Vrms
    RUN_PATTERN(7, 0, 0, 0);
    unsigned char pcm_byte = capture_one_pcm_byte(7, 46, 4);  // 图形索引7，DX通道46,offset = 4;根据图形7，第一个fsx脉冲将出现在第五个周期（从0开始计数为4）
    if (pcm_byte == 0xFF) {
        BIN(7);   // 捕获失败
        goto AFTER_GXR;//跳过GXA，GXR
    }

    double vout_rms_gxa = pcm_to_voltage_rms(pcm_byte);
    double gain_db = 20.0 * log10(vout_rms_gxa / 1.2276);
    gxa1020 = gain_db;
    // ========== 判断并显示结果 ==========
    if (gain_db <= -0.15 || gain_db >= 0.15) {
        SHOW_RESULT("GXA", gain_db, "dB", 0.15, -0.15);
        BIN(8);
    }
    else {
        SHOW_RESULT("GXA", gain_db, "dB", 0.15, -0.15);
    }





    //GXR
    SETUP();
    double freq_list_gxr[] = { 300.0, 1000.0, 2000.0, 3000.0 };
    int num_freq_gxr = sizeof(freq_list_gxr) / sizeof(freq_list_gxr[0]);//计算列表中元素数量
    int fail_count = 0;//gxr在规定范围之外的个数

    for (int i = 0; i < num_freq_gxr; i++) {
        double f = freq_list_gxr[i];
        SET_AS(1.2276, V, f, HZ);
        Delay(10);

        RUN_PATTERN(7, 0, 0, 0);          // 运行图形段1（9个周期）
        unsigned char pcm_byte = capture_one_pcm_byte(7, 46, 4);
        if (pcm_byte == 0xFF) {
            BIN(9);   // 捕获失败，分箱
            fail_count++;
            continue;
        }
        double vout_rms_gxr = pcm_to_voltage_rms(pcm_byte);
        double Gxf = 20.0 * log10(vout_rms_gxr / 1.2276);
        double GXRf = Gxf - gxa1020;

        if (GXRf <= -0.15 || GXRf >= 0.15) {
            SHOW_RESULT("GXR_Hz", GXRf, "dB", 0.15, -0.15);
            fail_count++;
        }
        else {
            SHOW_RESULT("GXR_Hz", GXRf, "dB", 0.15, -0.15);
        }
    }

    if (fail_count != 0) BIN(10);

AFTER_GXR:





    //GRA
    SETUP();
    double vout_rms_gra = 0.0;
    RUN_PATTERN(10, 0, 0, 0);
    SET_AVM_PATH(LP20, BPPASS);// 开启 20kHz 低通滤波，滤除高频噪声
    // 测量 AVM_CHANNEL 通道，量程 AVM_RANGE_V，延时 DELAY_MS
    vout_rms_gra = AVM_MEASURE(1, 2.0, V, 10);
    gra1020 = 20.0 * log10(vout_rms_gra / 1.2276);

    // 结果判断及分箱 
    if (gra1020 <= -0.15 || gra1020 >= 0.15)
    {
        SHOW_RESULT("gra1020", gra1020, "dB", 0.15, -0.15);
        BIN(11);
    }
    else
    {
        SHOW_RESULT("gra1020", gra1020, "dB", 0.15, -0.15);
    }




    //GRR
    SETUP();
    int start_idx_list[] = { 8, 12 };  // 分别对应 300,3000 Hz 0dBm0 PCM输入
    double freq_list_grr[] = { 300.0, 3000.0 };
    int num_freq_grr = sizeof(freq_list_grr) / sizeof(freq_list_grr[0]);//计算列表中元素数量
    int fail = 0;

    // 循环测试
    for (int i = 0; i < num_freq_grr; i++)
    {
        // 运行对应频率的图形
        RUN_PATTERN(start_idx_list[i], 0, 0, 0);
        SET_AVM_PATH(LP20, BPPASS);
        double vout_rms_grr = AVM_MEASURE(1, 2.0, V, 10);
        double Grf = 20.0 * log10(vout_rms_grr / 1.2276);
        double GRR = Grf - gra1020;

        if (GRR <= -0.15 || GRR >= 0.15)
        {
            SHOW_RESULT("GRR_Hz", GRR, "dB", 0.15, -0.15);
            fail++;
        }
        else
        {
            SHOW_RESULT("GRR_Hz", GRR, "dB", 0.15, -0.15);
        }

    }
    if (fail != 0) BIN(12); //分箱



    //GRRL
    SETUP();
    // 电平列表 (dBm0)
    double level_dbm0[] = { -40.0, 3.0 };
    // 对应的 START_INDEX 序号
    int index[] = { 9, 11 };
    int num = 2;//选测2个点

    RUN_PATTERN(10, 0, 0, 0);               // 运行 0 dBm0 码流段
    SET_AVM_PATH(LP20, BPPASS);            // 20kHz 低通滤波
    double Vref = AVM_MEASURE(1, 4.0, V, 10);
    // 参考绝对增益（可不计算，直接记录 Vref 用于相对比较）
    double Gref = 20.0 * log10(Vref / 1.2276);   // 1.2276 是 0 dBm0 理想电压

    int fail_grrl = 0;
    for (int i = 0; i < num; i++)
    {
        int idx = index[i];
        double L = level_dbm0[i];
        RUN_PATTERN(idx, 0, 0, 0);
        double vout_grrl = AVM_MEASURE(1, 4.0, V, 10);
        double Gabs = 20.0 * log10(vout_grrl / 1.2276);
        double delta = Gabs - Gref;          // 增益变化

        // 判断限值：若电平在 -40 ~ +3 dBm0 范围内，允许 ±0.2 dB
        if (delta < -0.2 || delta > 0.2) {
            SHOW_RESULT("GRRL", delta, "dB", 0.2, -0.2);
            fail_grrl++;
        }
        else {
            SHOW_RESULT("GRRL", delta, "dB", 0.2, -0.2);
        }
    }
    if (fail_grrl != 0) BIN(13);





    // ========== SFDX 测试（发送单频失真） ==========

    // 1. 初始化环境（确保芯片上电、时钟等）
    SETUP();                     // 设置电源、时钟、FORMAT
    SET_AS(1.2276, V, 1020, HZ); // 输入 1020 Hz, 0 dBm0
    Delay(20);                   // 等待稳定

    // 2. 运行图形7（预期 DX 全 L，失效继续）
    RUN_PATTERN(7, 0, 0, 0);

    // 3. 捕获多个 PCM 字节
    unsigned char pcm_data_sfdx[SFDX_FFT_POINTS];
    int captured_sfdx = capture_multi_pcm_bytes(7, 46, 4, pcm_data_sfdx, SFDX_FFT_POINTS);
    if (captured_sfdx != SFDX_FFT_POINTS) {
        BIN(14);   // 捕获失败分箱
        goto END_SFDX;
    }

    // 4. 解码为电压序列（有效值，但 FFT 需要瞬时值，这里直接使用峰值电压）
    double voltage_sfdx[SFDX_FFT_POINTS];
    for (int i = 0; i < SFDX_FFT_POINTS; i++) {
        double vrms = pcm_to_voltage_rms(pcm_data_sfdx[i]);
        // 为了 FFT，转换为峰值电压（正弦波峰值 = RMS * sqrt(2)）
        voltage_sfdx[i] = vrms * sqrt(2.0);
    }

    // 5. 加窗
    apply_hanning(voltage_sfdx, SFDX_FFT_POINTS);

    // 6. FFT
    double fft_mag_sfdx[SFDX_FFT_POINTS / 2];
    if (!DFT(voltage_sfdx, SFDX_FFT_POINTS, fft_mag_sfdx)) {
        BIN(15);   // FFT 失败
        goto END_SFDX;
    }

    // 7. 计算频率分辨率，找到基波和谐波 bin
    double freq_res_sfdx = SFDX_SAMPLE_RATE / SFDX_FFT_POINTS;
    int bin_fund_sfdx = (int)(1020.0 / freq_res_sfdx + 0.5);
    int bin_2nd_sfdx = (int)(2 * 1020.0 / freq_res_sfdx + 0.5);
    int bin_3rd_sfdx = (int)(3 * 1020.0 / freq_res_sfdx + 0.5);

    if (bin_2nd_sfdx >= SFDX_FFT_POINTS / 2 || bin_3rd_sfdx >= SFDX_FFT_POINTS / 2) {
        BIN(16);   // 频率超出范围
        goto END_SFDX;
    }

    double A1_sfdx = fft_mag_sfdx[bin_fund_sfdx];
    double A2_sfdx = fft_mag_sfdx[bin_2nd_sfdx];
    double A3_sfdx = fft_mag_sfdx[bin_3rd_sfdx];
    double Ah_sfdx = (A2_sfdx > A3_sfdx) ? A2_sfdx : A3_sfdx;
    double SFDX_db = 20.0 * log10(Ah_sfdx / A1_sfdx);

    // 8. 判断是否合格（MAX -46 dB）
    if (SFDX_db > -46) {
        SHOW_RESULT("SFDX", SFDX_db, "dB", -46, No_LoLimit);
        BIN(17);   // 失真超限
    }
    else
    {
        SHOW_RESULT("SFDX", SFDX_db, "dB", -46, No_LoLimit);
    }

END_SFDX:







    //SFDR
        // ========== SFD_R 测试（接收单频失真） ==========

    // 1. 初始化环境（确保芯片上电，接收时钟正常）
    SETUP();                     // 设置电源、时钟、FORMAT
    // 注意: 接收测试不需要 SET_AS，激励来自图形10的 PCM 码流

    // 2. 运行接收图形10（1020Hz 0dBm0 PCM 码流）
    if (!RUN_PATTERN(10, 0, 0, 0)) {
        BIN(18);   // 图形运行失败
        goto END_SFDR;
    }
    Delay(10);                   // 等待信号稳定

    // 5. 用 DVM 连续采样 VFR0（通道1）
    double voltage_sfdr[SFDR_FFT_POINTS];
    // 采样率 = 50MHz / freq_div，计算分频系数
    int freq_div = (int)(50e6 / SFDR_DVM_SR);
    // 参数：通道1, 量程2V, 延迟10ms, 采样点数, 分频系数, 数据数组
    MAT_DVM_MEASURE(1, 2.0, V, 10, SFDR_FFT_POINTS, freq_div, voltage_sfdr);


    // 4. 加汉宁窗
    for (int i = 0; i < SFDR_FFT_POINTS; i++) {
        double w = 0.5 * (1 - cos(2 * M_PI * i / (SFDR_FFT_POINTS - 1)));
        voltage_sfdr[i] *= w;
    }

    // 5. FFT 变换
    double fft_mag_sfdr[SFDR_FFT_POINTS / 2];
    if (!DFT(voltage_sfdr, SFDR_FFT_POINTS, fft_mag_sfdr)) {
        BIN(19);   // FFT 失败
        goto END_SFDR;
    }

    // 6. 查找基波和谐波幅度
    int bin_fund_sfdr = (int)(1020 / SFDR_FREQ_RES + 0.5);
    int bin_2nd_sfdr = (int)(2 * 1020 / SFDR_FREQ_RES + 0.5);
    int bin_3rd_sfdr = (int)(3 * 1020 / SFDR_FREQ_RES + 0.5);

    if (bin_2nd_sfdr >= SFDR_FFT_POINTS / 2 || bin_3rd_sfdr >= SFDR_FFT_POINTS / 2) {
        BIN(20);   // 谐波超出奈奎斯特频率
        goto END_SFDR;
    }

    double A1_sfdr = fft_mag_sfdr[bin_fund_sfdr];
    double A2_sfdr = fft_mag_sfdr[bin_2nd_sfdr];
    double A3_sfdr = fft_mag_sfdr[bin_3rd_sfdr];
    double Ah_sfdr = (A2_sfdr > A3_sfdr) ? A2_sfdr : A3_sfdr;
    double SFDR_db = 20.0 * log10(Ah_sfdr / A1_sfdr);

    // 7. 判断合格性（要求 SFDR_db 优于 -46dB,>-46为不合格）
    if (SFDR_db > -46)
    {
        SHOW_RESULT("SFDR", SFDR_db, "dB", -46, No_LoLimit);
        BIN(21);   // 失真超限
    }
    else
    {
        SHOW_RESULT("SFDR", SFDR_db, "dB", -46, No_LoLimit);
    }

END_SFDR:




    //IDM

    // ---------- 生成并播放双音波形 ---------
    WORD as_waveform[IMD_AS_POINTS];
    for (int i = 0; i < IMD_AS_POINTS; i++) {
        double t = i / IMD_AS_SAMPLE_RATE;
        double v = IMD_V_SINGLE_PEAK * (sin(2 * M_PI * IMD_F1 * t) + sin(2 * M_PI * IMD_F2 * t));
        if (v > 10) v = 10;
        if (v < -10) v = -10;
        as_waveform[i] = (WORD)(((v / 10) + 1.0) / 2.0 * 65535.0 + 0.5);
    }
    LOAD_AS_PATTERN(7, 1, as_waveform);
    RUN_AS_PATTERN(7, 1, IMD_AS_FREQ_DIV, 10);

    // ----------运行图形7（预期 DX 全 L，失效继续）----------
    RUN_PATTERN(7, 0, 0, 0);

    // ---------- 从失效存储器捕获 PCM 字节 ----------

    unsigned char pcm_data_idm[IMD_PCM_BYTES];
    int captured_idm = capture_multi_pcm_bytes(7, 46, 4, pcm_data_idm, IMD_PCM_BYTES);
    if (captured_idm != IMD_PCM_BYTES) {
        BIN(22);   // 捕获失败分箱
        goto END_IMD;
    }


    // ---------- 6. 解码为电压序列（使用现有 pcm_to_voltage_rms 并转成峰值） ----------
    double voltage_imd[IMD_PCM_BYTES];
    for (int i = 0; i < IMD_PCM_BYTES; i++) {
        double vrms = pcm_to_voltage_rms(pcm_data_idm[i]);
        voltage_imd[i] = vrms * sqrt(2.0);   // 转换为峰值电压
    }

    // ---------- 7. 加窗（调用现有 apply_hanning） ----------
    apply_hanning(voltage_imd, IMD_PCM_BYTES);

    // ---------- 8. FFT ----------
    double fft_mag_idm[IMD_FFT_POINTS / 2];
    if (!DFT(voltage_imd, IMD_FFT_POINTS, fft_mag_idm)) {
        BIN(23);
        goto END_IMD;
    }

    // ---------- 9. 查找基波和互调产物 ----------
    double freq_res_idm = IMD_FS_PCM / IMD_FFT_POINTS;
    int bin_f1 = (int)(IMD_F1 / freq_res_idm + 0.5);
    int bin_f2 = (int)(IMD_F2 / freq_res_idm + 0.5);
    int bin_sum = (int)((IMD_F1 + IMD_F2) / freq_res_idm + 0.5);
    int bin_diff = (int)((IMD_F2 - IMD_F1) / freq_res_idm + 0.5);

    if (bin_f1 >= IMD_FFT_POINTS / 2 || bin_f2 >= IMD_FFT_POINTS / 2 ||
        bin_sum >= IMD_FFT_POINTS / 2 || bin_diff >= IMD_FFT_POINTS / 2) {
        BIN(24);
        goto END_IMD;
    }

    double A_f1 = fft_mag_idm[bin_f1];
    double A_f2 = fft_mag_idm[bin_f2];
    double A_sum = fft_mag_idm[bin_sum];
    double A_diff = fft_mag_idm[bin_diff];
    double A_imd = (A_sum > A_diff) ? A_sum : A_diff;
    double A_fund = (A_f1 > A_f2) ? A_f1 : A_f2;
    double IMD_db = 20.0 * log10(A_imd / A_fund);

    // ---------- 10. 判断 ----------
    if (IMD_db > -41.0) {//超限
        SHOW_RESULT("IMD", IMD_db, "dB", -41.0, No_LoLimit);
        BIN(25);
    }
    else {
        SHOW_RESULT("IMD", IMD_db, "dB", -41.0, No_LoLimit);
    }


END_IMD:

END_TEST://contest失败后跳转到此


    DPS_OFF(DPS1);
    DPS_OFF(DPS2);




}


unsigned char capture_one_pcm_byte(int start_idx, int dx_channel, int offset)
{
    BYTE* fail_bits = NULL;
    int depth = 0;
    fail_bits = GET_PATTERN_RESULT(start_idx, dx_channel, &depth);
    if (fail_bits == NULL || depth < offset + 9) {
        return 0xFF;
    }
    unsigned char pcm_byte = 0;
    for (int bit = 0; bit < 8; bit++) {
        int cycle = offset + 1 + bit;   // 数据位从 offset+1 开始
        int byte_ofs = cycle / 8;
        int bit_ofs = cycle % 8;
        int level = (fail_bits[byte_ofs] >> bit_ofs) & 1;
        pcm_byte |= (level << (7 - bit));
    }
    return pcm_byte;
}


double pcm_to_voltage_rms(unsigned char tp3057_byte)
{
    // 1. 偶数位反转，恢复标准 A-law
    unsigned char alaw_std = tp3057_byte ^ 0x55;

    // 2. A-law 解码（标准 G.711 算法）
    short linear = 0;
    unsigned char sign = alaw_std & 0x80;
    unsigned char magnitude = alaw_std & 0x7F;
    unsigned char segment = (magnitude >> 4) & 0x07;
    unsigned char step = magnitude & 0x0F;
    if (segment == 0) {
        linear = (step << 4) + 8;
    }
    else {
        linear = ((0x10 | step) << (segment + 2)) + 8;
    }
    if (sign) linear = -linear;

    // 3. 转换为峰值电压
    double vpeak = (double)linear * (VPEAK_FS / LINEAR_MAX);
    // 4. 转换为有效值（正弦波）
    return vpeak / sqrt(2.0);
}



// 捕获多个 PCM 字节（从图形 start_idx 中，从 offset 周期开始提取）
// 返回实际捕获的字节数，数据存入 pcm_bytes 数组（至少 num_bytes 长度）
int capture_multi_pcm_bytes(int start_idx, int dx_channel, int offset,
    unsigned char* pcm_bytes, int num_bytes)
{
    BYTE* fail_bits = NULL;
    int depth = 0;
    fail_bits = GET_PATTERN_RESULT(start_idx, dx_channel, &depth);
    if (fail_bits == NULL || depth < offset + num_bytes * 9) {
        return 0;   // 数据不足
    }
    for (int byte = 0; byte < num_bytes; byte++) {
        unsigned char pcm_byte = 0;
        for (int bit = 0; bit < 8; bit++) {
            int cycle = offset + 1 + byte * 256 + bit;   // 第五字节为fsx置1，offset为4，每个字节占用256个周期，fsx-8khz
            int byte_ofs = cycle / 8;
            int bit_ofs = cycle % 8;
            int level = (fail_bits[byte_ofs] >> bit_ofs) & 1;
            pcm_byte |= (level << (7 - bit));
        }
        pcm_bytes[byte] = pcm_byte;
    }
    return num_bytes;
}


void apply_hanning(double* data, int n) //加汉宁窗
{
    for (int i = 0; i < n; i++) {
        double w = 0.5 * (1 - cos(2 * M_PI * i / (n - 1)));
        data[i] *= w;
    }
}