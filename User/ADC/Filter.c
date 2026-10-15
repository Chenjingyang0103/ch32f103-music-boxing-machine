/**
 *********************************************************************************
 * @file    Filter.c
 * @brief   ADC 滤波模块（当前为空实现）
 * @note    对外统一提供 Filter_AdcFilt 接口，滤波算法待后续补充。
 *********************************************************************************
 */

#include "Filter.h"
#include "HCT6521.h"
#include "Project_Config.h"
#include "debug.h"
#include "ch32f10x.h"
#include <string.h>

/* 静态数组，每个通道一份，外部无法访问 */
static FilterInfoTypedef FilterInfo[ADC_DEV_NUM];

/* ==================== 滤波深度参数表 ==================== */
/* s_filterDeep[深度][0] = 窗口总样本数
 * s_filterDeep[深度][1] = 累加起始索引（跳过排序后低端的样本个数）
 * s_filterDeep[深度][2] = 累加样本个数
 * s_filterDeep[深度][3] = 均值右移位数（等价除以 2^MoveNum）
 *
 * 累加区间：[起始索引, 起始索引 + 累加个数 - 1]
 * 累加个数均为 2 的幂，可用右移代替除法。
 */
static const uint8_t s_filterDeep[][4] =
{
    {8,   2,   1,   0},     // 窗口8，  从索引2开始，累加1个，不右移
    {15,  7,   2,   1},     // 窗口15，从索引7开始，累加2个，右移1位
    {20,  9,   2,   1},     // 窗口20，从索引9开始，累加2个，右移1位
    {20,  8,   4,   2},     // 窗口20，从索引8开始，累加4个，右移2位
};


/**
 * @brief  静态窗口滤波（去低端极值平均）——算法与 Calibration 的 Cal_WindowAver 一致
 * @param  DevNum    通道号
 * @param  NewValue  本次 ADC 采样原始值
 * @retval 滤波后的均值（已右移实现除法）
 * @note   动态 / 静态模式都需每拍调用，保证切静态时窗口内已是最新历史。
 */
static int32_t Filter_WindowAver(AdcDevNumTypedef DevNum, int32_t NewValue)
{
    uint8_t i, j;
    uint8_t iSet, jSet, kSet, MoveNum;
    int32_t tmp;
    uint8_t deep = FILTER_STATIC_DEEP;

    FilterInfoTypedef *p = &FilterInfo[DevNum];

    /* ---- 1. 从深度配置表读取参数 ---- */
    iSet    = s_filterDeep[deep][0] - 1;                          // 排序外循环次数 = 窗口大小 - 1
    jSet    = s_filterDeep[deep][0];                              // 窗口大小
    kSet    = s_filterDeep[deep][2] + s_filterDeep[deep][1];      // 累加结束索引（起始 + 个数）
    MoveNum = s_filterDeep[deep][3];                              // 右移位数

    /* ---- 2. 插入新值：小于上次均值放尾部，否则放头部 ---- */
    if (NewValue < p->WinAvg)
    {
        p->FilterArray[s_filterDeep[deep][0] - 1] = NewValue;
    }
    else
    {
        p->FilterArray[0] = NewValue;
    }

    /* ---- 3. 冒泡排序：窗口数组从小到大排序 ---- */
    for (i = 0; i < iSet; i++)
    {
        for (j = i + 1; j < jSet; j++)
        {
            if (p->FilterArray[i] > p->FilterArray[j])
            {
                tmp = p->FilterArray[i];
                p->FilterArray[i] = p->FilterArray[j];
                p->FilterArray[j] = tmp;
            }
        }
    }

    /* ---- 4. 跳过低端极值，取中间区间累加 ---- */
    p->WinAvg = 0;
    for (i = s_filterDeep[deep][1]; i < kSet; i++)
    {
        p->WinAvg += p->FilterArray[i];
    }
    /* 累加和右移实现除法，得到窗口均值（避免浮点运算） */
    p->WinAvg >>= MoveNum;

    return p->WinAvg;
}


/* ---------- 对外接口实现 ---------- */

/**
 * @brief  ADC 滤波：按 FilterMode 切换动态透传 / 静态窗口滤波
 * @param  DevNum      通道号
 * @param  RawValue    原始 ADC 值
 * @param  FilterMode  FilterMode_Dynamic=动态透传（保峰值），FilterMode_Static=静态窗口滤波
 * @retval 动态模式返回 RawValue，静态模式返回窗口滤波值
 * @note   无论何种模式都先喂入窗口滤波，保证切静态时窗口内已是最新历史。
 */
int32_t Filter_AdcFilt(AdcDevNumTypedef DevNum, int32_t RawValue, FilterModeTypedef FilterMode)
{
    int32_t staticVal = Filter_WindowAver(DevNum, RawValue);

    if (FilterMode == FilterMode_Static)
    {
        return staticVal;
    }
    return RawValue;
}

/**
* @brief 初始化指定通道的滤波状态（保留接口/空实现）
* @param DevNum 通道号
* @note  当前为空实现，仅保留接口；后续接入滤波算法时在此初始化状态结构体。
*/
void Filter_Init(AdcDevNumTypedef DevNum)
{
    memset(&FilterInfo[DevNum], 0, sizeof(FilterInfoTypedef));
    // memset(&FilterInfo[DevNum], 0, sizeof(FilterInfoTypedef));
}
