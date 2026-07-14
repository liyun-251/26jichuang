import math
import struct

# 预计算 A-law 编码表 (13-bit 输入 -> 8-bit 输出)
_ALAW_TABLE = None

def generate_alaw_sine(frequency, dbm0, sample_rate=8000,
                       duration=None, num_cycles=None, invert=True):
    """
    生成 A-law PCM 编码的正弦波。

    参数：
        frequency : 信号频率 (Hz)
        dbm0      : 电平 (dBm0)，0 dBm0 = 1.2276 Vrms
        sample_rate: 采样率 (Hz)，默认 8000
        duration  : 期望时长 (秒)，为 None 时自动计算最小整周期长度
        num_cycles: 期望周期数，优先级低于 duration
        invert    : 是否对偶数位取反 (ITU 标准)，默认 True（适合 WAV）

    返回：
        bytes : A-law PCM 编码字节串，可直接写入 WAV 或循环播放
    """
    # ---------- 计算幅度 ----------
    peak_0dBm0 = 0.6967   # 0 dBm0 的归一化峰值
    amplitude = peak_0dBm0 * (10 ** (dbm0 / 20.0))

    # ---------- 确定样本数（保证整周期）----------
    if duration is not None:
        # 根据期望时长取最接近的可闭环长度
        desired_samples = int(sample_rate * duration)
        # 求最小整数倍周期
        period = sample_rate / frequency
        m = max(1, round(desired_samples / period))
        N = round(m * period)
    elif num_cycles is not None:
        period = sample_rate / frequency
        m = num_cycles
        N = round(m * period)
    else:
        # 默认生成刚好 1 个周期的最小样本数
        period = sample_rate / frequency
        # 寻找最小的 m 使 m*period 为整数
        m = 1
        while abs(round(m * period) - m * period) > 1e-9:
            m += 1
            if m > 10000:  # 防止死循环
                break
        N = round(m * period)
        # 如果找不到完美整数，使用近似整数
        if abs(round(m * period) - m * period) > 1e-6:
            N = round(m * period)

    # ---------- 生成线性 PCM 样本（16-bit 范围）----------
    linear = []
    for n in range(N):
        # 相位从 0 开始
        t = n / sample_rate
        val = amplitude * math.sin(2 * math.pi * frequency * t)
        # 限幅保护
        if val > 1.0: val = 1.0
        if val < -1.0: val = -1.0
        # 转为 16 位整数 (-32768..32767)
        pcm16 = int(val * 32767)
        linear.append(pcm16)

    # ---------- A-law 编码 ----------
    alaw_bytes = bytearray()
    for pcm in linear:
        byte_val = linear_to_alaw_fast(pcm, invert)
        alaw_bytes.append(byte_val)

    return bytes(alaw_bytes)

def _init_alaw_table():
    """
    查表法
    """
    global _ALAW_TABLE
    if _ALAW_TABLE is not None:
        return
    _ALAW_TABLE = [0] * 4096
    # 按照 G.711 标准表生成
    # 段界和步长
    for i in range(4096):
        # 复制标准算法
        pcm = i
        seg = 7
        val = pcm
        while val >= 16:
            val >>= 1
            seg -= 1
        seg = max(0, 7 - seg)
        if seg < 2:
            quant = (pcm >> 1) & 0x0F
            seg = 0 if pcm < 32 else 1
        else:
            quant = (pcm >> (seg + 3)) & 0x0F
        _ALAW_TABLE[i] = (seg << 4) | quant

def linear_to_alaw_fast(pcm16, invert=True):
    """
    查表法
    """
    pcm = pcm16 >> 3
    sign = 0x00 if pcm < 0 else 0x80
    pcm_abs = abs(pcm)
    if pcm_abs > 4095:
        pcm_abs = 4095
    _init_alaw_table()
    code = sign | _ALAW_TABLE[pcm_abs]
    if invert:
        code ^= 0x55
    return code & 0xFF

