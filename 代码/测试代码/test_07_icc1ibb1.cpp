/* ============================================================
   test_07_icc1ibb1.cpp — 工作电流测试 (ICC1 / IBB1)
   编译：shared.cpp + 本文件 加入工程
   语言标准：C95

   测试内容：图形6 使芯片进入正常工作状态，DPS 回读 VCC 和 VBB 电流
   限值：ICC1 <= 9mA, IBB1 <= 9mA
   ============================================================ */


   /* ============================================================
   shared.h — TP3057 测试程序公共头文件
   所有拆分后的测试程序均 #include "shared.h"
   编译时需将 shared.cpp 一同加入工程

   语言标准：C95 (ISO/IEC 9899:1995)
   ============================================================ */

#include "stdafx.h"
#include "testdef.h"
#include "data.h"
#include "math.h"

/* ==================== 物理常量 ==================== */
#define VPEAK_FS              2.492       /* TP3057 A-law 满量程峰值电压 (V) */
#define LINEAR_MAX            4032        /* G.711 A-law 13-bit 线性编码最大值, = (2*15+33)*2^6 */
#define M_PI                  3.14159265358979323846

/* ==================== SFDX 参数 ==================== */
#define SFDX_FFT_POINTS       512         /* FFT 点数（= 捕获的 PCM 字节数） */
#define SFDX_SAMPLE_RATE      8000.0      /* PCM 采样率 (Hz) */

/* ==================== SFDR 参数 ==================== */
#define SFDR_FFT_POINTS       2048        /* FFT 点数（2的幂） */
#define SFDR_DVM_SR           100000.0    /* DVM 采样率 (Hz) */
#define SFDR_FREQ_RES         (SFDR_DVM_SR / SFDR_FFT_POINTS)

/* ==================== IMD 参数 (Loop Around Measurement) ==================== */
/*更改频率为390.625Hz和3320.3125Hz，保证频点对齐FFT bin，避免频谱泄漏
#define IMD_F1                390.625
#define IMD_F2                3320.3125
#define IMD_FREQ_DIV          500        
*/

#define IMD_F1                300.0
#define IMD_F2                3400.0
#define IMD_TEST_LEVEL_DBM   (-10.0)
#define IMD_V_TOTAL_RMS       (1.2276 * pow(10.0, IMD_TEST_LEVEL_DBM / 20.0))
#define IMD_V_SINGLE_PEAK     IMD_V_TOTAL_RMS
#define IMD_AS_POINTS         1024
#define IMD_AS_SAMPLE_RATE    100000.0
#define IMD_AS_FREQ_DIV       ((int)(100e6 / IMD_AS_SAMPLE_RATE))  /* 100MHz 主频, =1000 */
#define IMD_FFT_POINTS        2048        /* FFT 点数（2的幂），DVM 采样点数 */
#define IMD_FREQ_DIV          488         /* 50MHz/488≈102.5kHz, Δf≈50Hz, 4频点对齐bin */
#define IMD_NAVG              4           /* 测量平均次数 */

/* ==================== 全局变量（跨测试共享） ==================== */
extern double gxa1020;   /* GXA 测得的 1020Hz 参考增益，供 GXR 使用 */
extern double gra1020;   /* GRA 测得的 1020Hz 参考增益，供 GRR/GRRL 使用 */

/* ==================== 公共函数声明 ==================== */

/*
 * 系统初始化：电源、时钟、时序、FORMAT、上电
 */
void SETUP(void);

/*
 * 单字节 PCM 捕获（从 fail memory 中提取一个 A-law 字节）
 * start_idx: 图形索引, dx_channel: DX 通道号, offset: 第一个 FSX 脉冲所在周期
 * 返回: 8-bit A-law PCM 码字; 0xFF 表示捕获失败
 */
int capture_one_pcm_byte(int start_idx, int dx_channel, int offset);

/*
 * 多字节 PCM 捕获（连续提取 num_bytes 个 A-law 字节，每字节间隔 256 周期）
 * 返回: 实际捕获的字节数
 */
int capture_multi_pcm_bytes_fsx(int start_idx, int dx_channel, int fsx_channel, int offset,
                                unsigned char* pcm_bytes, int num_bytes);

/*
 * A-law 解码 -> RMS 电压
 * tp3057_byte: 带偶数位反转的 A-law 码字
 * 返回: 有效值电压 (Vrms)
 */
double pcm_to_voltage(unsigned char tp3057_byte);

/*
 * PCM 字节数组 -> 真 RMS 电压
 * pcm_bytes: PCM 字节数组, count: 字节数
 * 返回: 均方根电压 (Vrms)
 */
double compute_vrms(unsigned char* pcm_bytes, int count);

/*
 * 汉宁窗
 */
void apply_hanning(double* data, int n);

