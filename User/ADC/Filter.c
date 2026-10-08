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



// /* 静态数组，每个通道一份，外部无法访问 */
// static FilterInfoTypedef FilterInfo[ADC_DEV_NUM];



// /**
// * @brief  滑动窗口最大值滤波，自动剔除孤立毛刺
// * @param  DevNum    通道号
// * @param  NewValue  最新ADC原始值
// * @retval 滤波后的窗口最大值（已排除单点突跳）
// */
// static int32_t Filter_WindowMax(AdcDevNumTypedef DevNum, int32_t NewValue)
// {
//     int32_t limit = Filter_Get100kgValue(DevNum);
//     // 如果 NewValue 大于 limit(差不多100kg),则大概率是噪声, 直接滤除
//     if(NewValue >= limit)
//     {
//         NewValue = 0;
//     }
//     u8 i;
//     u8 WinSize = WINDOW_SIZE;                     // 窗口大小（采样点数）
//     FilterInfoTypedef *p = &FilterInfo[DevNum];       // 当前通道状态指针

//     /* 越界保护:窗口大小为 0 时直接返回错误码 */
//     if (WinSize == 0) {
//         return 0xFF;   // 返回错误值
//     }

//     /* -------- 1. 滑动窗口更新 -------- */
//     // 整体左移，丢弃最旧数据（FilterArray[0]），将新值放入队尾
//     for (i = 0; i < WinSize - 1; i++)
//     {
//         p->FilterArray[i] = p->FilterArray[i + 1];
//     }
//     p->FilterArray[WinSize - 1] = NewValue;

//     /* -------- 2. 求窗口内最大值和次大值 -------- */
//     // 初始化最大和次大为第一个元素，遍历比较更新
//     int32_t MaxVal, SecondVal;

//     /* 处理数组只有一个元素的情况 */
//     if (WinSize == 1) {
//         MaxVal = p->FilterArray[0];
//         SecondVal = p->FilterArray[0]; // 只有一个元素时，次大值只能等于最大值
//     }
//     else
//     {
//         /* 用前两个元素初始化最大值和次大值 */
//         if (p->FilterArray[0] >= p->FilterArray[1])
//         {
//             MaxVal = p->FilterArray[0];
//             SecondVal = p->FilterArray[1];
//         }
//         else
//         {
//             MaxVal = p->FilterArray[1];
//             SecondVal = p->FilterArray[0];
//         }

//         /* 从第三个元素开始遍历，更新最大值和次大值 */
//         for (i = 2; i < WinSize; i++)
//         {
//             if (p->FilterArray[i] > MaxVal)
//             {
//                 /* 当前元素比已知最大值还大，原最大值降为次大 */
//                 SecondVal = MaxVal;
//                 MaxVal = p->FilterArray[i];
//             }
//             else if (p->FilterArray[i] > SecondVal)
//             {
//                 /* 当前元素不大于最大值，但比当前次大值大，更新次大值 */
//                 SecondVal = p->FilterArray[i];
//             }
//         }
//     }

//     /* -------- 3. 孤立毛刺判别 -------- */
//     // 正常冲击：窗口内至少有两个超过阈值高值
//     // 孤立毛刺：仅单点异常高，最大与次大差距悬殊
//     // 此函数只是为了区分毛刺和正常数据,正常的冲击被设置成了连续三个值一样的值,而毛刺只有一个值
//     // 可以通过带有波形显示的串口调试助手看数据,实现滤除毛刺数据是直接在PunchDetect.c中处理的
//     // if (SecondVal > 0 && ((float)(MaxVal - SecondVal) * 100.0f / (float)(MaxVal)) > SPIKE_RATIO)
//     if (SecondVal > 0 && (MaxVal - SecondVal) > SPIKE_ABS_THRESHOLD)
//     {
//     // printf("{有毛刺}%d\n",SecondVal);
//        return SecondVal;    // 丢弃毛刺，返回次大值（即正常数据的最大值）
//     }
//    // 正常情况：直接返回峰值
//     return MaxVal;
// }


/* ---------- 对外接口实现 ---------- */

/**
 * @brief  ADC 滤波：当前为空实现，原样返回 RawValue（滤波算法待实现）
 * @param  DevNum    通道号
 * @param  RawValue  原始 ADC 值
 * @retval 原样返回 RawValue
 */
int32_t Filter_AdcFilt(AdcDevNumTypedef DevNum, int32_t RawValue)
{
    return RawValue;
}


/**
* @brief 初始化指定通道的滤波状态（保留接口/空实现）
* @param DevNum 通道号
* @note  当前为空实现，仅保留接口；后续接入滤波算法时在此初始化状态结构体。
*/
void Filter_Init(AdcDevNumTypedef DevNum)
{
    (void)DevNum;   // 当前为空实现，参数暂未使用
    // memset(&FilterInfo[DevNum], 0, sizeof(FilterInfoTypedef));
}
