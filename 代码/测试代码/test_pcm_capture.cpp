/* ============================================================
   test_pcm_capture.cpp — PCM 读取验证程序
   编译：shared.cpp + 本文件 加入工程
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



/* ============================================================
   shared.h — TP3057 测试程序公共头文件
   所有拆分后的测试程序均 #include "shared.h"
   编译时需将 shared.cpp 一同加入工程

   语言标准：C95 (ISO/IEC 9899:1995)
   ============================================================ */

/* ==============================================================
    26.7.1更新
    1、停用capture_multi_pcm_bytes，改名为capture_multi_pcm_bytes_legacy
    2、新增capture_multi_pcm_bytes_fsx，基于FSX通道动态检测PCM码流   读fsx脚：ch43
    3、新增2.5 调试：FSX/DX 通道原始值，将输出前DBG_BYTE_COUNT个字节的fsx和dx读数
    4、更改setup中DPS1/DPS2电源极性，DPS1负5V，DPS2正5V
   ==========================================================*/


#include "StdAfx.h"
#include "testdef.h"
#include "data.h"
#include "math.h"

/* ==================== 物理常量 ==================== */
#define VPEAK_FS              2.492       /* TP3057 A-law 满量程峰值电压 (V) */
#define LINEAR_MAX            4032        /* G.711 A-law 13-bit 线性编码最大值, = (2*15+33)*2^6 */
#define M_PI                  3.14159265358979323846
/* FSX 监测通道号现作为函数参数 fsx_channel 传入，不再使用宏定义 */

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
 * 多字节 PCM 捕获（基于FSX通道动态检测，连续提取 num_bytes 个 A-law 字节）
 * fsx_channel: FSX 监测通道号（图形中该通道恒为0，FSX=1时失效位=1）
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

    if (fail_bits == NULL || depth * 8 < offset + (num_bytes - 1) * 10) {//有效性检查 depth 是以 byte 为单位的，乘以 8 转换为bit ；num_bytes * 256 需要跳过前offset个周期，再加上每条pcm码需要256周期(1起始位+8数据位+247空闲周期)
        return 0;
    }
    for (byte_idx = 0; byte_idx < num_bytes; byte_idx++) {  /* byte_idx 要捕获的第几个pcm字节，从0开始 */
        pcm_byte = 0;
        for (bit = 0; bit < 8; bit++) {
            cycle    = offset + 1 + byte_idx * 10 + bit;  /* 每字节 256 周期 */
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
    fsx_fail_bits = NULL;//GET_PATTERN_RESULT返回的BYTE数组
    fsx_depth = 0;//GET_PATTERN_RESULT返回的BYTE数组的深度，单位为byte
    fsx_fail_bits = GET_PATTERN_RESULT(start_idx, fsx_channel, &fsx_depth);

    /* 获取 DX 数据通道的失效存储器 */
    dx_fail_bits = NULL;
    dx_depth = 0;
    dx_fail_bits = GET_PATTERN_RESULT(start_idx, dx_channel, &dx_depth);

    if (fsx_fail_bits == NULL || dx_fail_bits == NULL) {
        return 0;
    }

    /* 从 offset 指定的周期开始搜索 FSX 脉冲 */
    search_start = offset;//搜索起始位，由offset参数指定，通常为0，表示从第0个周期开始搜索FSX脉冲
    byte_idx = 0;//已捕获的字节计数，也是写入 pcm_bytes[] 的数组下标。从 0 开始，每捕获一个 PCM 字节就 +1

    for (cycle = search_start; cycle < fsx_depth * 8 && byte_idx < num_bytes; cycle++) {
        byte_ofs = cycle / 8;//cycle 在 FSX fail_bits 数组中的字节索引，即第几个 BYTE
        bit_ofs = cycle % 8;//cycle 在该字节中的 bit 索引，即第几个 bit

        /* FSX=1 → 失效位=1（图形期望 0，实际输出 1，不匹配） */
        if ((fsx_fail_bits[byte_ofs] >> bit_ofs) & 1) {//若 FSX 失效位为 1，开始捕获 DX 数据

            /* 确保 DX 失效存储器有足够深度容纳 8 个数据位 */
            if (cycle + 9 > dx_depth * 8) {
                break;
            }

            /* 提取 FSX 脉冲之后 8 个周期的 DX 数据位 */
            pcm_byte = 0;//正在组装的单个 PCM 字节
            for (bit = 0; bit < 8; bit++) {
                dx_cycle    = cycle + 1 + bit;           /* FSX 之后的第 1~8 个周期 */
                dx_byte_ofs = dx_cycle / 8;//dx_cycle 在 DX fail_bits 数组中的字节索引
                dx_bit_ofs  = dx_cycle % 8;//dx_cycle 在 DX fail_bits 字节中的位索引
                dx_level    = (dx_fail_bits[dx_byte_ofs] >> dx_bit_ofs) & 1;//从 DX fail_bits 中提取出的该周期实际输出电平（0 或 1）
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
        v_inst = pcm_to_voltage(pcm_bytes[i]);//当前 PCM 字节对应的瞬时电压
        sum_sq += v_inst * v_inst;//电压平方累加和,每轮循环把当前字节解码出的瞬时电压平方后累加进去。
    }
    return sqrt(sum_sq / count);//均方根电压Vrms = sqrt(平均值)  平均值 = sum_sq / count
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
#define USE_AS_SOURCE        0

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
