#include "MyRTC.h"
#include "HCT6521.h"        // AD采集

#include "Filter.h"         // 滤波
#include "Weight.h"         // 重量转换
#include "SM16028E.h"       // 数码管显示
#include "Led.h"            // 靶子指示LED
#include "Timer.h"          // 系统计时
#include "Key.h"            // 按键
#include "PunchDetect.h"    // 拳击检测
#include "GameLogic.h"      // 游戏逻辑
#include "Project_Config.h" // 整个工程的配置文件
#include "Calibration.h"
#include "KeyCal.h"         // 按键步进标定业务
#include "FilterTest.h"     // 滤波稳定性测试（测试用，不影响检测通道）

#include "debug.h"          // printf调试和delay延时

uint8_t KeyNum = 0;         // 键值（按键功能已恢复：PB12 低电平有效）


/*********************************************************************
 * @fn      main
 *
 * @brief   Main program.
 *
 * @retval  none
 */
int main(void)
{


    ///
    
    SystemCoreClockUpdate();        // 更新系统时钟变量（SystemCoreClock），确保后续延时和配置准确

    Delay_Init();                   // 初始化延时函数（基于SysTick）
    USART_Printf_Init(115200);      // 初始化串口打印，用于输出调试信息

    Timer2_Init(1000, 8);            // ARR = 999, PSC = 7 定时器初始化, 定时1Ms中断一次

    Key_Init();                     // 按键初始化（PB12，低电平有效）

    HCT6521_Init();                 //SPI引脚初始化,HTC6521寄存器配置初始化
    Cal_Init();                     // 标定模块初始化：加载 Flash 或使用默认值

    for (uint8_t i = 0; i < ADC_DEV_NUM; i++)
    {
        Filter_Init((AdcDevNumTypedef)i);
    }

    FilterTest_InitAll();           // 测试模块：复位全部通道的测试滤波状态

    // 获取数据，将当前稳定值设为零点
    for (uint8_t i = 0; i < ADC_DEV_NUM; i++)
    {
        int32_t  AdRawValue = 0;
        int32_t  FilterValue;    // 滤波后AD值
        uint32_t ZeroReadCnt = 0;

        // 读取一次空载AD值作为0点（带超时保护，避免 SDO 异常时死循环）
        while (!HCT6521_Task((AdcDevNumTypedef)i, &AdRawValue))
        {          
            if (++ZeroReadCnt > ADC_ZERO_READ_TIMEOUT)
            {
                printf("零点读取超时, 通道 %d, 按默认零点 0 处理\r\n", i);
                break;
            }
        }
        FilterValue = Filter_AdcFilt((AdcDevNumTypedef)i, AdRawValue, FilterMode_Dynamic);   // 开机零点采集：固定动态模式透传

        printf("FilterValue= %d  i= %d\r\n", FilterValue, i);
        Weight_Init((AdcDevNumTypedef)i, FilterValue);
        FilterTest_SetZero((AdcDevNumTypedef)i, FilterValue);   // 测试模块零点：与 Weight 同源（开机实测空载值）
    }

    for (uint8_t i = 0; i < ADC_DEV_NUM; i++)
    {
        Punch_Init((AdcDevNumTypedef)i);
    }

    MyRTC_Init();

    SM28_Init(SM28DevNum0);
    SM28_Clear(SM28DevNum0);

    Led_InitAll();                  // 初始化 6 路靶子指示 LED（默认全灭）

    Game_Init();
    Game_Start();

    printf("初始化完成！！！\r\n");

    uint8_t CalDevNum = 0;              // 按键标定目标通道号：0 = AdcDevNum0；按键数量不足，暂不做运行时通道切换，需标定其它通道时直接改此初值
    while (1)
    {
        int32_t RawValue;
        for (uint8_t i = 0; i < ADC_DEV_NUM; i++)
        {
            if (HCT6521_Task((AdcDevNumTypedef)i, &RawValue))
            {
                /* ----  滤波 ---- */
                int32_t AdFilterValue = Filter_AdcFilt((AdcDevNumTypedef)i, RawValue, Punch_GetFilterMode((AdcDevNumTypedef)i));
                
                /* ----  重量转换 ---- */
                Weight_Update((AdcDevNumTypedef)i, AdFilterValue);

                /* ----  取重量 ---- */
                float Weight = Weight_GetValue((AdcDevNumTypedef)i);

                /* ----  拳击检测 ---- */
                Punch_Tick((AdcDevNumTypedef)i, Weight);

                /* ---- 调试打印 ---- */
                if (i == 0 || i == 1 || i == 2 || i == 3 || i == 4 || i == 5)
                {
                    // printf("{AD%d}%d\r\n", i + 1, RawValue);
                    // printf("{Filt%d}%d\r\n", i + 1, AdFilterValue);
                    printf("{Wei%d}%.02f\r\n", i + 1, Weight);
                }

                #if 0    /* FilterTest 测试模块：观察窗口滤波后的重量是否稳定（关闭改 0） */
                {
                    /* 如需只观察某一路，把条件改为 (i == 0) 即可 */
                    if (i == 0/*  || i == 1 || i == 2 || i == 3 || i == 4 || i == 5 */)
                    {
                        /* Feed 只判预热是否完成（占位输出传 NULL）；重量由 FilterTest_GetWeight 换算 */
                        if (FilterTest_Feed((AdcDevNumTypedef)i, RawValue, NULL))
                        {
                            printf("{Test%d}%.2f\r\n", i + 1, FilterTest_GetWeight((AdcDevNumTypedef)i));   // 重量（g，以标定表零点为基准）
                        }
                    }
                }
                #endif
            }
        }


        #if 1    /* 按键功能（PB12，低电平有效）；以下为原按键扫描代码 */
        // 按键扫描：读取键值后分发处理（无按键时返回 0）
        KeyNum = Key_GetNum();
        
        if (KeyNum == 2)
        {
            // Game_Start();
        }
        else if (KeyNum == 1)
        {
            KeyCal_Process((AdcDevNumTypedef)CalDevNum);   // 按键步进标定：每次按键标定一个点（目标通道由 CalDevNum 指定）
        }

        #endif

        Game_Task();    // 游戏状态机轮询

        static uint32_t Time , LastTime;
        Time = RTC_GetCounter();
        // 每1秒更新一次时间
        if (Time != LastTime)
        {
            SM28_ShowRunTime(SM28DevNum0, Time, 0);
//            printf("Time:%d\r\n", Time);
            LastTime = Time;
            SM28_Update(SM28DevNum0);
        }
        
    };
}

