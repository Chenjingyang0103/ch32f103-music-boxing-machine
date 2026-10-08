#ifndef __PUNCHDETECT_H
#define __PUNCHDETECT_H

#include "Project_Config.h"
#include "ch32f10x.h"

// 已废弃：统一使用 Project_Config.h 中的 AdcDevNumTypedef
// typedef enum
// {
//     PunchDevNum0        = 0,    // AdcDevNum0 = 0,本项目只用到了AdcDevNum0, 其余枚举后续拓展用
//     PunchDevNum1,               // AdcDevNum1 = 1,
//     PunchDevNum2,               // AdcDevNum2 = 2,
//     PunchDevNum3,               // AdcDevNum3 = 3,
// }PunchDevNumTypedef;

/**
 * @brief 击打检测状态机状态
 */
typedef enum
{
    PunchState_Idle = 0,    // 空闲，等待冲击开始
    PunchState_Confirm,     // 已有一个点超触发阈值，等第二个点确认
    PunchState_Track,       // 已确认真实冲击，持续追踪峰值
} PunchStateTypedef;


/**
 * @brief 运行参数
 */
typedef struct
{
    PunchStateTypedef   state;          // 状态机当前状态
    float               PeakForce;      // 本次冲击到目前为止的峰值（g）
    uint8_t             Active;         // 1=冲击中，0=空闲
    uint8_t             HitReported;    // 1=有击中事件待上报（边沿触发）
    uint8_t             ReleaseCnt;     // 释放计数器：连续低于释放阈值的点数
} PunchRunningParamTypedef;


/**
 * @brief 静态参数
 */
typedef struct
{
    float   TriggerThreshold;   // 触发阈值（g），超过才算候选冲击
    float   ReleaseThreshold;   // 释放阈值（g），低于才累计释放计数
    uint8_t ReleaseCntLimit;    // 释放计数阈值 N：连续 N 点低于释放阈值判定松手
} PunchStaticParamTypedef;

/**
 * @brief 完整设备参数
 */
typedef struct
{
    PunchRunningParamTypedef    RunningParam;   // 运行参数
    PunchStaticParamTypedef     StaticParam;    // 静态参数
} PunchDeviceParamTypedef;

void Punch_Init(AdcDevNumTypedef DevNum);   // 初始化击打检测状态与阈值
void Punch_Tick(AdcDevNumTypedef DevNum, float force);  // 每次循环调用，更新检测状态
void Punch_Reset(AdcDevNumTypedef DevNum);  // 清空峰值和标志（每轮开始前调用）

uint8_t Punch_IsHit(AdcDevNumTypedef DevNum);   // 是否检测到击中（边沿触发，击中一次返回1后自动清零）
uint8_t Punch_IsActive(AdcDevNumTypedef DevNum);    // 是否处于冲击中（给 GameLogic 重置计时用）

float Punch_GetPeakForce(AdcDevNumTypedef DevNum);  // 获取本次击打的峰值力度（单位：g）
void Punch_ClearPeakForce(AdcDevNumTypedef DevNum); // 清空峰值力度（准备新一轮击打）

#endif
