#include "Key.h"
#include "debug.h"
#include "ch32f10x.h"

// 按键事件标志（volatile 确保编译器不优化）
static volatile uint8_t s_KeyPressed = 0;

/**
 * @brief 按键引脚初始化（GPIO + EXTI 中断）
 * @note  使用 PB12 引脚（低电平有效），下降沿触发，中断线 EXTI12（属于 EXTI15_10_IRQn）
 */
void Key_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    EXTI_InitTypeDef EXTI_InitStructure = {0};
    NVIC_InitTypeDef NVIC_InitStructure = {0};

    // 1. 使能 GPIOB 时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

    // 2. 配置 PB12 为上拉输入（低电平有效）
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // 3. 将 PB12 映射到 EXTI12
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource12);

    // 4. 配置 EXTI12 为下降沿触发中断
    EXTI_InitStructure.EXTI_Line = EXTI_Line12;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    // 5. 配置 NVIC 中断优先级（抢占优先级 2，子优先级 2）
    NVIC_InitStructure.NVIC_IRQChannel = EXTI15_10_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

/**
 * @brief  读取一次按键事件
 * @retval 1 本周期内检测到有效按键；0 无按键事件
 * @note   内部使用 Delay_Ms(20) 消抖，**会阻塞主循环**约 20ms
 */
uint8_t Key_GetNum(void)
{
    uint8_t KeyNum = 0;
    if (s_KeyPressed)
    {
        Delay_Ms(20);
        // 再次确认引脚仍为低电平（按下状态）
        if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_12) == 0)
        {
            KeyNum = 1;
        }
        s_KeyPressed = 0;   // 无论是否有效，清除标志，等待下次中断
    }
    return KeyNum;
}

/**
 * @brief EXTI15_10 中断服务函数（处理 PB12 下降沿）
 */
//void EXTI15_10_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast"))); // CH32V系列才需要开启这行函数
void EXTI15_10_IRQHandler(void)
{
    // 检查是否为 EXTI12 中断
    if (EXTI_GetITStatus(EXTI_Line12) != RESET)
    {
        s_KeyPressed = 1;   // 置位按键事件
        // 清除中断挂起标志
        EXTI_ClearITPendingBit(EXTI_Line12);
    }
}
