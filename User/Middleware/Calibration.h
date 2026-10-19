#ifndef __CALIBRATION_H
#define __CALIBRATION_H

#include "ch32f10x.h"
#include "Project_Config.h"

/* ==================== 稳定判定默认参数 ==================== */
#define CAL_FILTER_DEEP          1       // 标定滤波深度档位（0~3）
#define CAL_STABLE_THRESHOLD     20      // 稳定阈值：连续两次滤波输出变化小于此值认为稳定
#define CAL_STABLE_CONFIRM_CNT   3       // 连续稳定次数，达到此值认为标定值可用
#define CAL_STABLE_TIMEOUT_MS    3000    // 稳定判定超时时间（毫秒）
#define CAL_RATE_MAX             10.0f   // Rate 上限，超过视为标定异常

/* ==================== 标定点枚举 ==================== */
/* 索引与标定表列一一对应：0=零点，1~5=砝码点 */
typedef enum
{
    CalPoint_Zero = 0,      // 零点（空载）
    CalPoint_200g,          // 200g
    CalPoint_500g,          // 500g
    CalPoint_1000g,         // 1000g
    CalPoint_2000g,         // 2000g
    CalPoint_4000g,         // 4000g
    CalPoint_Num            // 标定点总数（= ADC_CAL_POINTS）
} CalPointTypedef;

/* ==================== 对外接口 ==================== */

void Cal_Init(void);                                                          // 初始化标定模块（加载 Flash 或使用默认值）

uint8_t Cal_WaitStable(AdcDevNumTypedef DevNum, int32_t *OutValue);           // 等待 ADC 稳定，成功返回 1 并输出稳定值，超时返回 0

uint8_t Cal_SetZero(AdcDevNumTypedef DevNum);                                 // 标定零点：等待稳定后写入标定表第 0 列
uint8_t Cal_SetPoint(AdcDevNumTypedef DevNum, CalPointTypedef Point);         // 标定砝码点：等待稳定后写入对应列

uint8_t Cal_Save(void);                                                       // 将标定表保存到 Flash
uint8_t Cal_Load(void);                                                       // 从 Flash 加载标定表

int32_t Cal_GetAdc(AdcDevNumTypedef DevNum, CalPointTypedef Point);           // 获取某通道某标定点的 ADC 值
int32_t Cal_GetDiff(AdcDevNumTypedef DevNum, CalPointTypedef Point);          // 获取某通道某标定点相对零点的 ADC 差值
uint8_t Cal_IsValid(AdcDevNumTypedef DevNum);                                 // 判断某通道是否已完成全部标定

uint8_t Cal_SetRate(AdcDevNumTypedef DevNum, CalPointTypedef Point);           // 单点标定：放砝码后等待稳定，计算并保存 Rate
float   Cal_GetRate(AdcDevNumTypedef DevNum);                                 // 获取某通道当前 Rate

#endif /* __CALIBRATION_H */
