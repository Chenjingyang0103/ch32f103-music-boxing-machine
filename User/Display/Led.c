/**
 *********************************************************************************
 * @file    Led.c
 * @brief   靶子指示 LED 驱动（每路一个 GPIO 推挽输出，高电平点亮）
 * @note    - 6 路 LED 与 6 个靶子一一对应：LedDevNum0~5 → 靶子 1~6
 *          - 引脚：LED1=PB10, LED2=PB2, LED3=PB1, LED4=PB0, LED5=PA7, LED6=PA6
 *          - 极性：高电平点亮（共阴极），初始化为熄灭
 *********************************************************************************
 */

#include "Led.h"
#include "Project_Config.h"
#include "ch32f10x.h"
#include <stdint.h>

/* ==================== 设备参数数组（静态） ==================== */
static LedDeviceParamTypedef LedDeviceParam[LED_DEV_NUM];

/* ==================== 静态辅助函数 ==================== */
/**
 * @brief  使能 LED 所在端口的时钟并配置为推挽输出
 * @param  pSta  设备静态参数指针
 * @retval None
 * @note   高电平点亮，初始化后默认输出低电平（熄灭）
 */
static void Led_GPIO_Init(LedStaticParamTypedef *pSta)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    /* 使能对应端口时钟 */
    if (pSta->Port == GPIOA) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    else if (pSta->Port == GPIOB) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    else if (pSta->Port == GPIOC) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    else if (pSta->Port == GPIOD) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);
    else if (pSta->Port == GPIOE) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE);

    /* 配置为推挽输出 */
    GPIO_InitStruct.GPIO_Pin   = pSta->Pin;
    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(pSta->Port, &GPIO_InitStruct);

    GPIO_ResetBits(pSta->Port, pSta->Pin);      /* 高电平点亮，初始输出低电平（灭） */
}

/**
 * @brief  按状态写出 LED 电平（高电平点亮）
 * @param  pSta   设备静态参数指针
 * @param  State  1=点亮，0=熄灭
 * @retval None
 */
static void Led_WriteLevel(LedStaticParamTypedef *pSta, uint8_t State)
{
    if (State)
    {
        GPIO_SetBits(pSta->Port, pSta->Pin);        /* 高电平点亮 */
    }
    else
    {
        GPIO_ResetBits(pSta->Port, pSta->Pin);      /* 低电平熄灭 */
    }
}

/* ==================== 对外接口实现 ==================== */

/**
 * @brief  初始化指定 LED（配置引脚 + 默认熄灭）
 * @param  DevNum  设备号（0 ~ LED_DEV_NUM-1）
 * @retval None
 */
void Led_Init(LedDevNumTypedef DevNum)
{
    if (DevNum >= LED_DEV_NUM) return;

    LedStaticParamTypedef  *pSta = &LedDeviceParam[DevNum].StaticParam;
    LedRunningParamTypedef *pRun = &LedDeviceParam[DevNum].RunningParam;

    /* 按设备号配置引脚（LED1~LED6 ↔ LedDevNum0~5） */
    switch (DevNum)
    {
        case LedDevNum0: pSta->Port = GPIOB; pSta->Pin = GPIO_Pin_10; break;   // LED1
        case LedDevNum1: pSta->Port = GPIOB; pSta->Pin = GPIO_Pin_2;  break;   // LED2
        case LedDevNum2: pSta->Port = GPIOB; pSta->Pin = GPIO_Pin_1;  break;   // LED3
        case LedDevNum3: pSta->Port = GPIOB; pSta->Pin = GPIO_Pin_0;  break;   // LED4
        case LedDevNum4: pSta->Port = GPIOA; pSta->Pin = GPIO_Pin_7;  break;   // LED5
        case LedDevNum5: pSta->Port = GPIOA; pSta->Pin = GPIO_Pin_6;  break;   // LED6
        default: break;
    }

    Led_GPIO_Init(pSta);                    /* 配置引脚，默认熄灭 */
    pRun->State = 0;                        /* 初始状态：灭 */
}

/**
 * @brief  初始化全部 LED（上电调用一次）
 * @param  无
 * @retval None
 */
void Led_InitAll(void)
{
    for (uint8_t i = 0; i < LED_DEV_NUM; i++)
    {
        Led_Init((LedDevNumTypedef)i);
    }
}

/**
 * @brief  点亮指定 LED
 * @param  DevNum  设备号
 * @retval None
 */
void Led_On(LedDevNumTypedef DevNum)
{
    if (DevNum >= LED_DEV_NUM) return;
    LedDeviceParam[DevNum].RunningParam.State = 1;
    Led_WriteLevel(&LedDeviceParam[DevNum].StaticParam, 1);
}

/**
 * @brief  熄灭指定 LED
 * @param  DevNum  设备号
 * @retval None
 */
void Led_Off(LedDevNumTypedef DevNum)
{
    if (DevNum >= LED_DEV_NUM) return;
    LedDeviceParam[DevNum].RunningParam.State = 0;
    Led_WriteLevel(&LedDeviceParam[DevNum].StaticParam, 0);
}

/**
 * @brief  翻转指定 LED 状态
 * @param  DevNum  设备号
 * @retval None
 */
void Led_Toggle(LedDevNumTypedef DevNum)
{
    if (DevNum >= LED_DEV_NUM) return;
    LedDeviceParam[DevNum].RunningParam.State = !LedDeviceParam[DevNum].RunningParam.State;
    Led_WriteLevel(&LedDeviceParam[DevNum].StaticParam, LedDeviceParam[DevNum].RunningParam.State);
}

/**
 * @brief  按状态设置指定 LED
 * @param  DevNum  设备号
 * @param  State   1=点亮，0=熄灭
 * @retval None
 */
void Led_Set(LedDevNumTypedef DevNum, uint8_t State)
{
    if (DevNum >= LED_DEV_NUM) return;
    LedDeviceParam[DevNum].RunningParam.State = State ? 1 : 0;
    Led_WriteLevel(&LedDeviceParam[DevNum].StaticParam, LedDeviceParam[DevNum].RunningParam.State);
}

/**
 * @brief  熄灭全部 LED
 * @param  无
 * @retval None
 */
void Led_AllOff(void)
{
    for (uint8_t i = 0; i < LED_DEV_NUM; i++)
    {
        Led_Off((LedDevNumTypedef)i);
    }
}

/**
 * @brief  查询指定 LED 当前状态
 * @param  DevNum  设备号
 * @retval 1=点亮，0=熄灭（DevNum 越界返回 0）
 */
uint8_t Led_GetState(LedDevNumTypedef DevNum)
{
    if (DevNum >= LED_DEV_NUM) return 0;
    return LedDeviceParam[DevNum].RunningParam.State;
}
