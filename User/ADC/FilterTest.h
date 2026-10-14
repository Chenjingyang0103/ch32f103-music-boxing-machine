#ifndef __FILTERTEST_H
#define __FILTERTEST_H

#include "Project_Config.h"
#include "ch32f10x.h"

/* ==================== 测试模块参数 ==================== */
/* 滤波深度档位（0~3），与 Calibration 的 CAL_FILTER_DEEP 语义一致：
 *   0=窗口8 /取1点 /不右移
 *   1=窗口15/取2点 /右移1位
 *   2=窗口20/取2点 /右移1位
 *   3=窗口20/取4点 /右移2位
 */
#define FILTERTEST_DEEP     1

/* ==================== 对外接口 ==================== */

void    FilterTest_Init(AdcDevNumTypedef DevNum);                        // 复位指定通道的测试滤波状态
void    FilterTest_InitAll(void);                                        // 复位全部通道的测试滤波状态

/* 投喂一个 ADC 原始值（非阻塞，一次只做一次滤波）。
 * 返回 1 表示窗口已预热满、本次产出有效滤波值（*OutValue 有效）；
 * 返回 0 表示仍在预热、或参数越界，*OutValue 不更新。 */
uint8_t FilterTest_Feed(AdcDevNumTypedef DevNum, int32_t RawValue, int32_t *OutValue);

int32_t FilterTest_GetValue(AdcDevNumTypedef DevNum);                    // 获取最近一次滤波输出

/* 将最近一次滤波值换算为重量（单位：g，已乘 Rate，保留两位小数）。
 * 零点由 FilterTest_SetZero 设置（开机实测空载值），插值算法与 Weight_Update 一致；
 * 仅测试用，不改动 Weight 模块状态。 */
float   FilterTest_GetWeight(AdcDevNumTypedef DevNum);

/* 设置某通道零点（开机实测空载值），作为重量换算基准 */
void    FilterTest_SetZero(AdcDevNumTypedef DevNum, int32_t ZeroValue);

#endif /* __FILTERTEST_H */
