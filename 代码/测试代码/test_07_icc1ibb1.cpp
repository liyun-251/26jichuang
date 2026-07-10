/* ============================================================
   test_07_icc1ibb1.cpp — 工作电流测试 (ICC1 / IBB1)
   编译：head0710.cpp + 本文件 复制进入工程

   测试内容：图形6 使芯片进入正常工作状态，DPS 回读 VCC 和 VBB 电流
   限值：ICC1 <= 9mA, IBB1 <= 9mA
   ============================================================ */








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
