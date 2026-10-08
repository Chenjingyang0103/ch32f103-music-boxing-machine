#ifndef __HCT6521_H
#define __HCT6521_H

#include "Project_Config.h"
#include "ch32f10x.h"

// /// @brief          设备号枚举
// typedef enum
// {
//     AdcDevNum0        = 0,    // AdcDevNum0 = 0,
//     AdcDevNum1,               // AdcDevNum1 = 1,
//     AdcDevNum2,
//     AdcDevNum3,
// }AdcDevNumTypedef;

/// @brief      PGA 增益选择（对应 ANA_REGIN 的 GA<1:0>）
typedef enum
{
    AdcGain_128           = 0x00,     // 128 倍
    AdcGain_64            = 0x01,     // 64 倍
    AdcGain_32            = 0x02,     // 32 倍
    AdcGain_1             = 0x03,     // 1 倍
} AdcGainTypedef;

/// @brief      数据输出速率（对应 ANA_REGIN 的 DR）
typedef enum
{
    AdcDr_10Hz            = 0x00,     // 10 Hz
    AdcDr_20Hz            = 0x40,     // 20 Hz
} AdcDrTypedef;

/// @brief      基准源选择（对应 ANA_REGIN 的 BGPEN）
typedef enum
{
    AdcRef_External       = 0x00,     // 外部基准（VREFP 引脚输入）
    AdcRef_Internal       = 0x20,     // 内部 1.19V 基准
} AdcRefTypedef;

/// @brief      输入短路控制（对应 ANA_REGIN 的 SHI）
typedef enum
{
    AdcShi_Disable        = 0x00,     // 关闭输入短路
    AdcShi_Enable         = 0x10,     // 开启输入短路（测量零漂）
} AdcShiTypedef;

/// @brief      MODE 位控制（对应 ANA_REGIN 的 MODE）
typedef enum
{
    AdcMode_Disable       = 0x00,     // 关闭（标准功耗）
    AdcMode_Enable        = 0x04,     // 开启（ENOB +0.4bit，功耗 +800uA）
} AdcModeTypedef;





/// @brief      状态机状态
typedef enum
{
    AdcFsmState_Idle = 0,   // 等 SDO 变低
    AdcFsmState_Reading,    // 读 24 位
    AdcFsmState_WaitHigh,   // 发 SCK pulse 后等 SDO 回高
} AdcFsmStateTypedef;

/// @brief      状态机运行参数
typedef struct
{
    AdcFsmStateTypedef  AdcFsmState;    // 当前状态机状态
    uint32_t            TimeOutCount;   // 超时计数
} AdcFsmParamTypedef;


/// @brief      静态参数（硬件配置，初始化后不变）
typedef struct
{
    /* ---- SDO 引脚配置（共用的数据线） ---- */
    GPIO_TypeDef            *SdoPort;        // SDO 读取端口（共用）
    uint16_t                SdoPin;           // SDO 读取引脚（共用）

    /* ---- 每个 ADC 独立的 SCK 控制引脚 ---- */
    GPIO_TypeDef            *SckPort;        // SCK 控制端口
    uint16_t                SckPin;           // SCK 控制引脚
} AdcStaticParamTypedef;


/// @brief      运行参数（动态数据，每次采集后更新）
typedef struct
{
    /* ---- 状态机参数 ---- */

    AdcFsmParamTypedef      AdcFsmParam;
    /* ---- 原始数据 ---- */
    int32_t                 RawValue;           // 原始AD值
    int32_t                 LastValue;          // 上一次采集值（当前项目未使用，可删除）

    /* ---- 低功耗状态（当前低功耗功能未启用） ---- */
    uint8_t                 LowPowerFlag;       // 0=工作模式，1=睡眠模式
    uint8_t                 FirstReadAfterWake; // 退出睡眠后首次读取需丢弃标记

} AdcRunningParamTypedef;


/// @brief      完整设备参数
typedef struct
{
    AdcStaticParamTypedef     StaticParam;      // 静态参数
    AdcRunningParamTypedef    RunningParam;     // 运行参数
} AdcDeviceParamTypedef;


void HCT6521_Init(void);    // 初始化全部 HCT6521 ADC 设备
uint8_t HCT6521_Task(AdcDevNumTypedef DevNum, int32_t *OutValue);   // 非阻塞状态机任务，读取完成返回 1 并输出原始值

/* 下部分为低功耗代码, 本项目中未使用因此注释掉了, 如果需要使用可以解除注释 */
// void HCT6521_LowPower(AdcDevNumTypedef DevNum, FunctionalState Enable);  // 进入/退出低功耗模式
// uint8_t HCT6521_IsLowPower(AdcDevNumTypedef DevNum); // 获取低功耗状态

#endif /* __HCT6521_H */
