/* ============================================================
   test_all.cpp — TP3057 全参数集成测试程序
   编译：shared.cpp + 本文件 加入工程
   语言标准：C95 (ISO/IEC 9899:1995)

   测试流程（共14项参数）：
     CON-TEST → FUNC-TEST → IIL → IIH → IOZ
     → ICC0IBB0 → ICC1IBB1 → GXA → GXR → GRA
     → GRR → GRRL → SFDX → SFDR → IMD

   各测试项采用 { } 块作用域，变量互不冲突。
   与原 test_01~test_12 中各独立 TP3057() 函数等价整合。

   BIN 编号说明（已消除原独立文件中的重复编号）：
     原 test_08 GXA_CAP_FAIL  BIN(7) → 27
     原 test_08 GXA           BIN(8) → 28
     原 test_11 SFDR_PAT_FAIL BIN(18)→ 31

   测试开关：修改下方对应宏为 0 可跳过对应测试项
   ============================================================ */

#include "StdAfx.h"

   /* ============================================================
   shared.h — TP3057 测试程序公共头文件
   所有拆分后的测试程序均 #include "shared.h"
   编译时需将 shared.cpp 一同加入工程

   语言标准：C95 (ISO/IEC 9899:1995)
   ============================================================ */


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











/* ==================== 各测试项独立使能开关 ==================== */
#define TEST_CON_ENABLE       1   /* 连接性测试 */
#define TEST_FUNC_ENABLE      1   /* 功能测试 */
#define TEST_IIL_ENABLE       1   /* IIL 输入低电平漏电流 */
#define TEST_IIH_ENABLE       1   /* IIH 输入高电平漏电流 */
#define TEST_IOZ_ENABLE       1   /* IOZ 三态漏电流 */
#define TEST_ICC0IBB0_ENABLE  1   /* 掉电电流 ICC0/IBB0 */
#define TEST_ICC1IBB1_ENABLE  1   /* 工作电流 ICC1/IBB1 */
#define TEST_GXA_ENABLE       1   /* GXA 发送绝对增益 */
#define TEST_GXR_ENABLE       1   /* GXR 发送增益-频率响应 */
#define TEST_GRA_ENABLE       1   /* GRA 接收绝对增益 */
#define TEST_GRR_ENABLE       1   /* GRR 接收增益-频率响应 */
#define TEST_GRRL_ENABLE      1   /* GRRL 接收增益-电平响应 */
#define TEST_SFDX_ENABLE      1   /* SFDX 发送单频失真 */
#define TEST_SFDR_ENABLE      1   /* SFDR 接收单频失真 */
#define TEST_IMD_ENABLE       1   /* IMD 互调失真 */

/* SFDR 本地常量（原 test_11_sfdr.cpp 中定义，不在 shared.h） */
#define SFDR_FREQ_DIV  383         /* 50MHz/383≈130.5kHz, 1020Hz 对齐 bin16 */
#define SFDR_NAVG      4           /* 测量平均次数 */


