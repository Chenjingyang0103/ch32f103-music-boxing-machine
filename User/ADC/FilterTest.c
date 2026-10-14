/**
 *********************************************************************************
 * @file    FilterTest.c
 * @brief   滤波稳定性测试模块（仅测试用，不参与检测 / 称重通道）
 * @note    - 算法与 Calibration.c 的 Cal_WindowAver 一致：
 *            新值插入 → 冒泡排序 → 跳过低端极值 → 中间区间累加 → 右移当除法
 *          - 窗口参数表与 Calibration.c 的 s_calFilterDeep 内容一致，本模块自持一份
 *          - 滤波状态本模块独立持有，与标定 / 检测通道互不影响
 *          - 非阻塞：由调用方（主循环）投喂 ADC 原始值，每次调用只做一次滤波
 *          - 窗口预热满一个窗口后才对外输出，避免前端未填满时的偏差
 *          - 可选按标定表把滤波值换算为重量（g），仅用于观察，不改动 Weight 模块
 *********************************************************************************
 */

#include "FilterTest.h"
#include "Calibration.h"    // 只读标定表：零点、各标定点差值、Rate
#include <string.h>

/* ==================== 窗口参数表 ==================== */
/* 与 Calibration.c 的 s_calFilterDeep 内容保持一致（4 档窗口配置）
 * s_filterTestDeep[深度][0] = 窗口总样本数
 * s_filterTestDeep[深度][1] = 累加起始索引（跳过排序后低端的样本个数）
 * s_filterTestDeep[深度][2] = 累加样本个数
 * s_filterTestDeep[深度][3] = 均值右移位数（等价除以 2^MoveNum）
 */
static const uint8_t s_filterTestDeep[][4] =
{
    {8,   2,   1,   0},     // 窗口8，  从索引2开始，累加1个，不右移
    {15,  7,   2,   1},     // 窗口15，从索引7开始，累加2个，右移1位
    {20,  9,   2,   1},     // 窗口20，从索引9开始，累加2个，右移1位
    {20,  8,   4,   2},     // 窗口20，从索引8开始，累加4个，右移2位
};

/* ==================== 重量换算标准值 ==================== */
/* 各标定点对应的标准重量值（单位：g），与 CalPointTypedef 的砝码点一一对应 */
static const float s_filterTestWeiCal[CAL_TIMES] =
{
    200.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f,
};

/* ==================== 测试专用滤波状态 ==================== */
/**
 * @brief  测试专用滤波状态（每通道一份）
 * @note   与 Calibration 的标定滤波状态相互独立，互不干扰
 */
typedef struct
{
    int32_t FilterArray[20];   // 窗口数组（最大窗口 20）
    int32_t WinAvg;            // 上次窗口均值，用于决定新值插入头部还是尾部
    uint8_t WarmCount;         // 预热计数，达到一个窗口大小后才对外输出
    int32_t LastValue;         // 最近一次滤波输出
    int32_t ZeroAdc;           // 零点（开机实测空载值），重量换算基准
} FilterTestInfoTypedef;

static FilterTestInfoTypedef s_filterTest[ADC_DEV_NUM];

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief  抗脉冲加权均值滤波（去低端极值平均）——算法与 Calibration 的 Cal_WindowAver 一致
 * @param  DevNum    通道号
 * @param  NewValue  本次 ADC 采样原始值
 * @retval 滤波后的均值（已右移实现除法）
 * @note   仅用于测试观察。窗口内若只有 1~2 个真实峰值，会被排序后排到高端，
 *         累加区间取不到，峰值会被削掉，因此不可用于检测通道。
 */