/*
 * 频率 -> 最近的 FFT bin 索引
 * freq: 目标频率 (Hz), freq_res: FFT 频率分辨率 (Hz), max_bin: 最大 bin 号
 * 返回: 四舍五入后的 bin 索引，超出范围返回 -1
 */
int freq_to_bin(double freq, double freq_res, int max_bin);

/*
 * 计算失真 dB（谐波幅度 / 基波幅度）
 * A_fund:  基波幅度
 * A_harm1: 第一个谐波幅度
 * A_harm2: 第二个谐波幅度
 * 返回: 20*log10(max(A_harm1, A_harm2) / A_fund)
 */
double calc_distortion_db(double A_fund, double A_harm1, double A_harm2);

 

/* ============================================================
   shared.cpp — TP3057 测试程序公共函数实现
   编译时与测试程序 .cpp 一同加入工程

   语言标准：C95 (ISO/IEC 9899:1995)
   ============================================================ */



/* 全局变量定义 */
double gxa1020 = 0.0;
double gra1020 = 0.0;

/* ==================== 系统初始化 ==================== */
void SETUP(void)
{
    SET_DPS(DPS1, -5.0, V, 100, MA);
    SET_DPS(DPS2, 5.0, V, 100, MA);
    Delay(10);
    SET_PERIOD(488);//周期设置 2.048MHz
    SET_TIMING(100, 350, 450);//LeadEdge比较沿(100ns)Ctg锁存沿(350ns)EndEdge终止沿(450ns)
    SET_INPUT_LEVEL(2.2, 0.6);
    SET_OUTPUT_LEVEL(2.4, 0.4);
    FORMAT(NRZ0, "48,4,47,46,1,2");   /* FSR-1;DR-2;BCLKR-3;PDN-4;BCLKX-44;MCLKX-45;DX-46;FSX-47;TSX-48 */
    FORMAT(RZ, "45,44,3");
    RUN_PATTERN(3, 1, 0, 0);          /* 图形3: 上电，取消 DX 高阻态 */
    Delay(10);
}

/* ==================== 单字节 PCM 捕获 ==================== */
int capture_one_pcm_byte(int start_idx, int dx_channel, int offset) /* 返回:>=0成功(PCM字节值0~255), -1失败 */
{
    BYTE* fail_bits; //指向失效位数据的指针 fail memory 是一个 bit 数组，第 N 个 bit 记录了第 N 个测试周期的比较结果
    int depth;      //获取fail memory的深度，单位为byte 
    unsigned char pcm_byte;
    int bit;
    int cycle, byte_ofs, bit_ofs, level;

    fail_bits = NULL; 
    depth = 0;
    fail_bits = GET_PATTERN_RESULT(start_idx, dx_channel, &depth);

    if (fail_bits == NULL || depth * 8 < offset + 9) {//有效性检查 depth 是以 byte 为单位的，乘以 8 转换为 bit；offset+9 需要跳过前offset个周期，再加上1个周期的起始位和8个周期的数据位
        return -1;
    }
    //按位提取 PCM 数据
    pcm_byte = 0;
    for (bit = 0; bit < 8; bit++) {
        cycle    = offset + 1 + bit;       //图形文件中的周期号  数据位从 offset+1 开始  跳过前4+1个周期
        byte_ofs = cycle / 8;              //确定该周期在fail_bits数组中的第几个字节byte
        bit_ofs  = cycle % 8;              //确定该周期在该字节中的第几个 bit
        level    = (fail_bits[byte_ofs] >> bit_ofs) & 1;  //该周期实际输出的电平  0 或 1 
        // fail_bits[byte_ofs] >> bit_ofs ：将该字节右移 bit_ofs 位，使得目标 bit 移动到最低位；& 1：取最低位的值，即目标 bit 的值
        pcm_byte |= (unsigned char)(level << (7 - bit));  //将该 bit 放到 pcm_byte 的正确位置上  bit 0 是最高位，所以左移 (7 - bit) tp3057符合msb-first
    }
    return pcm_byte;
}

