#ifndef __FILTER_H
#define __FILTER_H

#include "Project_Config.h"
#include "ch32f10x.h"

// /* ---------- 私有状态结构体（外部不可见） ---------- */
// typedef struct {
//     int32_t FilterArray[WINDOW_SIZE];
// } FilterInfoTypedef;

/* ---------- 对外接口 ---------- */
void Filter_Init(AdcDevNumTypedef DevNum);          // 初始化指定通道的滤波状态
int32_t Filter_AdcFilt(AdcDevNumTypedef DevNum, int32_t RawValue);           // 执行一次滤波处理（读取ADC并更新输出）
// int32_t Filter_GetOutput(AdcDevNumTypedef DevNum);      // 获取当前滤波输出值

// uint8_t Filter_GetStaAnaFlag(AdcDevNumTypedef DevNum); // 获取稳定标志（1=数据稳定）
// void Filter_ClearStaFlag(AdcDevNumTypedef DevNum);// 设置稳定标志位的值
// void Filter_SetDepth(AdcDevNumTypedef DevNum, uint8_t depth); // 设置滤波深度档位（0~3）

#endif