static int32_t FilterTest_WindowAver(AdcDevNumTypedef DevNum, int32_t NewValue)
{
    uint8_t i, j;
    uint8_t iSet, jSet, kSet, MoveNum;
    int32_t tmp;
    uint8_t deep = FILTERTEST_DEEP;

    FilterTestInfoTypedef *p = &s_filterTest[DevNum];

    /* ---- 1. 从深度配置表读取参数 ---- */
    iSet    = s_filterTestDeep[deep][0] - 1;                              // 排序外循环次数 = 窗口大小 - 1
    jSet    = s_filterTestDeep[deep][0];                                  // 窗口大小
    kSet    = s_filterTestDeep[deep][2] + s_filterTestDeep[deep][1];      // 累加结束索引（起始 + 个数）
    MoveNum = s_filterTestDeep[deep][3];                                  // 右移位数

    /* ---- 2. 插入新值：小于上次均值放尾部，否则放头部 ---- */
    if (NewValue < p->WinAvg)
    {
        p->FilterArray[s_filterTestDeep[deep][0] - 1] = NewValue;
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
    for (i = s_filterTestDeep[deep][1]; i < kSet; i++)
    {
        p->WinAvg += p->FilterArray[i];
    }
    /* 累加和右移实现除法，得到窗口均值（避免浮点运算） */
    p->WinAvg >>= MoveNum;

    return p->WinAvg;
}

/* ==================== 对外接口实现 ==================== */

/**
 * @brief  复位指定通道的测试滤波状态
 * @param  DevNum  通道号
 * @retval None
 */
void FilterTest_Init(AdcDevNumTypedef DevNum)
{
    if (DevNum >= ADC_DEV_NUM) return;
    memset(&s_filterTest[DevNum], 0, sizeof(FilterTestInfoTypedef));
}

/**
 * @brief  复位全部通道的测试滤波状态
 * @param  无
 * @retval None
 */
void FilterTest_InitAll(void)
{
    for (uint8_t i = 0; i < ADC_DEV_NUM; i++)
    {
        FilterTest_Init((AdcDevNumTypedef)i);
    }
}

/**
 * @brief  设置某通道零点
 * @param  DevNum    通道号
 * @param  ZeroValue 新的零点值（开机实测空载的滤波值）
 * @retval None
 * @note   仅作为测试模块自身的重量换算基准，不写 Weight 模块状态。
 *         需在 FilterTest_Init / FilterTest_InitAll 之后调用，否则会被清零覆盖。
 */
void FilterTest_SetZero(AdcDevNumTypedef DevNum, int32_t ZeroValue)
{
    if (DevNum >= ADC_DEV_NUM) return;
    s_filterTest[DevNum].ZeroAdc = ZeroValue;
}

/**
 * @brief  投喂一个 ADC 原始值并输出滤波结果（非阻塞）
 * @param  DevNum    通道号
 * @param  RawValue  本次 ADC 采样原始值（由调用方从 HCT6521_Task 取得）
 * @param  OutValue  输出参数，预热完成后填入本次滤波值
 * @retval 1=本次产出有效滤波值，0=仍在预热或参数越界
 * @note   本函数不自行读取 ADC，避免与主循环的 HCT6521_Task 状态机重复采样、
 *         打乱采样节拍；由调用方把已读到的原始值投喂进来。
 */
uint8_t FilterTest_Feed(AdcDevNumTypedef DevNum, int32_t RawValue, int32_t *OutValue)
{
    FilterTestInfoTypedef *p;
    int32_t filtered;
    uint8_t win;

    if (DevNum >= ADC_DEV_NUM) return 0;

    p   = &s_filterTest[DevNum];
    win = s_filterTestDeep[FILTERTEST_DEEP][0];

    filtered = FilterTest_WindowAver(DevNum, RawValue);
    p->LastValue = filtered;

    /* 窗口预热：填满一个窗口后才对外输出有效值 */
    if (p->WarmCount < win)
    {
        p->WarmCount++;
        return 0;
    }

    if (OutValue != NULL) *OutValue = filtered;
    return 1;
}

/**
 * @brief  获取最近一次滤波输出
 * @param  DevNum  通道号
 * @retval 最近一次滤波值；参数越界返回 0
 */
int32_t FilterTest_GetValue(AdcDevNumTypedef DevNum)
{
    if (DevNum >= ADC_DEV_NUM) return 0;
    return s_filterTest[DevNum].LastValue;
}

/**
 * @brief  将最近一次滤波值换算为重量（单位：g）
 * @param  DevNum  通道号
 * @retval 重量（g，已乘 Rate）；参数越界返回 0
 * @note   换算基准（零点）由 FilterTest_SetZero 设置（开机实测空载值），
 *         与 Weight 模块零点同源，故上电空载时重量约为 0。
 *         分段线性插值逻辑与 Weight_Update 一致：
 *           - diff <= 第0点：按第 0 点比例折算
 *           - 落在标定点之间：线性插值
 *           - 超过最大标定点：按零点到最大标定点的平均斜率外推
 *         仅测试观察用，不写 Weight 模块状态，二者互不干扰。
 */
float FilterTest_GetWeight(AdcDevNumTypedef DevNum)
{
    int32_t diff;                       // 当前滤波值相对零点的差分
    int32_t d[CAL_TIMES];               // 各标定点相对零点的差值
    float   wei;                        // 插值得到的重量（未乘 Rate）

    if (DevNum >= ADC_DEV_NUM) return 0.0f;

    /* 差分 = 滤波值 - 零点（由 FilterTest_SetZero 设置） */
    diff = s_filterTest[DevNum].LastValue - s_filterTest[DevNum].ZeroAdc;

    /* 从 Calibration 只读各标定点差值 */
    d[0] = Cal_GetDiff(DevNum, CalPoint_200g);
    d[1] = Cal_GetDiff(DevNum, CalPoint_500g);
    d[2] = Cal_GetDiff(DevNum, CalPoint_1000g);
    d[3] = Cal_GetDiff(DevNum, CalPoint_2000g);
    d[4] = Cal_GetDiff(DevNum, CalPoint_4000g);

    /* 防单点除零（与 Weight_Init 的处理一致） */
    for (uint8_t i = 0; i < CAL_TIMES; i++)
    {
        if (d[i] == 0) d[i] = 1;
    }

    /* ===== 分段线性插值（与 Weight_Update 同算法） ===== */
    if (diff <= d[0])
    {
        wei = s_filterTestWeiCal[0] / d[0] * diff;
    }
    else if (diff <= d[1])
    {
        wei = s_filterTestWeiCal[0] +
              (s_filterTestWeiCal[1] - s_filterTestWeiCal[0]) / (d[1] - d[0]) *
              (diff - d[0]);
    }
    else if (diff <= d[2])
    {
        wei = s_filterTestWeiCal[1] +
              (s_filterTestWeiCal[2] - s_filterTestWeiCal[1]) / (d[2] - d[1]) *
              (diff - d[1]);
    }
    else if (diff <= d[3])
    {
        wei = s_filterTestWeiCal[2] +
              (s_filterTestWeiCal[3] - s_filterTestWeiCal[2]) / (d[3] - d[2]) *
              (diff - d[2]);
    }
    else if (diff <= d[4])
    {
        wei = s_filterTestWeiCal[3] +
              (s_filterTestWeiCal[4] - s_filterTestWeiCal[3]) / (d[4] - d[3]) *
              (diff - d[3]);
    }
    else
    {
        /* 超过最大标定点：按零点到最大标定点的整体平均斜率外推 */
        wei = diff * s_filterTestWeiCal[CAL_TIMES - 1] / d[CAL_TIMES - 1];
    }

    return wei * Cal_GetRate(DevNum);
}
