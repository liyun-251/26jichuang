/* ============================================================
   test_06_icc0ibb0.cpp — 掉电电流测试 (ICC0 / IBB0)
   编译：head0710.cpp + 本文件 复制进入工程

   测试内容：图形5 使芯片进入掉电状态，DPS 回读 VCC 和 VBB 电流
   限值：ICC0 <= 1.5mA, IBB0 <= 0.3mA
   ============================================================ */







#define TEST_ENABLE  1

void PASCAL TP3057()
{
#if TEST_ENABLE
    int fail;

    SETUP();

    /* 运行图形5：芯片进入掉电状态 */
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
    {        
        BIN(6);
    }
    else
        {SHOW_RESULT("ICC0IBB0=>PASS", 1, "", 1, 1);}

    DPS_OFF(DPS1);
    DPS_OFF(DPS2);
#endif
}
