#ifndef __LED_H
#define __LED_H

#include "Project_Config.h"
#include "ch32f10x.h"
#include <stdint.h>

/* ==================== 设备号枚举 ==================== */
/// @brief 靶子指示 LED 设备号（每路一个 LED，直连靶子号）
typedef enum
{
    LedDevNum0 = 0,     // 靶子1 → LED1 → PB10
    LedDevNum1,         // 靶子2 → LED2 → PB2
    LedDevNum2,         // 靶子3 → LED3 → PB1
    LedDevNum3,         // 靶子4 → LED4 → PB0
    LedDevNum4,         // 靶子5 → LED5 → PA7
    LedDevNum5,         // 靶子6 → LED6 → PA6
} LedDevNumTypedef;

/* ==================== 设备参数结构体 ==================== */

/// @brief 静态参数（硬件引脚配置，初始化后不变）
typedef struct
{
    GPIO_TypeDef    *Port;      // LED 所在端口
    uint16_t        Pin;        // LED 所在引脚
} LedStaticParamTypedef;

/// @brief 运行参数（动态状态）
typedef struct
{
    uint8_t         State;      // 当前状态：1=点亮，0=熄灭
} LedRunningParamTypedef;

/// @brief 完整设备参数
typedef struct
{
    LedStaticParamTypedef      StaticParam;     // 静态参数
    LedRunningParamTypedef     RunningParam;    // 运行参数
} LedDeviceParamTypedef;

/* ==================== 对外接口 ==================== */

void Led_Init(LedDevNumTypedef DevNum);                     // 初始化指定 LED（配置引脚 + 默认熄灭）
void Led_InitAll(void);                                     // 初始化全部 LED（上电调用一次）
void Led_On(LedDevNumTypedef DevNum);                       // 点亮指定 LED
void Led_Off(LedDevNumTypedef DevNum);                      // 熄灭指定 LED
void Led_Toggle(LedDevNumTypedef DevNum);                   // 翻转指定 LED 状态
void Led_Set(LedDevNumTypedef DevNum, uint8_t State);       // 按状态设置（1=亮，0=灭）
void Led_AllOff(void);                                      // 熄灭全部 LED
uint8_t Led_GetState(LedDevNumTypedef DevNum);              // 查询指定 LED 当前状态（1=亮，0=灭）

#endif /* __LED_H */
