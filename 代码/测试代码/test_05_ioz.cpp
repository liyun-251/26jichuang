/* ============================================================
   test_05_ioz.cpp — IOZ 三态漏电流测试
   编译：head0710.cpp + 本文件 复制进入工程

   测试内容：图形4 使 DX 进入高阻态，PMU 分别测 IOZL(0V) 和 IOZH(5V)
   限值：+-10 uA
   ============================================================ */


   



#define TEST_ENABLE  1
double result_iozl_1 = 0;
double result_iozh_1 = 0;

void PASCAL TP3057()
{
#if TEST_ENABLE
    SETUP();

    /* 运行图形4：DX 进入三态高阻 */
    RUN_PATTERN(4, 1, 0, 0);

    /* IOZL：对地漏电（加 0.1V 测电流） GND ≤ VO ≤ VCC */
    PMU_CONDITIONS(FVMI, 0.1, V, 100, UA);
    if (!PMU_MEASURE("46", 5, "IOZL", UA, 10, -10))
        BIN(4);



    //result_iozl_1 = PMU_MEASURE("46", 5);
    //SHOW_RESULT("result_iozl_1", result_iozl_1, "UA", 10, -10);



    /* IOZH：对 VCC 漏电（加 4.9V 测电流） GND ≤ VO ≤ VCC */
    PMU_CONDITIONS(FVMI, 4.9, V, 100, UA);
    if (!PMU_MEASURE("46", 5, "IOZH", UA, 10, -10))
        BIN(5);

    SHOW_RESULT("IOZ=>PASS", 1, "", 1, 1);



    //result_iozh_1 = PMU_MEASURE("46", 5);
    //SHOW_RESULT("result_iozh_1", result_iozh_1, "UA", 10, -10);


    
    DPS_OFF(DPS1);
    DPS_OFF(DPS2);
#endif
}