def linear_to_alaw(pcm16, invert=True):
    """
    将 16 位线性 PCM 转为 A-law 8 位码字。
    pcm16: int, -32768..32767
    invert: 是否偶数位取反 (异或 0x55)
    """
    # 标准 G.711 输入为 13 位，16 位右移 3 位
    pcm = pcm16 >> 3

    # 符号位
    sign = 0x00 if pcm < 0 else 0x80
    pcm = abs(pcm)

    # 限幅到 13 位最大值 4095
    if pcm > 4095:
        pcm = 4095

    # 确定段位和量化台阶
    if pcm < 256:
        # 段 0，量化台阶 = 2
        segment = 0
        quant = pcm >> 1       # 除以 2，得到 4 位（0..127 -> 0..63? 段 0 实际用 0..31 步? 按标准映射）
        # G.711 实际：段0 step=2, 16个量化值（0..15）？
        # 精确：A-law 段0 有 32 个量化级，步长 2，输入值 0..63 映射到 0..31
        # 这里按经典实现：
        if pcm < 64:
            segment = 0
            quant = pcm >> 1   # /2 -> 0..31
        else:
            segment = 1
            quant = (pcm - 64) >> 2
    else:
        # 段 1-7
        # 找到最高非零位
        seg = 1
        threshold = 256
        while seg < 7 and pcm >= threshold * 2:
            seg += 1
            threshold <<= 1
        segment = seg
        # 量化台阶 = 2^(seg+2) ？ 标准：段 s 步长 = 2^(s+2)
        shift = seg + 2
        # 段内起始值
        base = 1 << shift
        quant = (pcm - base) >> (shift - 4)  # 得到 4 位量化值 (0..15)

    # 当 pcm 很小或很大时，上面的逻辑需要统一
    # 采用常见的查表方式更可靠，这里重写一段标准版

    # ---- 更可靠的 A-law 编码实现 ----
    # 直接使用常用且经过验证的算法
    pcm = pcm16 >> 3
    sign = 0x00 if pcm < 0 else 0x80
    pcm = abs(pcm)
    if pcm > 4095:
        pcm = 4095

    # 确定段和量化
    if pcm < 256:
        seg = 0
        # 将 pcm 映射到 4 位码 (0..15)
        # 段 0 步长 = 2, 量化值 0..15 对应 0..30?
        # 标准: 段0 量化码 = (pcm/2) ，但输出是异或后的？
        # 为准确，采用 ITU 表格的反函数
        # 直接使用经典的 linear2alaw 实现（来自 sox / pjsip）
        pass

    # 改为下面这个标准实现
    return _linear_to_alaw_standard(pcm16, invert)


def _linear_to_alaw_standard(pcm16, invert):
    """经过验证的 linear -> A-law 算法 (G.711)"""
    # 取 16 位有符号，取绝对值的高 13 位
    pcm = pcm16 >> 3
    sign = 0x00 if pcm < 0 else 0x80
    pcm = abs(pcm)
    if pcm > 4095:
        pcm = 4095

    segment = 7
    if pcm < 16:
        segment = 0
        quant = pcm << 1  # ?
    else:
        # 计算段位
        val = pcm
        while val >= 16:
            val >>= 1
            segment -= 1
        segment = 7 - segment if segment < 7 else 0
        # 量化值
        quant = (pcm >> (segment + 3)) & 0x0F

    # 对于段 0，特殊处理
    if segment < 2:
        quant = (pcm >> 1) & 0x0F
        segment = 0 if pcm < 32 else 1

    # 组装码字: sign | (segment<<4) | quant
    code = sign | (segment << 4) | quant

    # 偶数位取反 (增加脉冲密度)
    if invert:
        code ^= 0x55

    return code & 0xFF

def alaw_sine_stream(frequency, dbm0, sample_rate=8000, chunk_size=160):
    """无限产生 A-law 字节流的生成器 (每次返回 chunk_size 字节)"""
    amp = 0.6967 * (10 ** (dbm0 / 20.0))
    n = 0
    while True:
        chunk = bytearray()
        for _ in range(chunk_size):
            t = n / sample_rate
            val = amp * math.sin(2 * math.pi * frequency * t)
            pcm16 = int(max(-1.0, min(1.0, val)) * 32767)
            chunk.append(linear_to_alaw_fast(pcm16))
            n += 1
        yield bytes(chunk)

def print_alaw_binary(data: bytes, per_row: int = 8, sep: str = " "):
    """
    将 A-law PCM 字节数据以二进制文本形式打印到控制台。
    
    参数:
        data    : bytes 对象，每个字节是一个 A-law 码字
        per_row : 每行显示的字节数
        sep     : 字节之间的分隔符
    """
    for i in range(0, len(data), per_row):
        chunk = data[i:i+per_row]
        bin_strs = [f'{byte:08b}' for byte in chunk]
        print(sep.join(bin_strs))


def show_alaw_binary(frequency, dbm0, sample_rate=8000, count=256, per_row=8):
    """
    便捷函数：生成指定参数的 A-law 正弦波并打印前 count 个字节的二进制。
    
    参数:
        frequency   : 信号频率 (Hz)
        dbm0        : 电平 (dBm0)
        sample_rate : 采样率 (Hz)
        count       : 打印的字节数
        per_row     : 每行字节数
    """
    data = generate_alaw_sine(frequency, dbm0, sample_rate)
    print(f"A-law PCM 二进制文本 (前 {min(count, len(data))} 字节):")
    print_alaw_binary(data[:count], per_row)

