/**
 *********************************************************************************
 * @file    PunchDetect.c
 * @brief   拳击击打检测状态机（阈值触发 → 确认 → 峰值追踪 → 松手上报）
 * @note    状态流转：Idle → Confirm → Track → Idle，击中事件由 Punch_IsHit 边沿上报。
 *********************************************************************************
 */

#include "PunchDetect.h"


static PunchDeviceParamTypedef PunchDeviceParam[ADC_DEV_NUM]; // Punch 设备参数数组

// /**
//  * @brief  获取指定通道的设备参数指针（仅供内部或调试使用）
//  * @param  DevNum  通道号
//  * @retval 指向该通道 PunchDeviceParam 结构体的常量指针
//  * @note   此函数未在 .h 中声明，当前仅供内部调试使用；建议加 static。
//  */
// const PunchDeviceParamTypedef *GetPunchDeviceParam(AdcDevNumTypedef DevNum)
// {
//     return &PunchDeviceParam[DevNum];
// }

// void PunchDeviceRunningParamInit(PunchRunningParamTypedef *pstInit, AdcDevNumTypedef emDevNum)
// {
//     memcpy(&PunchDeviceParam[emDevNum].RunningParam, pstInit, sizeof(PunchRunningParamTypedef) / sizeof(uint8_t));
// }

// void PunchDeviceStaticParamInit(PunchStaticParamTypedef *pstInit, AdcDevNumTypedef emDevNum)
// {
//     memcpy(&PunchDeviceParam[emDevNum].StaticParam, pstInit, sizeof(PunchRunningParamTypedef) / sizeof(uint8_t));
// }

/**
 * @brief 击打检测初始化：清空运行参数，设置阈值宏定义值
 * @param DevNum 通道号
 * @retval None
 * @note  阈值来自 Project_Config.h，可根据量程调整。
 */
void Punch_Init(AdcDevNumTypedef DevNum)
{
    // 获取当前通道的运行参数指针和静态参数指针
    PunchRunningParamTypedef *pRun = &PunchDeviceParam[DevNum].RunningParam;
    PunchStaticParamTypedef  *pSta = &PunchDeviceParam[DevNum].StaticParam;

    /* ---- 运行参数清零（每次重新开始或复位时需重置） ---- */
    pRun->state       = PunchState_Idle;
    pRun->PeakForce   = 0;       // 本次击打过程中的峰值力度（单位 g）
    pRun->Active      = 0;
    pRun->HitReported = 0;       // 本次击中事件是否已被外部读取上报（用于边沿触发）
    pRun->ReleaseCnt  = 0;
    pRun->StaticCnt   = 0;       // 连续超触发阈值计数清零
    pRun->FilterMode     = FilterMode_Dynamic;   // 滤波模式清零（动态）
    pRun->LastFilterMode = FilterMode_Dynamic;   // 松手锁存的滤波模式清零（动态）
    pRun->StaticWeight   = 0;    // 静态载稳定读数锁存清零

    /* ---- 静态阈值（可根据实际调试调整） ---- */
    pSta->TriggerThreshold  = TRIGGER_THRESHOLD;    // 击中阈值,重量大于阈值时认为有效击中（单位 g）
    pSta->ReleaseThreshold  = RELEASE_THRESHOLD;    // 松手复位门槛,力度回落到此值以下时，认为一次击打结束（单位 g）
    pSta->ReleaseCntLimit      = RELEASE_COUNT;     // 连续 N 点低于释放阈值判定松手
    pSta->StaticCntLimit       = STATIC_CNT_LIMIT;  // 连续 N 点超过触发阈值判定为静态加载
}

/**
 * @brief 复位状态机：清空所有状态和峰值，准备新一轮检测
 * @param DevNum 通道号
 * @retval None
 */
void Punch_Reset(AdcDevNumTypedef DevNum)
{
    PunchRunningParamTypedef *pRun = &PunchDeviceParam[DevNum].RunningParam;
    pRun->state       = PunchState_Idle;
    pRun->PeakForce   = 0;
    pRun->Active      = 0;
    pRun->HitReported = 0;
    pRun->ReleaseCnt  = 0;
    pRun->StaticCnt   = 0;
    pRun->FilterMode     = FilterMode_Dynamic;
    pRun->LastFilterMode = FilterMode_Dynamic;   // 松手锁存的滤波模式清零
    pRun->StaticWeight   = 0;    // 静态载稳定读数锁存清零
}

/**
 * @brief 状态机主函数，每周期调用一次
 * @param DevNum 通道号
 * @param force  当前重量（g）
 * @retval None
 */
