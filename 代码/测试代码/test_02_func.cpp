/* ============================================================
   test_02_func.cpp — 功能测试 (FUNC-TEST)
   编译：head0710.cpp + 本文件 复制进入工程


   测试内容：上电后运行功能图形，DVM 测 VFRO 输出电压
   预期范围：2.3V ~ 2.7V
   ============================================================ */





#define TEST_ENABLE  1

void PASCAL TP3057()
{
#if TEST_ENABLE


    SETUP();

    SET_RELAY("7");
    Delay(10);
    SET_AS(2.5, V, 1020, HZ);
    Delay(10);

    RUN_PATTERN(13,0,0,0);
    double BL_Vout=AVM_MEASURE(1,5,V,5);

    double backloop_db = 20.0 * log10(BL_Vout / 1.2276);
    
    if (backloop_db < -0.3225 || backloop_db > 0.3225) {
        SHOW_RESULT("FUNC_FAIL",backloop_db,"dB",0.3225,-0.3225);
    }
    else{
        SHOW_RESULT("FUNC_PASS",backloop_db,"dB",0.3225,-0.3225);
    }

    CLEAR_RELAY("7");
    DPS_OFF(DPS1);
    DPS_OFF(DPS2);
#endif
}
