/* ============================================================
   test_03_iil.cpp — IIL 输入低电平漏电流测试
   编译：head0710.cpp + 本文件 复制进入工程

   测试内容：图形1 将所有输入拉高，PMU 对每个输入脚加 0.4V 测电流
   限值：+-10 uA
   ============================================================ */





#define TEST_ENABLE  1
double result_iil_1 = 0;

void PASCAL TP3057()
{
#if TEST_ENABLE
    SETUP();

    /* 运行图形1：所有输入端置"1" */
    RUN_PATTERN(1, 1, 0, 0);

    /* 加压测流：0.4V，量程 100uA */
    PMU_CONDITIONS(FVMI, 0.4, V, 100, UA);
    
    if (!PMU_MEASURE("1,2,3,4,44,45,47", 5, "IIL", UA, 10, -10))//所有数字输入脚，删除了tsx-48，应在ioz测试
    {
        BIN(2);
    }
    else
    {
        SHOW_RESULT("IIL=>PASS", 1, "", 1, 1);
    }

    //result_iil_1 = PMU_MEASURE("1,2,3,4,44,45,47", 5);
    //SHOW_RESULT("result_iil_1", result_iil_1, "UV", 10, -10);


    DPS_OFF(DPS1);
    DPS_OFF(DPS2);
#endif
}
