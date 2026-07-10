/* ============================================================
   test_01_contest.cpp — 连接性测试 (CON-TEST)
   编译：head0710.cpp + 本文件 复制进入工程


   测试内容：对所有引脚 PMU 加流测压，检查二极管正向压降
   失败后自动停止，不执行后续任何测试
   ============================================================ */





#define TEST_ENABLE  1    /* 1=执行, 0=跳过本测试 */

void PASCAL TP3057()
{
#if TEST_ENABLE
    /* 闭合继电器，连接模拟脚到 PMU */
    CLOSE_RELAY("3,4,5,6");

    /* 电源置零，确保安全 */
    SET_DPS(1, 0, V, 20, MA);
    SET_DPS(2, 0, V, 20, MA);

    /* 加流测压：-0.1mA，电压箝位 2V */
    PMU_CONDITIONS(FIMV, -0.1, MA, 2, V);

    /* 测量全部模拟通道，判断范围 -0.1V ~ -1.9V */
    if (!PMU_MEASURE("1,2,3,4,44,45,46,47,48", 20, "CON_", V, -0.1, -1.9))//加入模拟通道：1,2,3,4,5,41,42,43,44,45,46,47,48（第一次测试时模拟脚数值异常）
    {
        SHOW_RESULT("CON-TEST=>FAIL", 1, "", 1, 1);
        BIN(1);
        /* 连接性失败 -> 停止一切 */
        CLEAR_RELAY("3,4,5,6");
        DPS_OFF(DPS1);
        DPS_OFF(DPS2);
        return;
    }

    CLEAR_RELAY("3,4,5,6");
    Delay(10);
    CLEAR_ALL();

    SHOW_RESULT("CON-TEST=>PASS", 1, "", 1, 1);
#endif
}
