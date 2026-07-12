/* ============================================================
   shared.h — TP3057 测试程序公共头文件
   所有拆分后的测试程序均 #include "shared.h"
   编译时需将 shared.cpp 一同加入工程

   语言标准：C95 (ISO/IEC 9899:1995)
   ============================================================ */

#ifndef SHARED_H
#define SHARED_H

#include "StdAfx.h"
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
#define SFDR_FFT_POINTS       512         /* FFT 点数（2的幂） */
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

#endif /* SHARED_H */