/* ==================== 多字节 PCM 捕获（旧版：基于固定offset，已弃用保留） ==================== */
int capture_multi_pcm_bytes_legacy(int start_idx, int dx_channel, int offset,  //输入参数：图形编号（需与RUN_PATTERN中一致）、DX通道号、数据位起始周期偏移、
                             unsigned char* pcm_bytes, int num_bytes)   //pcm_bytes 输出缓冲区指针、num_bytes 要捕获的字节数
{
    BYTE* fail_bits;//指向失效位数据的指针 fail memory 是一个 bit 数组，第 N 个 bit 记录了第 N 个测试周期的比较结果
    int depth;      //获取fail memory的深度，单位为byte 
    int byte_idx, bit;
    int cycle, byte_ofs, bit_ofs, level;
    unsigned char pcm_byte;

    fail_bits = NULL;
    depth = 0;
    fail_bits = GET_PATTERN_RESULT(start_idx, dx_channel, &depth);

    if (fail_bits == NULL || depth * 8 < offset + (num_bytes - 1) * 256) {//有效性检查 depth 是以 byte 为单位的，乘以 8 转换为bit ；num_bytes * 256 需要跳过前offset个周期，再加上每条pcm码需要256周期(1起始位+8数据位+247空闲周期)
        return 0;
    }
    for (byte_idx = 0; byte_idx < num_bytes; byte_idx++) {  /* byte_idx 要捕获的第几个pcm字节，从0开始 */
        pcm_byte = 0;
        for (bit = 0; bit < 8; bit++) {
            cycle    = offset + 1 + byte_idx * 256 + bit;  /* 每字节 256 周期 */
            byte_ofs = cycle / 8;
            bit_ofs  = cycle % 8;
            level    = (fail_bits[byte_ofs] >> bit_ofs) & 1;
            pcm_byte |= (unsigned char)(level << (7 - bit));
        }
        pcm_bytes[byte_idx] = pcm_byte; //将拼装好的一个pcm字节存入输出缓冲区pcm_bytes[byte_idx]
    }
    return num_bytes;
}
/* ==================== 多字节 PCM 捕获（新版：基于FSX通道动态检测） ==================== */
/*
 * 工作原理：
 *   1. 读取 fsx_channel 的失效存储器——图形中该通道恒为 0，
 *      当 FSX 输出 1 时，比较不匹配，失效位 = 1。
 *   2. 扫描 FSX 失效存储器，找到 FSX=1 的周期。
 *   3. FSX=1 之后的 8 个周期即为 DX 输出的 PCM 数据位 (MSB-first)。
 *   4. 从 DX 通道失效存储器中提取这 8 位，组装为一个 PCM 字节存入 pcm_bytes。
 *
 *   offset 参数保留用于兼容，表示从第几个周期开始搜索 FSX 脉冲。
 */
int capture_multi_pcm_bytes_fsx(int start_idx, int dx_channel, int fsx_channel, int offset,
                                unsigned char* pcm_bytes, int num_bytes)
{
    BYTE* fsx_fail_bits;   /* FSX 监测通道的失效存储器指针 */
    BYTE* dx_fail_bits;    /* DX 数据通道的失效存储器指针 */
    int fsx_depth, dx_depth;
    int byte_idx, bit;
    int cycle, byte_ofs, bit_ofs;
    int search_start;
    int dx_cycle, dx_byte_ofs, dx_bit_ofs, dx_level;
    unsigned char pcm_byte;

    /* 获取 FSX 监测通道的失效存储器 */
    fsx_fail_bits = NULL;
    fsx_depth = 0;
    fsx_fail_bits = GET_PATTERN_RESULT(start_idx, fsx_channel, &fsx_depth);

    /* 获取 DX 数据通道的失效存储器 */
    dx_fail_bits = NULL;
    dx_depth = 0;
    dx_fail_bits = GET_PATTERN_RESULT(start_idx, dx_channel, &dx_depth);

    if (fsx_fail_bits == NULL || dx_fail_bits == NULL) {
        return 0;
    }

    /* 从 offset 指定的周期开始搜索 FSX 脉冲 */
    search_start = offset;
    byte_idx = 0;

    for (cycle = search_start; cycle < fsx_depth * 8 && byte_idx < num_bytes; cycle++) {
        byte_ofs = cycle / 8;
        bit_ofs = cycle % 8;

        /* FSX=1 → 失效位=1（图形期望 0，实际输出 1，不匹配） */
        if ((fsx_fail_bits[byte_ofs] >> bit_ofs) & 1) {

            /* 确保 DX 失效存储器有足够深度容纳 8 个数据位 */
            if (cycle + 9 > dx_depth * 8) {
                break;
            }

            /* 提取 FSX 脉冲之后 8 个周期的 DX 数据位 */
            pcm_byte = 0;
            for (bit = 0; bit < 8; bit++) {
                dx_cycle    = cycle + 1 + bit;           /* FSX 之后的第 1~8 个周期 */
                dx_byte_ofs = dx_cycle / 8;
                dx_bit_ofs  = dx_cycle % 8;
                dx_level    = (dx_fail_bits[dx_byte_ofs] >> dx_bit_ofs) & 1;
                pcm_byte |= (unsigned char)(dx_level << (7 - bit));  /* MSB-first */
            }
            pcm_bytes[byte_idx] = pcm_byte;
            byte_idx++;
        }
    }

    return byte_idx;  /* 返回实际捕获的 PCM 字节数 */
}