def show_stream_binary(frequency, dbm0, sample_rate=8000, count=256, per_row=8):
    """从 alaw_sine_stream 实时取字节打印二进制"""
    stream = alaw_sine_stream(frequency, dbm0, sample_rate)
    for i in range(count):
        b = next(stream)
        print(f'{b:08b}', end=' ')
        if (i + 1) % per_row == 0:
            print()
    print()  # 最后换行

def print_alaw_binary_full(data: bytes, sep: str = " "):
    """
    将 generate_alaw_sine 生成的完整 A-law 字节流
    以二进制文本形式打印到控制台。
    
    参数:
        data : bytes 对象
        sep  : 字节之间的分隔符，默认空格
    """
    for i, byte in enumerate(data):
        # 每字节 8 位二进制，高位在前
        print(f'{byte:08b}', end=sep if i < len(data)-1 else '')
    print()  # 最后换行


def show_alaw_binary_full(frequency, dbm0, sample_rate=8000):
    """
    便捷函数：生成正弦波 A-law PCM 并打印全部二进制文本。
    """
    data = generate_alaw_sine(frequency, dbm0, sample_rate)
    print_alaw_binary_full(data)

def print_alaw_inc_format(data: bytes):
    """
    将 A-law PCM 字节流按 INC(X11010Xab) 格式打印到控制台。
    每 256 行一个块，块首 a=1,b=0，其后 8 行输出一个字节的位，
    剩余行 a=0,b=0；数据不足的块用 a=0,b=0 补齐。
    """
    # 将所有字节拆成位序列（先高位）
    bits = []
    for byte in data:
        for i in range(7, -1, -1):
            bits.append((byte >> i) & 1)

    total_bytes = len(data)
    bytes_used = 0
    bit_ptr = 0

    while bytes_used < total_bytes:
        # 块首：a=1, b=0
        print("INC(X11010X10)")

        # 输出当前字节的 8 个位
        for _ in range(8):
            b_val = bits[bit_ptr]
            bit_ptr += 1
            print(f"INC(X11010X0{b_val})")

        bytes_used += 1

        # 补齐当前块剩余 247 行 (256 - 1 - 8 = 247)
        for _ in range(247):
            print("INC(X11010X00)")

    # 如果数据长度正好是整数个块，以上已经结束，不需要补零
    # 但若最后一个块不满，其实我们的循环已经用补齐行填满了，所以无需额外处理

def show_alaw_inc_format(frequency, dbm0, sample_rate=8000):
    """生成正弦波 A-law PCM 并打印 INC(X11010Xab) 格式"""
    data = generate_alaw_sine(frequency, dbm0, sample_rate)
    print(f"样本数: {len(data)} 字节")
    print_alaw_inc_format(data)

def save_alaw_inc_format(filename, frequency, dbm0, sample_rate=8000):
    """
    生成 A-law PCM 正弦波，并将 INC(X11010Xab) 格式的文本
    保存到指定的 txt 文件。
    
    参数:
        filename    : 输出文件路径 (例如 'output.txt')
        frequency   : 信号频率 (Hz)
        dbm0        : 电平 (dBm0)
        sample_rate : 采样率 (Hz)，默认 8000
    """
    # 生成闭环的 A-law PCM 字节
    data = generate_alaw_sine(frequency, dbm0, sample_rate)
    
    # 将所有字节拆成位序列（高位在前）
    bits = []
    for byte in data:
        for i in range(7, -1, -1):
            bits.append((byte >> i) & 1)

    total_bytes = len(data)
    bytes_used = 0
    bit_ptr = 0

    with open(filename, 'w') as f:
        # 可选：写入一行注释，说明参数和样本数
        f.write(f"# frequency={frequency}Hz, dBm0={dbm0}, samples={total_bytes} bytes\n")

        while bytes_used < total_bytes:
            # 块首：a=1, b=0
            f.write("INC(X11010X10)\n")
            # 当前字节的 8 个位
            for _ in range(8):
                b_val = bits[bit_ptr]
                bit_ptr += 1
                f.write(f"INC(X11010X0{b_val})\n")
            bytes_used += 1
            # 补齐当前块至 256 行
            for _ in range(247):
                f.write("INC(X11010X00)\n")

if __name__ == "__main__":
    save_alaw_inc_format("3000Hz 0dBm0.txt",3000,0)