#include "MyRTC.h"
#include "HCT6521.h"        // AD采集
#include "MyFlash.h"        // 内部FLash存储

#include "Filter.h"         // 滤波
#include "Weight.h"         // 重量转换
#include "SM16028E.h"       // 数码管显示
#include "Timer.h"          // 系统计时
#include "PunchDetect.h"    // 拳击检测
#include "GameLogic.h"      // 游戏逻辑
#include "Project_Config.h" // 整个工程的配置文件
#include "Calibration.h"

#include "debug.h"          // printf调试和delay延时

uint8_t KeyNum = 0;         // 键值

/**
 * @brief  执行一次完整标定流程（阻塞）
 * @param  DevNum  通道号
 * @note   按顺序标定：零点 → 200g → 500g → 1000g → 2000g → 4000g
 *         全部完成后调用 Cal_Save 写入 Flash。
 *         单点标定由 Cal_SetRate 独立完成，不在此函数内。
 */
static void Cal_Process(AdcDevNumTypedef DevNum)
{
    printf("开始标定通道 %d\r\n", DevNum);

    /* ---- 1. 标定零点 ---- */
    printf("请清空秤台，等待稳定...\r\n");
    if (!Cal_SetZero(DevNum))
    {
        printf("零点标定失败（超时）\r\n");
        return;
    }
    printf("零点标定完成：%d\r\n", Cal_GetAdc(DevNum, CalPoint_Zero));

    /* ---- 2. 依次标定各砝码点 ---- */
    struct
    {
        CalPointTypedef point;
        const char     *name;
    } steps[] = {
        {CalPoint_200g,  "200g"},
        {CalPoint_500g,  "500g"},
        {CalPoint_1000g, "1000g"},
        {CalPoint_2000g, "2000g"},
        {CalPoint_4000g, "4000g"},
    };

    for (uint8_t i = 0; i < sizeof(steps) / sizeof(steps[0]); i++)
    {
        printf("请放置 %s 砝码，等待稳定...\r\n", steps[i].name);
        if (!Cal_SetPoint(DevNum, steps[i].point))
        {
            printf("%s 标定失败（超时）\r\n", steps[i].name);
            return;
        }
        printf("%s 标定完成：%d\r\n", steps[i].name,
               Cal_GetAdc(DevNum, steps[i].point));
    }

    /* ---- 3. 保存到 Flash ---- */
    if (Cal_Save())
    {
        printf("标定数据已保存到 Flash\r\n");
    }
    else
    {
        printf("标定数据保存失败\r\n");
    }

    /* ---- 4. 用新标定值重新初始化 Weight ---- */
    Weight_Init(DevNum, Cal_GetAdc(DevNum, CalPoint_Zero));
    printf("标定流程结束\r\n");
}

/*********************************************************************
 * @fn      main
 *
 * @brief   Main program.
 *
 * @retval  none
 */
int main(void)
{
    SystemCoreClockUpdate();        // 更新系统时钟变量（SystemCoreClock），确保后续延时和配置准确

    Delay_Init();                   // 初始化延时函数（基于SysTick）
    USART_Printf_Init(115200);      // 初始化串口打印，用于输出调试信息

    Timer2_Init(800, 10);           // ARR = 800, PSC = 10 定时器初始化, 定时1Ms中断一次

    // Key_Init();      //按键初始化



//    if (!MyFlash_Init((uint32_t *)AdCalArray, Rate))
//    {
//        printf("MyFlash: 标定表初始化失败\r\n");
//        /* 报警 / 看门狗复位 */
//    }
//    MyFlash_GetRate(Rate);
//    for(uint8_t i = 0; i < ADC_DEV_NUM; i ++)
//    {
//        printf("Rate[%d]:%f  ", i, Rate[i]);
//    }
//    printf("\r\n");


    HCT6521_Init();                 //SPI引脚初始化,HTC6521寄存器配置初始化
    Cal_Init();                     // 标定模块初始化：加载 Flash 或使用默认值

    for (uint8_t i = 0; i < ADC_DEV_NUM; i++)
    {
        Filter_Init((AdcDevNumTypedef)i);
    }





    // 获取数据，将当前稳定值设为零点
    for (uint8_t i = 0; i < ADC_DEV_NUM; i++)
    {
        int32_t AdRawValue;
        int32_t FilterValue;    // 滤波后AD值

        // 读取一次空载AD值作为0点
        while (!HCT6521_Task((AdcDevNumTypedef)i, &AdRawValue));
        FilterValue = Filter_AdcFilt((AdcDevNumTypedef)i, AdRawValue);

        printf("FilterValue= %d  i= %d\r\n", FilterValue, i);
        Weight_Init((AdcDevNumTypedef)i, FilterValue);
    }

    for (uint8_t i = 0; i < ADC_DEV_NUM; i++)
    {
        Punch_Init((AdcDevNumTypedef)i);
    }


    MyRTC_Init();

    SM28_Init(SM28DevNum0);
    SM28_Clear(SM28DevNum0);

    Game_Init();

    printf("初始化完成！！！\r\n");

    while (1)
    {
        int32_t RawValue;
        for (uint8_t i = 0; i < ADC_DEV_NUM; i++)
        {
            if (HCT6521_Task((AdcDevNumTypedef)i, &RawValue))
            {
                /* ----  滤波 ---- */
                int32_t AdFilterValue = Filter_AdcFilt((AdcDevNumTypedef)i, RawValue);

                /* ----  重量转换 ---- */
                Weight_Update((AdcDevNumTypedef)i, AdFilterValue);

                /* ----  取重量 ---- */
                float Weight = Weight_GetValue((AdcDevNumTypedef)i);

                /* ----  拳击检测 ---- */
                Punch_Tick((AdcDevNumTypedef)i, Weight);

                /* ---- 调试打印 ---- */
//                printf("{AD%d}%d\r\n", i + 1, RawValue);
//                printf("{Filt%d}%d\r\n", i + 1, AdFilterValue);
//                printf("{Wei%d}%f\r\n", i + 1, Weight);
            }
        }


        // 可以通过调试修改KeyNum来实现按键按下的效果, 进入之后KeyNum必须清零
        if (KeyNum == 1)
        {
            KeyNum = 0;
            Game_Start();

        }

        if (KeyNum == 2)
        {
            KeyNum = 0;
            Cal_Process(AdcDevNum0);   // 进入标定
        }

        if (KeyNum == 3)
        {
//            KeyNum = 0;

//            int32_t AdCalVal;
//            if(HCT6521_Task((AdcDevNumTypedef)0, &AdCalVal))
//            {
//                Rate[0] = Weight_Calibrate((AdcDevNumTypedef)0, Weight_500g, AdCalVal, AdCalArray);
//                printf("Rate:%f\r\n", Rate[0]);
//                if(Rate[0] == -1.0f)
//                {
//                    printf("校准错误!!!\r\n");
//                }
//                else
//                {
//                    MyFlash_SetRate(Rate);
//                }
//                if(!MyFlash_Save())
//                {
//                    printf("Rate数据写入失败!!!\r\n");
//                }

//            }
        }

        Game_Task();    // 游戏状态机轮询

        static uint32_t Time , LastTime;
        Time = RTC_GetCounter();
        // 每1秒更新一次时间
        if (Time != LastTime)
        {
            SM28_ShowRunTime(SM28DevNum0, Time, 0);
//            printf("Time:%d\r\n", Time);
            LastTime = Time;


        }
        SM28_Update(SM28DevNum0);

    };
}

