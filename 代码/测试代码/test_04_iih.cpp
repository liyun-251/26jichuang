/* ============================================================
   test_04_iih.cpp — IIH 输入高电平漏电流测试
   编译：head0710.cpp + 本文件 复制进入工程

   测试内容：图形2 将所有输入拉低，PMU 对每个输入脚加 3V 测电流
   限值：+-10 uA
   ============================================================ */





#define TEST_ENABLE  1
double result_iih_1 = 0;    
void PASCAL TP3057()
{
#if TEST_ENABLE
    SETUP();

    /* 运行图形2：所有输入端置"0" */
    RUN_PATTERN(2, 1, 0, 0);

    /* 加压测流：3V，量程 100uA */
    PMU_CONDITIONS(FVMI, 3, V, 100, UA);

    if (!PMU_MEASURE("1,2,3,4,44,45,47", 5, "IIH", UA, 10, -10))
    {
        BIN(3);
    }
    else
    {
        SHOW_RESULT("IIH=>PASS", 1, "", 1, 1);
    }
    //result_iih_1 = PMU_MEASURE("1,2,3,4,44,45,47", 5);
   //SHOW_RESULT("result_iih_1", result_iih_1, "UV", 10, -10);


    
    DPS_OFF(DPS1);
    DPS_OFF(DPS2);
#endif
}
