#ifndef __FILTER_H
#define __FILTER_H

#include "Project_Config.h"
#include "ch32f10x.h"

/* ---------- 私有状态结构体（外部不可见） ---------- */
typedef struct
{
    int32_t FilterArray[20];   // 窗口数组（最大窗口 20，与 Calibration 一致）
    int32_t WinAvg;            // 上次窗口均值，用于决定新值插入头部还是尾部
} FilterInfoTypedef;

/* ---------- 滤波模式（Filter_AdcFilt 的模式入参） ---------- */
typedef enum
{
    FilterMode_Dynamic = 0,    // 动态模式：直接透传原始值，保留拳击峰值
    FilterMode_Static  = 1,    // 静态模式：窗口滤波，输出稳定重量
} FilterModeTypedef;

/* ---------- 对外接口 ---------- */
void Filter_Init(AdcDevNumTypedef DevNum);          // 初始化指定通道的滤波状态
int32_t Filter_AdcFilt(AdcDevNumTypedef DevNum, int32_t RawValue, FilterModeTypedef FilterMode);   // 执行一次滤波处理（FilterMode_Dynamic=动态透传，FilterMode_Static=静态窗口滤波）
// int32_t Filter_GetOutput(AdcDevNumTypedef DevNum);      // 获取当前滤波输出值

// uint8_t Filter_GetStaAnaFlag(AdcDevNumTypedef DevNum); // 获取稳定标志（1=数据稳定）
// void Filter_ClearStaFlag(AdcDevNumTypedef DevNum);// 设置稳定标志位的值
// void Filter_SetDepth(AdcDevNumTypedef DevNum, uint8_t depth); // 设置滤波深度档位（0~3）

#endif