/* ==================== A-law 解码 -> 单个电压 ==================== */
double pcm_to_voltage(unsigned char tp3057_byte)//只是瞬时电压，无关rms和pk
{
    unsigned char alaw_std;
    short linear;
    unsigned char sign, magnitude, segment, step;
    double vpeak;

    /* 1. 偶数位反转，恢复标准 A-law */
    alaw_std = tp3057_byte ^ 0x55;

    /* 2. G.711 A-law 解码 */
    linear = 0;                           //线性 PCM 值，范围约 -4032 ~ +4032
    sign      = alaw_std & 0x80;          //提取bit7 符号位 0x80 = 1000 0000 bit7=1表示正电压，bit7=0表示负电压
    magnitude = alaw_std & 0x7F;          //提取bit6~0 幅度位 0x7F = 0111 1111 bit7清零 幅度的绝对值信息
    segment   = (magnitude >> 4) & 0x07;  //提取bit6~4 段号 0~7  将bit 6~4 右移4位到bit 2~0，得到段号；& 0x07 是为了确保只保留3位段号
    step      = magnitude & 0x0F;         //提取bit3~0 步进号 0~15 0x0F = 0000 1111——只保留低 4 位（bit 3~0）
 
    if (segment == 0) {  //段落0：线性增量为 1，范围 -15 ~ +15
        linear = (short)(2 * step + 1);                     /* 范围: 1 ~ 31 */
    } else {
        linear = (short)((2 * step + 33) << (segment - 1)); /* 范围: 33 ~ 4032 ; x << n 将 x 的二进制位全部向左移动 n 位，右侧空位补 0。效果等同于 x × 2^n*/
    }
    if (!sign) linear = (short)(-linear);                   /* 符号处理 MSB=0 → 负电压 (手册 p.11) */

    /* 3. 线性值 -> 峰值电压 */
    vpeak = (double)linear * (VPEAK_FS / LINEAR_MAX);//只是瞬时电压，无关rms和pk
    return vpeak ;//只是瞬时电压，无关rms和pk
}

/* ==================== PCM 字节数组 -> 真 RMS 电压 ==================== */
double compute_vrms(unsigned char* pcm_bytes, int count)//pcm_bytes: PCM 字节数组, count: 字节数  返回: 均方根电压 (Vrms)
{
    int i;
    double sum_sq, v_inst;

    sum_sq = 0.0;
    for (i = 0; i < count; i++) {
        v_inst = pcm_to_voltage(pcm_bytes[i]);
        sum_sq += v_inst * v_inst;
    }
    return sqrt(sum_sq / count);
}


/* ==================== 汉宁窗 ==================== */
void apply_hanning(double* data, int n)
{
    int i;
    double w;
    for (i = 0; i < n; i++) {
        w = 0.5 * (1.0 - cos(2.0 * M_PI * (double)i / (double)(n - 1)));
        data[i] *= w;
    }
}

/* ==================== 辅助函数 ==================== */

/*
 * 频率 -> 最近的 FFT bin 索引
 */
int freq_to_bin(double freq, double freq_res, int max_bin)
{
    int bin;
    bin = (int)(freq / freq_res + 0.5);
    if (bin < max_bin)
        return bin;
    else
        return -1;
}

/*
 * 计算失真 dB
 */
double calc_distortion_db(double A_fund, double A_harm1, double A_harm2)
{
    double A_max_harm;
    if (A_fund <= 0.0) return 1;  //基波幅度为零或负数，返回1，表示失真无法计算
    A_max_harm = (A_harm1 > A_harm2) ? A_harm1 : A_harm2;
    return 20.0 * log10(A_max_harm / A_fund);
}















#define TEST_ENABLE  1

void PASCAL TP3057()
{
#if TEST_ENABLE
    int fail;
    int icc1_fail = 0;
    int ibb1_fail = 0;

    SETUP();

    /* 确保电源电压正确 */
    SET_DPS(1, -5, V, 100, MA);
    SET_DPS(2, 5, V, 100, MA);

    /* 运行图形6：芯片进入工作状态 */
    RUN_PATTERN(6, 1, 0, 0);
    Delay(50);

    fail = 0;

    if (!DPS_MEASURE(DPS1, R20MA, 5, "ICC1", MA, 9, No_LoLimit)) {
        fail++;
        icc1_fail = 1;
    }
    if (!DPS_MEASURE(DPS2, R20MA, 5, "IBB1", MA, 9, No_LoLimit)) {
        fail++;
        ibb1_fail = 1;
    }

    if (fail != 0){
        if (icc1_fail){
            SHOW_RESULT("ICC1=>FAIL", 0, "", 1, 0);
            BIN(7);
        }

        if (ibb1_fail){
            SHOW_RESULT("IBB1=>FAIL", 0, "", 1, 0);
            BIN(8);
        }
    }

    else
        SHOW_RESULT("ICC1IBB1=>PASS", 1, "", 1, 1);

    DPS_OFF(DPS1);
    DPS_OFF(DPS2);
#endif
}