void PASCAL TP3057()
{
    /* ================================================================
       Test 01: CON-TEST — 连接性测试
       对所有模拟引脚加 -0.1mA 测二极管正向压降，范围 -0.1V ~ -1.9V
       失败后立即停止，不执行后续任何测试
       ================================================================ */
#if TEST_CON_ENABLE
    {
        CLOSE_RELAY("3,4,5,6");

        SET_DPS(1, 0, V, 20, MA);
        SET_DPS(2, 0, V, 20, MA);

        PMU_CONDITIONS(FIMV, -0.1, MA, 2, V);

        if (!PMU_MEASURE("1,2,3,4,44,45,46,47,48", 20, "CON_", V, -0.1, -1.9))
        {
            SHOW_RESULT("CON-TEST=>FAIL", 1, "", 1, 1);
            //BIN(1);
            CLEAR_RELAY("3,4,5,6");
            DPS_OFF(DPS1);
            DPS_OFF(DPS2);
            return;
        }

        CLEAR_RELAY("3,4,5,6");
        Delay(10);
        CLEAR_ALL();

        SHOW_RESULT("CON-TEST=>PASS", 1, "", 1, 1);
    }
#endif

    /* ================================================================
       Test 02: FUNC-TEST — 功能测试
       上电后运行功能图形，DVM 测 VFRO 输出电压
       预期 loopback gain：±0.3225 dB（相对 0 dBm0）
       ================================================================ */
#if TEST_FUNC_ENABLE
    {
        double Vout;
        double backloop_db;

        SETUP();

        SET_RELAY("7");
        Delay(10);
        SET_AS(2.5, V, 1020, HZ);
        Delay(10);

        RUN_PATTERN(13, 0, 0, 0);
        Vout = AVM_MEASURE(1, 5, V, 5);

        backloop_db = 20.0 * log10(Vout / 1.2276);

        if (backloop_db < -0.3225 || backloop_db > 0.3225) {
            SHOW_RESULT("FUNC_FAIL", backloop_db, "dB", 0.3225, -0.3225);
        }
        else {
            SHOW_RESULT("FUNC_PASS", backloop_db, "dB", 0.3225, -0.3225);
        }

        CLEAR_RELAY("7");
        DPS_OFF(DPS1);
        DPS_OFF(DPS2);
    }
#endif

    /* ================================================================
       Test 03: IIL — 输入低电平漏电流测试
       图形1 将所有输入拉高，PMU 对每个输入脚加 0.4V 测电流
       限值：±10 uA
       ================================================================ */
#if TEST_IIL_ENABLE
    {
        double result_iil_1;

        SETUP();

        RUN_PATTERN(1, 1, 0, 0);

        PMU_CONDITIONS(FVMI, 0.4, V, 100, UA);

        if (!PMU_MEASURE("1,2,3,4,44,45,47", 5, "IIL", UA, 10, -10))
            //{ BIN(2);}
        else
            {SHOW_RESULT("IIL=>PASS", 1, "", 1, 1);}

        //result_iil_1 = PMU_MEASURE("1,2,3,4,44,45,47", 5);
        //SHOW_RESULT("result_iil_1", result_iil_1, "UV", 10, -10);

        DPS_OFF(DPS1);
        DPS_OFF(DPS2);
    }
#endif

    /* ================================================================
       Test 04: IIH — 输入高电平漏电流测试
       图形2 将所有输入拉低，PMU 对每个输入脚加 3V 测电流
       限值：±10 uA
       ================================================================ */
#if TEST_IIH_ENABLE
    {
        double result_iih_1;

        SETUP();

        RUN_PATTERN(2, 1, 0, 0);

        PMU_CONDITIONS(FVMI, 3, V, 100, UA);

        if (!PMU_MEASURE("1,2,3,4,44,45,47", 5, "IIH", UA, 10, -10))
            //{ BIN(3); }
        else
            { SHOW_RESULT("IIH=>PASS", 1, "", 1, 1); }

        //result_iih_1 = PMU_MEASURE("1,2,3,4,44,45,47", 5);
        //SHOW_RESULT("result_iih_1", result_iih_1, "UV", 10, -10);

        DPS_OFF(DPS1);
        DPS_OFF(DPS2);
    }
#endif

    /* ================================================================
       Test 05: IOZ — 三态漏电流测试
       图形4 使 DX 进入高阻态，PMU 分别测 IOZL(0V) 和 IOZH(5V)
       限值：±10 uA
       ================================================================ */
#if TEST_IOZ_ENABLE
    {
        double result_iozl_1;
        double result_iozh_1;

        SETUP();

        RUN_PATTERN(4, 1, 0, 0);

        /* IOZL：对地漏电（加 0.1V 测电流） */
        PMU_CONDITIONS(FVMI, 0.1, V, 100, UA);
        if (!PMU_MEASURE("46", 5, "IOZL", UA, 10, -10))
            //{BIN(4);}

        //result_iozl_1 = PMU_MEASURE("46", 5);
        //SHOW_RESULT("result_iozl_1", result_iozl_1, "UA", 10, -10);

        /* IOZH：对 VCC 漏电（加 4.9V 测电流） */
        PMU_CONDITIONS(FVMI, 4.9, V, 100, UA);
        if (!PMU_MEASURE("46", 5, "IOZH", UA, 10, -10))
           // {BIN(5);}

        SHOW_RESULT("IOZ=>PASS", 1, "", 1, 1);

        //result_iozh_1 = PMU_MEASURE("46", 5);
        //SHOW_RESULT("result_iozh_1", result_iozh_1, "UA", 10, -10);

        DPS_OFF(DPS1);
        DPS_OFF(DPS2);
    }
#endif

    /* ================================================================
       Test 06: ICC0/IBB0 — 掉电电流测试
       图形5 使芯片进入掉电状态，DPS 回读 VCC 和 VBB 电流
       限值：ICC0 ≤ 1.5mA, IBB0 ≤ 0.3mA
       ================================================================ */
#if TEST_ICC0IBB0_ENABLE
    {
        int fail;

        SETUP();

        RUN_PATTERN(5, 1, 0, 0);
        Delay(50);

        fail = 0;

        if (!DPS_MEASURE(DPS1, R20MA, 5, "ICC0", MA, 1.5, No_LoLimit)) {
            fail++;
        }
        if (!DPS_MEASURE(DPS2, R2MA, 5, "IBB0", MA, 0.3, No_LoLimit)) {
            fail++;
        }

        if (fail != 0)
            //{BIN(6);}
        else
            {SHOW_RESULT("ICC0IBB0=>PASS", 1, "", 1, 1);}

        DPS_OFF(DPS1);
        DPS_OFF(DPS2);
    }
#endif

    /* ================================================================
       Test 07: ICC1/IBB1 — 工作电流测试
       图形6 使芯片进入正常工作状态，DPS 回读 VCC 和 VBB 电流
       限值：ICC1 ≤ 9mA, IBB1 ≤ 9mA
       ================================================================ */
#if TEST_ICC1IBB1_ENABLE
    {
        int fail;
        int icc1_fail;
        int ibb1_fail;

        SETUP();

        SET_DPS(1, -5, V, 100, MA);
        SET_DPS(2, 5, V, 100, MA);

        RUN_PATTERN(6, 1, 0, 0);
        Delay(50);

        fail = 0;
        icc1_fail = 0;
        ibb1_fail = 0;

        if (!DPS_MEASURE(DPS1, R20MA, 5, "ICC1", MA, 9, No_LoLimit)) {
            fail++;
            icc1_fail = 1;
        }
        if (!DPS_MEASURE(DPS2, R20MA, 5, "IBB1", MA, 9, No_LoLimit)) {
            fail++;
            ibb1_fail = 1;
        }

        if (fail != 0) {
            if (icc1_fail) {
                SHOW_RESULT("ICC1=>FAIL", 0, "", 1, 0);
               // BIN(7);
            }
            if (ibb1_fail) {
                SHOW_RESULT("IBB1=>FAIL", 0, "", 1, 0);
                //BIN(8);
            }
        }
        else
            SHOW_RESULT("ICC1IBB1=>PASS", 1, "", 1, 1);

        DPS_OFF(DPS1);
        DPS_OFF(DPS2);
    }
#endif

    /* ================================================================
       Test 08: GXA + GXR — 发送增益测试
       GXA：1020Hz 0dBm0 → TX → 捕获 DX PCM → 计算绝对增益 ±0.15dB
       GXR：遍历 300/1000/2000/3000Hz，计算相对 GXA 的增益偏差 ±0.15dB
       依赖：GXR 使用全局变量 gxa1020（GXA 测得的参考增益）
       注意：原独立文件中 BIN(7)/BIN(8) 与 ICC1IBB1 冲突，已改为 27/28
       ================================================================ */
#if TEST_GXA_ENABLE || TEST_GXR_ENABLE
    {
#if TEST_GXA_ENABLE
        /* ---------- GXA ---------- */
        double vout_rms_gxa;
        double gain_db;
        unsigned char pcm_buf_gxa[512];
        int num_byte;

        SETUP();
        SET_AS(1.2276, V, 1020, HZ);

        RUN_PATTERN(7, 0, 0, 0);
        num_byte = capture_multi_pcm_bytes_fsx(7, 46, 43, 0, pcm_buf_gxa, 512);
        if (num_byte == 0) {
            //BIN(27);                            /* 原 BIN(7)，与 ICC1 冲突，改为 27 */
            SHOW_RESULT("GXA_CAP_FAIL", 1, "", 1, 0);
            goto END_GXA;
        }

        vout_rms_gxa = compute_vrms(pcm_buf_gxa, num_byte);
        gain_db = 20.0 * log10(vout_rms_gxa / 1.2276);
        gxa1020 = gain_db;

        SHOW_RESULT("GXA", gxa1020, "dB", 0.15, -0.15);
        if (gxa1020 <= -0.15 || gxa1020 >= 0.15) {
            //BIN(28);                            /* 原 BIN(8)，与 IBB1 冲突，改为 28 */
        }
#endif
    END_GXA: ;

#if TEST_GXR_ENABLE
        /* ---------- GXR ---------- */
        {
            unsigned char pcm_buf_gxr[512];
            int num_byte_gxr;
            double freq_list_gxr[4];
            int num_freq_gxr;
            int fail_count;
            int i;
            double f;
            double vout_rms_gxr;
            double Gxf;
            double GXRf;
            char name[32];

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
                    //BIN(29);
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
               // {BIN(30);}
            else
                {SHOW_RESULT("GXR=>PASS", 1, "", 1, 1);}
        }
#endif

        DPS_OFF(DPS1);
        DPS_OFF(DPS2);
    }
#endif /* TEST_GXA_ENABLE || TEST_GXR_ENABLE */

    /* ================================================================
       Test 09: GRA + GRR + GRRL — 接收增益测试
       GRA：  图形10(1020Hz 0dBm0) → AVM 测 VFRO → 计算绝对增益 ±0.15dB
       GRR：  遍历 300/3000Hz 0dBm0，相对 GRA 的增益偏差 ±0.15dB
       GRRL： 遍历 -40/+3 dBm0，相对 GRA 的增益偏差 ±0.2dB
       依赖：GRR/GRRL 使用全局变量 gra1020
       ================================================================ */
#if TEST_GRA_ENABLE || TEST_GRR_ENABLE || TEST_GRRL_ENABLE
    {
        double gra1020_local;

        gra1020_local = 0.0;

#if TEST_GRA_ENABLE
        /* ---------- GRA ---------- */
        {
            double vout_rms_gra;

            SETUP();

            vout_rms_gra = 0.0;
            RUN_PATTERN(10, 0, 0, 0);
            SET_AVM_PATH(LP20, BPPASS);
            vout_rms_gra = AVM_MEASURE(1, 2.0, V, 10);
            gra1020_local = 20.0 * log10(vout_rms_gra / 1.2276);
            gra1020 = gra1020_local;

            SHOW_RESULT("GRA", gra1020_local, "dB", 0.15, -0.15);
            if (gra1020_local <= -0.15 || gra1020_local >= 0.15) {
                //BIN(11);
            }
        }
#endif

#if TEST_GRR_ENABLE
        /* ---------- GRR ---------- */
        {
            int start_idx_list[2];
            double freq_list_grr[2];
            int num_freq_grr;
            int fail;
            int i;
            double vout_rms_grr;
            double Grf;
            double GRR_val;
            char name[32];

            SETUP();

            start_idx_list[0] = 8;
            start_idx_list[1] = 12;
            freq_list_grr[0]  = 300.0;
            freq_list_grr[1]  = 3000.0;
            num_freq_grr = 2;
            fail = 0;

            for (i = 0; i < num_freq_grr; i++) {
                RUN_PATTERN(start_idx_list[i], 0, 0, 0);
                SET_AVM_PATH(LP20, BPPASS);
                vout_rms_grr = AVM_MEASURE(1, 2.0, V, 10);
                Grf = 20.0 * log10(vout_rms_grr / 1.2276);
                GRR_val = Grf - gra1020_local;

                sprintf_s(name, sizeof(name), "GRR_%dHz", (int)freq_list_grr[i]);
                SHOW_RESULT(name, GRR_val, "dB", 0.15, -0.15);

                if (GRR_val <= -0.15 || GRR_val >= 0.15) fail++;
            }

            if (fail != 0)
               // {BIN(12);}
            else
                {SHOW_RESULT("GRR=>PASS", 1, "", 1, 1);}
        }
#endif

#if TEST_GRRL_ENABLE
        /* ---------- GRRL ---------- */
        {
            double level_dbm0[2];
            int index[2];
            int num;
            int fail_grrl;
            int i;
            int idx;
            double L;
            double vout_grrl;
            double Gabs;
            double delta;
            char name[32];

            SETUP();

            level_dbm0[0] = -40.0;
            level_dbm0[1] = 3.0;
            index[0] = 9;
            index[1] = 11;
            num = 2;

            fail_grrl = 0;
            for (i = 0; i < num; i++) {
                idx = index[i];
                L   = level_dbm0[i];
                RUN_PATTERN(idx, 0, 0, 0);
                if (i == 0) {
                    vout_grrl = AVM_MEASURE(1, 20.0, MV, 10) / 1000.0;
                }
                if (i == 1) {
                    vout_grrl = AVM_MEASURE(1, 2.0, V, 10);
                }

                Gabs = 20.0 * log10(vout_grrl / 1.2276);
                delta = Gabs - L - gra1020_local;

                sprintf_s(name, sizeof(name), "GRRL_%+ddBm", (int)L);
                SHOW_RESULT(name, delta, "dB", 0.2, -0.2);

                if (delta < -0.2 || delta > 0.2) fail_grrl++;
            }

            if (fail_grrl != 0)
                //{BIN(13);}
            else
                {SHOW_RESULT("GRRL=>PASS", 1, "", 1, 1);}
        }
#endif

        DPS_OFF(DPS1);
        DPS_OFF(DPS2);
    }
#endif /* TEST_GRA_ENABLE || TEST_GRR_ENABLE || TEST_GRRL_ENABLE */

    /* ================================================================
       Test 10: SFDX — 发送单频失真测试
       1020Hz 0dBm0 正弦 → TX PCM 编码 → DX 输出 → 捕获 512 字节
       → A-law 解码 → 加汉宁窗 → DFT → 基波 + 2次/3次谐波
       限值：≤ -46dB
       FFT: Fs=8000Hz, N=512, Δf=15.625Hz, 1020Hz→bin65
       ================================================================ */
#if TEST_SFDX_ENABLE
    {
        unsigned char pcm_data[512];
        int captured;
        double voltage[512];
        double fft_mag[256];
        double freq_res;
        int max_bin;
        int bin_fund;
        int bin_2nd;
        int bin_3rd;
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
            //BIN(14);
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
            //BIN(15);
            goto END_SFDX;
        }

        /* 7. 查找基波和谐波 bin */
        freq_res = SFDX_SAMPLE_RATE / SFDX_FFT_POINTS;
        max_bin  = SFDX_FFT_POINTS / 2;

        bin_fund = freq_to_bin(1020.0, freq_res, max_bin);
        bin_2nd  = freq_to_bin(2.0 * 1020.0, freq_res, max_bin);
        bin_3rd  = freq_to_bin(3.0 * 1020.0, freq_res, max_bin);

        if (bin_2nd < 0 || bin_3rd < 0) {
            //BIN(16);
            goto END_SFDX;
        }

        /* 8. 计算失真 */
        SFDX_db = calc_distortion_db(fft_mag[bin_fund],
                                      fft_mag[bin_2nd],
                                      fft_mag[bin_3rd]);

        if (SFDX_db >= 0.9) {
           // BIN(17);
            SHOW_RESULT("SFDX_bin_fund_LOST", 1, "", 1, 0);
        }
        else {
            SHOW_RESULT("SFDX", SFDX_db, "dB", -46, No_LoLimit);
            if (SFDX_db > -46) {
                //BIN(18);
            }
            else {
                SHOW_RESULT("SFDX=>PASS", 1, "", 1, 1);
            }
        }

    END_SFDX:
        DPS_OFF(DPS1);
        DPS_OFF(DPS2);
    }
