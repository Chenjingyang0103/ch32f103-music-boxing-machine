/**
 *********************************************************************************
 * @file    Weight.c
 * @brief   称重转换模块（ADC 差分 → 分段线性插值 → 重量）
 * @note    标定差值从 Calibration 模块读取，重量结果为未乘 Rate 的原始值，
 *          对外通过 Weight_GetValue 输出乘上 Rate 后的重量（单位：g）。
 *********************************************************************************
 */

#include "Weight.h"
#include "Filter.h"
#include "HCT6521.h"
#include "Calibration.h"
#include "MyFlash.h"
#include "debug.h"
#include <string.h>


/* 静态数组，每个通道一份，外部无法访问 */
static WeightInfoTypedef WeightInfo[ADC_DEV_NUM];

/**
 * @brief  称重转换：差分 → 分段线性插值 → 重量（g）
 * @param  DevNum        通道号
 * @param  AdFilterValue 当前滤波后的 ADC 值
 * @note   使用 5 个标定点线性插值：
 *         - DiffValue <= DiffCalValue[0]：按第 0 点比例折算
 *         - 落在标定点之间：线性插值
 *         - 超过最大标定点：按零点到最大标定点的平均斜率外推
 *         结果存入 p->WeiValue（未乘 Rate），零点、皮重等需外部处理。
 */
void Weight_Update(AdcDevNumTypedef DevNum, int32_t AdFilterValue)
{
    WeightInfoTypedef *p = &WeightInfo[DevNum];

    // 计算差分（使用自己的零点）
    p->DiffValue = AdFilterValue - p->ZeroAdc;

    /* ===== 分段线性校准 ===== */
    // 根据当前 AD 值所在的区间，选择对应的线性插值公式
    if (p->DiffValue <= p->DiffCalValue[0])
    {
        // 低于或等于第0个标定点：按比例折算
        // 重量y = 斜率k * 变量x
        p->WeiValue = p->WeiCalValue[0] / p->DiffCalValue[0] * // 斜率k
                      p->DiffValue; // 变量x
    }
    else if (p->DiffValue <= p->DiffCalValue[1])
    {
        // 在第0~1标定点之间：线性插值
        // 重量y = 重量值b + 斜率k * 变量x
        p->WeiValue = p->WeiCalValue[0] + // 重量值b
                    (p->WeiCalValue[1] - p->WeiCalValue[0]) / (p->DiffCalValue[1] - p->DiffCalValue[0]) * // 斜率k
                    (p->DiffValue - p->DiffCalValue[0]); // 变量x
    }
    else if (p->DiffValue <= p->DiffCalValue[2])
    {
        // 在第1~2标定点之间：线性插值
        // 重量y = 重量值b + 斜率k * 变量x
        p->WeiValue = p->WeiCalValue[1] + // 重量值b
                    (p->WeiCalValue[2] - p->WeiCalValue[1]) / (p->DiffCalValue[2] - p->DiffCalValue[1]) * // 斜率k
                    (p->DiffValue - p->DiffCalValue[1]); // 变量x
    }
    else if (p->DiffValue <= p->DiffCalValue[3])
    {
        // 在第2~3标定点之间：线性插值
        // 重量y = 重量值b + 斜率k * 变量x
        p->WeiValue = p->WeiCalValue[2] + // 重量值b
                    (p->WeiCalValue[3] - p->WeiCalValue[2]) / (p->DiffCalValue[3] - p->DiffCalValue[2]) * // 斜率k
                    (p->DiffValue - p->DiffCalValue[2]); // 变量x
    }
    else if (p->DiffValue <= p->DiffCalValue[4])
    {
        // 在第3~4标定点之间：线性插值
        // 重量y = 重量值b + 斜率k * 变量x
        p->WeiValue = p->WeiCalValue[3] + // 重量值b
                    (p->WeiCalValue[4] - p->WeiCalValue[3]) / (p->DiffCalValue[4] - p->DiffCalValue[3]) * // 斜率k
                    (p->DiffValue - p->DiffCalValue[3]); // 变量x
    }
    else
    {
        // 超过最大校准点：使用零点到最大标定点的整体平均斜率
        // 重量y = 斜率k * 变量x
        p->WeiValue = p->DiffValue * p->WeiCalValue[CAL_TIMES - 1] / p->DiffCalValue[CAL_TIMES - 1];
    }
}

