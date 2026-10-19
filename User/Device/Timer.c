/* 本模块属设备层，禁止包含任何 APP 层头文件（APP→Device 调用仅限白名单） */

#include "Timer.h"
#include "Project_Config.h"
// #include "GameLogic.h"     // 分层重构：设备层不得包含应用层头文件，全文未使用 Game_* 符号
#include "ch32f10x.h"

static volatile uint32_t s_GameTickMs = 0;   // 累计毫秒，用于产生节拍
static volatile uint32_t s_GameTick = 0;     // 游戏节拍计数（每150ms+1）
static volatile uint32_t s_TempTickMs = 0;   // 累计毫秒（可清零，用于测时长）

/**
 * @brief 获取游戏时钟计数（每150ms递增一次）
 * @retval 当前游戏时钟计数值
 */
uint32_t GetGameTick(void)
{
    return s_GameTick;
}

/**
 * @brief 清零游戏时钟计数（每轮游戏开始时调用）
 */
void ClearGameTick(void)
{
    s_GameTick = 0;
}

/**
 * @brief 获取系统上电以来的毫秒计数
 * @retval 累计毫秒数
 */
uint32_t GetTempTickMs(void)
{
    return s_TempTickMs;
}

/**
 * @brief 清零系统毫秒计数（用于测量某段过程耗时，如游戏总时长）
 */
void ClearTempTickMs(void)
{
    s_TempTickMs = 0;
}

/**
 * @brief  初始化定时器2，用于产生系统时间基准（周期性中断）
 */
void Timer2_Init(uint16_t arr, uint16_t psc)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    // 使能 TIM2 时钟（位于 APB1 总线）
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    // 配置定时器时基结构体
    TIM_TimeBaseInitStructure.TIM_Period = arr;                         // 重装载值
    TIM_TimeBaseInitStructure.TIM_Prescaler = psc;                      // 预分频
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;         // 不额外分频
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;     // 向上计数模式
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);

    // 清除更新中断标志，并使能更新中断
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    // 配置 NVIC 中断优先级
    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;                 // 设置中断通道
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;       // 抢占优先级
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;              // 响应优先级
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // 启动定时器
    TIM_Cmd(TIM2, ENABLE);
}

//void TIM2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));  // CH32V系列才需要开启这行函数
void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        s_TempTickMs++;
        s_GameTickMs++;
        if (s_GameTickMs >= GAME_TICK_PERIOD_MS)
        {
            s_GameTick++;
            s_GameTickMs = 0;
        }
        // printf("Tick:%d\n", s_GameTickMs);
    }
}