#endif

    /* ================================================================
       Test 11: SFDR — 接收单频失真测试
       图形10(1020Hz 0dBm0 PCM) → DVM 采样 2048 点 × 4 次 → 加窗 → DFT
       → 累加平均幅度谱 → 基波 + 2次/3次谐波
       限值：≤ -46dB
       FFT: FREQ_DIV=383, Fs≈130548Hz, N=2048, Δf≈63.74Hz, 1020Hz→bin16
       注意：原独立文件中 BIN(18) 与 SFDX 冲突，SFDR_PAT_FAIL 改为 BIN(31)
       ================================================================ */
#if TEST_SFDR_ENABLE
    {
        double voltage[SFDR_FFT_POINTS];
        double fft_mag[SFDR_FFT_POINTS / 2];
        double fft_mag_sum[SFDR_FFT_POINTS / 2];
        double actual_sr;
        double freq_res;
        int max_bin;
        int bin_fund;
        int bin_2nd;
        int bin_3rd;
        double SFDR_db;
        int run;
        int i;

        /* 1. 初始化 */
        SETUP();

        /* 2. 清零累加数组 */
        for (i = 0; i < SFDR_FFT_POINTS / 2; i++) {
            fft_mag_sum[i] = 0.0;
        }

        /* 3. 多次测量，累加幅度谱 */
        for (run = 0; run < SFDR_NAVG; run++) {
            if (!RUN_PATTERN(10, 0, 0, 0)) {
                //BIN(31);                        /* 原 BIN(18)，与 SFDX 冲突，改为 31 */
                SHOW_RESULT("SFDR_PAT_FAIL", 1, "", 1, 0);
                goto END_SFDR;
            }
            Delay(10);

            MAT_DVM_MEASURE(1, 2.0, V, 10, SFDR_FFT_POINTS, SFDR_FREQ_DIV, voltage);

            apply_hanning(voltage, SFDR_FFT_POINTS);

            if (!DFT(voltage, SFDR_FFT_POINTS, fft_mag)) {
               // BIN(19);
                goto END_SFDR;
            }

            for (i = 0; i < SFDR_FFT_POINTS / 2; i++) {
                fft_mag_sum[i] += fft_mag[i];
            }
        }

        /* 4. 求平均幅度谱 */
        for (i = 0; i < SFDR_FFT_POINTS / 2; i++) {
            fft_mag[i] = fft_mag_sum[i] / (double)SFDR_NAVG;
        }

        /* 5. 查找基波和谐波 bin */
        actual_sr = 50e6 / SFDR_FREQ_DIV;
        freq_res  = actual_sr / SFDR_FFT_POINTS;
        max_bin   = SFDR_FFT_POINTS / 2;

        bin_fund = freq_to_bin(1020.0, freq_res, max_bin);
        bin_2nd  = freq_to_bin(2.0 * 1020.0, freq_res, max_bin);
        bin_3rd  = freq_to_bin(3.0 * 1020.0, freq_res, max_bin);

        if (bin_2nd < 0 || bin_3rd < 0) {
            //BIN(20);
            goto END_SFDR;
        }

        /* 6. 计算失真 */
        SFDR_db = calc_distortion_db(fft_mag[bin_fund],
                                      fft_mag[bin_2nd],
                                      fft_mag[bin_3rd]);

        if (SFDR_db >= 0.9) {
            //BIN(21);
            SHOW_RESULT("SFDR_FUND_LOST", 1, "", 1, 0);
        } else {
            SHOW_RESULT("SFDR", SFDR_db, "dB", -46, No_LoLimit);
            if (SFDR_db > -46) {
                //BIN(22);
            } else {
                SHOW_RESULT("SFDR=>PASS", 1, "", 1, 1);
            }
        }

    END_SFDR:
        DPS_OFF(DPS1);
        DPS_OFF(DPS2);
    }