/**
 * @brief  称重模块初始化：清零状态、设置零点、从 Calibration 加载标定差值
 * @param  DevNum        通道号
 * @param  AdStableValue 当前滤波后的 ADC 值（作为零点）
 * @note   标定差值直接从 Calibration 模块读取：
 *             DiffCalValue[i] = Cal_GetDiff(DevNum, CalPoint_200g ~ 4000g)
 *         Rate 也直接从 Calibration 读取。
 * @warning 调用前需确保 Cal_Init 已执行，标定表已加载。
 */
void Weight_Init(AdcDevNumTypedef DevNum, int32_t AdStableValue)
{
    WeightInfoTypedef *p = &WeightInfo[DevNum];
    memset(p, 0, sizeof(WeightInfoTypedef));

    /* 记录零点 */
    p->ZeroAdc = AdStableValue;

    /* ---- 从 Calibration 读取各标定点差值 ---- */
    p->DiffCalValue[0] = Cal_GetDiff(DevNum, CalPoint_200g);
    p->DiffCalValue[1] = Cal_GetDiff(DevNum, CalPoint_500g);
    p->DiffCalValue[2] = Cal_GetDiff(DevNum, CalPoint_1000g);
    p->DiffCalValue[3] = Cal_GetDiff(DevNum, CalPoint_2000g);
    p->DiffCalValue[4] = Cal_GetDiff(DevNum, CalPoint_4000g);

    /* 防单点除零 */
    for (int i = 0; i < CAL_TIMES; i++)
    {
        if (p->DiffCalValue[i] == 0)
        {
            p->DiffCalValue[i] = 1;
        }
    }

    /* 各标定点对应的标准重量值（单位：g） */
    p->WeiCalValue[0] = 200.0f;
    p->WeiCalValue[1] = 500.0f;
    p->WeiCalValue[2] = 1000.0f;
    p->WeiCalValue[3] = 2000.0f;
    p->WeiCalValue[4] = 4000.0f;

    /* 从 Calibration 读取 Rate */
    p->Rate = Cal_GetRate(DevNum);
}

/**
 * @brief 获取当前通道的重量值（单位：g）
 * @param DevNum 通道号
 * @retval 当前重量（浮点数，单位：g，已乘 Rate 修正系数）
 */
float Weight_GetValue(AdcDevNumTypedef DevNum)
{
    WeightInfoTypedef *p = &WeightInfo[DevNum];
    return p->WeiValue * p->Rate;
}

/**
 * @brief 手动置零：将当前ADC值设为零点，重量归零
 * @param DevNum    通道号
 * @param ZeroValue 新的零点值（当前滤波输出的ADC码值）
 * @note  由用户按键触发，与自动零点跟踪不同。
 */
void Weight_SetZero(AdcDevNumTypedef DevNum, int32_t ZeroValue)
{
    WeightInfo[DevNum].ZeroAdc = ZeroValue;   // 更新自己的零点
    WeightInfo[DevNum].WeiValue = 0;
}

/**
 * @brief 获取当前AD差分值（调试用）：滤波输出 - 零点
 * @param DevNum 通道号
 * @retval 未校准的原始差分值（单位ADC码值）
 */
int32_t Weight_GetAdcDiff(AdcDevNumTypedef DevNum)
{
    return WeightInfo[DevNum].DiffValue;
}

/**
 * @brief 获取当前零点值（ADC码值）
 * @param DevNum 通道号
 * @retval 零点ADC值
 */
int32_t Weight_GetZeroValue(AdcDevNumTypedef DevNum)
{
    return WeightInfo[DevNum].ZeroAdc;
}