void Punch_Tick(AdcDevNumTypedef DevNum, float force)
{
    PunchRunningParamTypedef *pRun = &PunchDeviceParam[DevNum].RunningParam;
    PunchStaticParamTypedef  *pSta = &PunchDeviceParam[DevNum].StaticParam;

    /* ---- 迟滞计数：连续超过触发阈值判定为静态加载 ---- */
    if (force > pSta->TriggerThreshold)
    {
        if (pRun->StaticCnt < 0xFF)
        {
            pRun->StaticCnt++;   // 上限保护，避免 uint8 溢出
        }
        if (pRun->StaticCnt >= pSta->StaticCntLimit)
        {
            if (pRun->FilterMode == FilterMode_Dynamic)
            {
                pRun->StaticWeight = force;   // 判定为静态加载的那一刻锁存稳定读数（g），之后不再更新，避免被移除时的下降值污染
            }
            pRun->FilterMode = FilterMode_Static;   // 判定为静态
        }
    }
    else if (force < pSta->ReleaseThreshold)
    {
        if (pRun->FilterMode == FilterMode_Static)
        {
            pRun->LastFilterMode = FilterMode_Static;   // 松手瞬间、清除 FilterMode 之前锁存：本次冲拳为静态载
        }
        pRun->StaticCnt  = 0;
        pRun->FilterMode = FilterMode_Dynamic;
    }
    /* 介于两阈值之间：迟滞，两者都保持不变 */

    switch (pRun->state)
    {
        case PunchState_Idle:
        {
            if (force > pSta->TriggerThreshold)
            {
                /* 第一个点超触发阈值，进 CONFIRM */
                pRun->state     = PunchState_Confirm;
                pRun->PeakForce = force;
                pRun->Active    = 1;
            }
            break;
        }
        case PunchState_Confirm:
        {
            if (force > pSta->TriggerThreshold)
            {
                /* 第二个点也超阈值 → 确认真实冲击，进 TRACK */
                if (force > pRun->PeakForce)
                {
                    pRun->PeakForce = force;
                }
                pRun->state      = PunchState_Track;
                pRun->ReleaseCnt = 0;
            }
            else
            {
                /* 第二个点回落 → 单点毛刺，丢弃，回 IDLE */
                pRun->state     = PunchState_Idle;
                pRun->PeakForce = 0;
                pRun->Active    = 0;
            }
            break;
        }

        case PunchState_Track:
        {
            if (force > pSta->ReleaseThreshold)
            {
                /* 仍在冲击中，更新峰值，清零释放计数 */
                if (force > pRun->PeakForce)
                {
                    pRun->PeakForce = force;
                }
                pRun->ReleaseCnt = 0;
            }
            else
            {
                /* 低于释放阈值，累计释放计数 */
                pRun->ReleaseCnt++;
                if (pRun->ReleaseCnt >= pSta->ReleaseCntLimit)
                {
                    /* 连续 N 点低于释放阈值 → 冲击结束 */
                    pRun->HitReported = 1;   // 上报事件
                    pRun->Active      = 0;
                    pRun->state       = PunchState_Idle;
                    pRun->ReleaseCnt  = 0;
                    /* PeakForce 保留，等外部读取后调用 Punch_ClearPeakForce */
                }
            }
            break;
        }

        /* 异常状态，强制回 IDLE */
        default:
        {
            pRun->state = PunchState_Idle;
            pRun->PeakForce  = 0;
            pRun->Active     = 0;
            pRun->ReleaseCnt = 0;
            break;
        }
    }
}

/**
 * @brief 检查击中事件（边沿触发，单次有效，读取后自动清零）
 * @param DevNum 通道号
 * @retval 1=有新击中，0=无
 */
uint8_t Punch_IsHit(AdcDevNumTypedef DevNum)
{
    PunchRunningParamTypedef *pRun = &PunchDeviceParam[DevNum].RunningParam;
    if (pRun->HitReported)
    {
        pRun->HitReported = 0;   // 清除上报标志，防止重复触发
        return 1;
    }
    return 0;
}

/**
 * @brief 是否处于冲击中（给 GameLogic 重置计时用）
 * @param DevNum 通道号
 * @retval 1=冲击中，0=空闲
 */
uint8_t Punch_IsActive(AdcDevNumTypedef DevNum)
{
    return PunchDeviceParam[DevNum].RunningParam.Active;
}

/**
 * @brief 清空峰值力度（准备新一轮击打）
 * @param DevNum 通道号
 * @retval None
 */
void Punch_ClearPeakForce(AdcDevNumTypedef DevNum)
{
    PunchDeviceParam[DevNum].RunningParam.PeakForce = 0;        // 清零峰值，准备记录新一轮击打
}

/**
 * @brief 获取本次击打的峰值力度（单位g），不清零
 * @param DevNum 通道号
 * @retval 峰值力度值（单位：g）
 */
float Punch_GetPeakForce(AdcDevNumTypedef DevNum)
{
    return PunchDeviceParam[DevNum].RunningParam.PeakForce;
}

/**
 * @brief 获取该通道当前滤波模式
 * @param DevNum 通道号
 * @retval FilterMode_Dynamic=动态，FilterMode_Static=静态；参数越界返回 FilterMode_Dynamic
 */
FilterModeTypedef Punch_GetFilterMode(AdcDevNumTypedef DevNum)
{
    if (DevNum >= ADC_DEV_NUM) return FilterMode_Dynamic;
    return PunchDeviceParam[DevNum].RunningParam.FilterMode;
}

/**
 * @brief 获取上次松手时锁存的滤波模式
 * @param DevNum 通道号
 * @retval FilterMode_Static=本次冲拳发生在静态载上，FilterMode_Dynamic=动态冲拳；参数越界返回 FilterMode_Dynamic
 * @note   松手瞬间 FilterMode 会被清成 Dynamic，故在清除前锁存到 LastFilterMode，
 *         供 GameLogic 在回合结束后判断显示稳定值还是峰值。
 */
FilterModeTypedef Punch_GetLastFilterMode(AdcDevNumTypedef DevNum)
{
    if (DevNum >= ADC_DEV_NUM) return FilterMode_Dynamic;
    return PunchDeviceParam[DevNum].RunningParam.LastFilterMode;
}

/**
 * @brief 获取锁存的静态载稳定读数
 * @param DevNum 通道号
 * @retval 静态载上的稳定读数（单位：g）；参数越界返回 0
 */
float Punch_GetStaticWeight(AdcDevNumTypedef DevNum)
{
    if (DevNum >= ADC_DEV_NUM) return 0;
    return PunchDeviceParam[DevNum].RunningParam.StaticWeight;
}