#endif

    /* ================================================================
       Test 12: IMD — 互调失真测试 (Loop Around Measurement)
       生成 300Hz+3400Hz 双音 → AS 播放 → [TX] → DX→DR(relay7环回) → [RX]
       → DVM 采样 VFR+ 2048 点 × 4 次 → DFT 平均 → 4 分量(f1,f2,sum,diff)
       IMD = 20*log10(max(A_sum,A_diff)/max(A_f1,A_f2))
       限值：≤ -41dB
       FFT: FREQ_DIV=488, Fs≈102459Hz, N=2048, Δf≈50.03Hz
       ================================================================ */
#if TEST_IMD_ENABLE
    {
        WORD as_waveform[IMD_AS_POINTS];
        double voltage[IMD_FFT_POINTS];
        double fft_mag[IMD_FFT_POINTS / 2];
        double fft_mag_sum[IMD_FFT_POINTS / 2];
        double actual_sr;
        double freq_res;
        int max_bin;
        int bin_f1;
        int bin_f2;
        int bin_sum;
        int bin_diff;
        double A_f1;
        double A_f2;
        double A_sum;
        double A_diff;
        double A_imd;
        double A_fund;
        double IMD_db;
        int run;
        int i;
        double t;
        double v;

        /* 1. 初始化 + 闭合环回继电器 */
        SETUP();
        CLOSE_RELAY("7");
        Delay(10);

        /* 2. 生成双音波形 (300Hz + 3400Hz) */
        for (i = 0; i < IMD_AS_POINTS; i++) {
            t = (double)i / IMD_AS_SAMPLE_RATE;
            v = IMD_V_SINGLE_PEAK * (sin(2.0 * M_PI * IMD_F1 * t)
                                   + sin(2.0 * M_PI * IMD_F2 * t));
            if (v > 10.0)  v = 10.0;
            if (v < -10.0) v = -10.0;
            as_waveform[i] = (WORD)(((v / 10.0) + 1.0) / 2.0 * 65535.0 + 0.5);
        }

        /* 3. 加载波形到音频源 */
        LOAD_AS_PATTERN(7, 1, as_waveform);

        /* 4. 清零累加数组 */
        for (i = 0; i < IMD_FFT_POINTS / 2; i++) {
            fft_mag_sum[i] = 0.0;
        }

        /* 5. 多次测量，累加幅度谱 */
        for (run = 0; run < IMD_NAVG; run++) {
            RUN_AS_PATTERN(7, 1, IMD_AS_FREQ_DIV, 10);
            RUN_PATTERN(13, 0, 0, 0);
            Delay(5);

            MAT_DVM_MEASURE(1, 2.0, V, 10, IMD_FFT_POINTS, IMD_FREQ_DIV, voltage);

            apply_hanning(voltage, IMD_FFT_POINTS);

            if (!DFT(voltage, IMD_FFT_POINTS, fft_mag)) {
                //BIN(23);
                goto END_IMD;
            }

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
        max_bin   = IMD_FFT_POINTS / 2;

        bin_f1   = freq_to_bin(IMD_F1, freq_res, max_bin);
        bin_f2   = freq_to_bin(IMD_F2, freq_res, max_bin);
        bin_sum  = freq_to_bin(IMD_F1 + IMD_F2, freq_res, max_bin);
        bin_diff = freq_to_bin(IMD_F2 - IMD_F1, freq_res, max_bin);

        if (bin_f1 < 0 || bin_f2 < 0 || bin_sum < 0 || bin_diff < 0) {
            //BIN(24);
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
            //BIN(25);
            SHOW_RESULT("IMD_FUND_LOST", 1, "", 1, 0);
            goto END_IMD;
        }

        IMD_db = 20.0 * log10(A_imd / A_fund);

        SHOW_RESULT("IMD", IMD_db, "dB", -41.0, No_LoLimit);
        if (IMD_db > -41.0) {
            //BIN(26);
        } else {
            SHOW_RESULT("IMD=>PASS", 1, "", 1, 1);
        }

    END_IMD:
        CLEAR_RELAY("7");
        DPS_OFF(DPS1);
        DPS_OFF(DPS2);
    }
#endif

    /* ==================== 所有测试完成 ==================== */
    SHOW_RESULT("ALL_TESTS_DONE", 1, "", 1, 1);
}
