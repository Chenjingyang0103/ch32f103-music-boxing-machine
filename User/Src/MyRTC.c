#include "MyRTC.h"
#include "debug.h"
#include "ch32f10x_rtc.h"


/**
 * @brief  RTC 初始化函数
 * @note   使用内部低速时钟 LSI (40000Hz) 作为 RTC 时钟源
 *         配置预分频器使得计数器每秒加 1
 */
void MyRTC_Init(void)
{
    // 使能电源管理和备份域接口时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_BKP, ENABLE);

    // 允许访问备份域（BKP 寄存器和 RTC）
    PWR_BackupAccessCmd(ENABLE);

    // 开启内部低速晶振 LSI
    RCC_LSICmd(ENABLE);

    // 等待 LSI 就绪（晶振起振稳定）
    while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) != SET);

    // 选择 LSI 作为 RTC 时钟源
    RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
    // 使能 RTC 时钟
    RCC_RTCCLKCmd(ENABLE);

    // 等待 RTC 时钟同步和 APB 接口同步
    RTC_WaitForSynchro();
    // 等待上一次 RTC 操作完成
    RTC_WaitForLastTask();

    // 设置预分频器：使 RTC 计数频率为 1 Hz
    // LSI = 40000 Hz，预分频值 = 40000 - 1 得到 1 Hz 脉冲
    RTC_SetPrescaler(40000 - 1);
    RTC_WaitForLastTask();

    // 设置 RTC 计数器初始值为时间戳 0
    RTC_SetCounter(0);
    RTC_WaitForLastTask();
}


static volatile uint8_t s_rtcWakeupFlag = 0;   // RTC 闹钟唤醒标志（供主循环轮询）

/**
 * @brief  使能RTC闹钟中断，并设置第一次闹钟（当前时间 + (seconds)秒）
 */
void MyRTC_Alarm_Init(uint32_t seconds)
{
    NVIC_InitTypeDef NVIC_InitStructure = {0};

    // 等待RTC操作完成
    RTC_WaitForLastTask();

    // 设置闹钟值 = 当前计数器值 + seconds
    uint32_t current_cnt = RTC_GetCounter();
    RTC_WaitForLastTask();
    RTC_SetAlarm(current_cnt + seconds);
    RTC_WaitForLastTask();

    // 使能RTC闹钟中断
    RTC_ITConfig(RTC_IT_ALR, ENABLE);
    RTC_WaitForLastTask();

    // 配置NVIC，使能RTC中断
    NVIC_InitStructure.NVIC_IRQChannel = RTC_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

/**
 * @brief  为当前RTC闹钟重载一个周期（在闹钟触发后调用）
 * @param  seconds  下一次闹钟的间隔（秒）
 * @note   该函数会读取当前RTC计数器，并设置闹钟为 当前值 + seconds - 1,为什么要-1是因为
 *         假如当前值为1,second为1,设置的值就是2,当闹钟值=2的时候不会报警,超过2(到3)的时候才会报警,所以值要-1
 */
void MyRTC_Alarm_Reload(uint32_t seconds)
{
    RTC_WaitForLastTask();
    uint32_t current_cnt = RTC_GetCounter();
    RTC_WaitForLastTask();
    RTC_SetAlarm(current_cnt + seconds - 1);
    RTC_WaitForLastTask();
}

/**
 * @brief  设置 RTC 计数值
 * @param  TimeStamp  要写入的计数值（秒）
 */
void MyRTC_SetCounter(time_t TimeStamp)
{
    RTC_WaitForLastTask();  // 等待上一次 RTC 操作完成
    RTC_SetCounter(TimeStamp);
}

/**
 * @brief  将时间戳转换为北京时间
 * @param  TimeStamp  [in]  输入时间戳（秒）
 * @param  tm         [out] 转换结果（北京时间，UTC+8）
 * @note   TimeStamp 或 tm 为 NULL 时直接返回
 */
void MyRTC_TimeToDate(time_t *TimeStamp, struct tm *tm)
{
    if (TimeStamp == NULL || tm == NULL)
    {
        return;
    }

    // 将时间戳加上 8 小时，得到北京时间
    time_t beijing_time = *TimeStamp + 8 * 3600;
    // 使用 gmtime 得到 UTC 时间，但加上偏移后相当于北京时间
    struct tm *utc_tm = gmtime(&beijing_time);
    if (utc_tm != NULL)
    {
        *tm = *utc_tm;  // 复制结构体
    }
}


/**
 * @brief  RTC闹钟中断服务函数
 *         __attribute__((interrupt("WCH-Interrupt-fast"))) 保持与你的风格一致
 */
void RTC_IRQHandler(void)
{
    if (RTC_GetITStatus(RTC_IT_ALR) != RESET)
    {
        RTC_ClearITPendingBit(RTC_IT_ALR);       // 清除闹钟中断标志
        // TODO: ISR 内不应 printf（07 文档规定 ISR 应短小）；建议改为置标志由主循环打印，本轮暂不改。
        printf("进入RTC中断\r\n");
        s_rtcWakeupFlag = 1;                     // 设置标志，通知主循环执行任务
    }
}
