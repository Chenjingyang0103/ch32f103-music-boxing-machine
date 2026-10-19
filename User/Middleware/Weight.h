#ifndef __WEIGHT_H
#define __WEIGHT_H

#include "Project_Config.h"
#include "ch32f10x.h"

/* ---------- 私有状态结构体（外部不可见） ---------- */
typedef struct
{
    /* 重量计算结果 */
    float WeiValue;                 // 当前重量值（单位：g，未乘 Rate）

    /* 标定参数 */
    float WeiCalValue[CAL_TIMES];    // 各校准点对应的标准重量值（单位：g），如 {200, 500, 1000, 2000, 4000}
    int32_t DiffCalValue[CAL_TIMES];    // 各校准点对应的AD差值（滤波输出 - 零点）

    float WeiTrackValue;            // 零点跟踪阈值（重量小于此值且稳定时自动归零，当前未使用）
    float Rate;                     // 倍率修正系数（单点标定得出的整体增益，默认 1.0f）

    /* 当前AD差分值 */
    int32_t  DiffValue;                // 当前AD差分值（滤波输出 - 零点）

    /* 零点管理（由 Weight 模块自己维护） */
    int32_t ZeroAdc;                  // 当前零点对应的原始滤波值（即滤波输出值）

    /* 零点跟踪相关 */
    uint8_t TraceCount;               // 零点跟踪计数器（连续稳定次数，当前未使用）
    uint8_t TraceFlag;                // 零点跟踪锁定标志（1=正在跟踪中，当前未使用）

    /* 凑整优化 */
    uint8_t CheatFlag;              // 凑整功能使能（0=关闭，1=开启，当前未使用）

} WeightInfoTypedef;

/* ---------- 对外接口 ---------- */
void Weight_Update(AdcDevNumTypedef DevNum, int32_t AdFilterValue);    // 称重核心处理：差分 → 分段线性插值 → 重量(g)
void Weight_Init(AdcDevNumTypedef DevNum, int32_t AdStableValue);   // 初始化通道：设零点、从 Calibration 读标定差值
float Weight_GetValue(AdcDevNumTypedef DevNum);         // 获取当前重量（单位：g）
int32_t Weight_GetZeroValue(AdcDevNumTypedef DevNum);   // 获取当前零点值（ADC 码值）
void Weight_SetZero(AdcDevNumTypedef DevNum, int32_t ZeroValue);   // 手动置零：将当前 ADC 值设为零点，重量归零
int32_t Weight_GetAdcDiff(AdcDevNumTypedef DevNum);      // 获取当前AD差分值（调试用）
int32_t Weight_GetAdcValue(AdcDevNumTypedef DevNum);      // 获取最近一次滤波后的 ADC 值（= 零点 + 当前差分）

#endif
