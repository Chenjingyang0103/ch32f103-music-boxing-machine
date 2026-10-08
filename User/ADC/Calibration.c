/**
 *********************************************************************************
 * @file    Calibration.c
 * @brief   标定模块：持有标定表，提供稳定判定、标定点设置、查询接口
 * @note    - 标定表 s_cal[ADC_DEV_NUM][ADC_CAL_POINTS]：0=零点，1~5=砝码点
 *          - 倍率表 s_rate[ADC_DEV_NUM]：单点标定得出的整体增益
 *          - 与 MyFlash 同步：Cal_Load / Cal_Save
 *          - 稳定判定使用窗口滤波（去低端极值平均）+ 连续稳定确认，
 *            仅在标定时调用，不影响检测通道
 *********************************************************************************
 */

#include "Calibration.h"
#include "HCT6521.h"
#include "MyFlash.h"
#include "debug.h"
#include <string.h>


/* ==================== 滤波深度参数表 ==================== */
/* s_calFilterDeep[深度][0] = 窗口总样本数
 * s_calFilterDeep[深度][1] = 累加起始索引（跳过排序后低端的样本个数）
 * s_calFilterDeep[深度][2] = 累加样本个数
 * s_calFilterDeep[深度][3] = 均值右移位数（等价除以 2^MoveNum）
 *
 * 累加区间：[起始索引, 起始索引 + 累加个数 - 1]
 * 累加个数均为 2 的幂，可用右移代替除法。
 */
static const uint8_t s_calFilterDeep[][4] =
{
    {8,   2,   1,   0},     // 窗口8，  从索引2开始，累加1个，不右移
    {15,  7,   2,   1},     // 窗口15，从索引7开始，累加2个，右移1位
    {20,  9,   2,   1},     // 窗口20，从索引9开始，累加2个，右移1位
    {20,  8,   4,   2},     // 窗口20，从索引8开始，累加4个，右移2位
};


/* ==================== 标定专用滤波状态 ==================== */
/**
 * @brief  标定专用滤波状态（每通道一份）
 * @note   仅用于标定时的稳定取值，不参与检测通道
 */
typedef struct
{
    int32_t FilterArray[20];   // 窗口数组（最大窗口 20）
    int32_t WinAvg;            // 上次窗口均值，用于决定新值插入头部还是尾部
    uint8_t Deep;              // 深度档位（0~3）
} CalFilterInfoTypedef;

static CalFilterInfoTypedef s_calFilter[ADC_DEV_NUM];


/* ==================== 内部持有的标定表 ==================== */
static int32_t s_cal[ADC_DEV_NUM][ADC_CAL_POINTS];
static float   s_rate[ADC_DEV_NUM];

/* 各通道标定有效性标志（1=已标定） */
static uint8_t s_valid[ADC_DEV_NUM];


/* ==================== 默认标定表 ==================== */
/* 行索引 = AdcDevNumTypedef，列索引 = CalPointTypedef */
static const int32_t s_defaultCal[ADC_DEV_NUM][ADC_CAL_POINTS] = {
    /* 空载    200g    500g    1000g   2000g   4000g */
    {2681, 4538, 7328, 11966, 21251, 39821},   // AdcDevNum0
    {3646, 3784, 4045, 4394, 4849, 5759},      // AdcDevNum1
    {0, 0, 0, 0, 0, 0},                         // AdcDevNum2（未标定）
    {0, 0, 0, 0, 0, 0},                         // AdcDevNum3（未标定）
    {0, 0, 0, 0, 0, 0},                         // AdcDevNum4（未标定）
    {0, 0, 0, 0, 0, 0},                         // AdcDevNum5（未标定）
};

static const float s_defaultRate[ADC_DEV_NUM] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};


/* ==================== 内部辅助函数 ==================== */

/**
 * @brief  从 ADC 阻塞读取一个原始值（带超时保护）
 * @param  DevNum    通道号
 * @param  OutValue  输出参数，读取到的原始 ADC 值
 * @retval 1=成功，0=超时
 * @note   内部循环调用 HCT6521_Task，直到状态机完成一次读取。
 *         会阻塞主循环，仅在标定场景下调用。
 *         超时阈值 20000 次调用，防止 SDO 异常时死循环。
 */
static uint8_t Cal_ReadAdc(AdcDevNumTypedef DevNum, int32_t *OutValue)
{
    uint32_t timeout = 0;

    while (!HCT6521_Task(DevNum, OutValue))
    {
        timeout++;
        if (timeout > 20000)
            return 0;   /* 超时，SDO 可能异常 */
    }
    return 1;
}


/**
 * @brief  抗脉冲加权均值滤波（去低端极值平均）
 * @param  DevNum    通道号
 * @param  NewValue  本次 ADC 采样原始值
 * @retval 滤波后的均值（已右移实现除法）
 * @note   算法流程：
 *         1. 从 s_calFilterDeep 读取窗口大小、累加起始索引、累加个数、右移位数
 *         2. 将新值插入数组头部或尾部（与上次均值比较，减少排序比较次数）
 *         3. 冒泡排序（从小到大）
 *         4. 跳过排序后低端的若干样本，对中间部分求和并右移得到均值
 * @warning 仅用于标定场景。窗口内若只有 1~2 个真实峰值，会被排序后
 *          排到高端，累加区间取不到，峰值会被削掉。不可用于检测通道。
 */
static int32_t Cal_WindowAver(AdcDevNumTypedef DevNum, int32_t NewValue)
{
    uint8_t i, j;
    uint8_t iSet, jSet, kSet, MoveNum;
    int32_t tmp;

    CalFilterInfoTypedef *p = &s_calFilter[DevNum];

    /* ---- 1. 从深度配置表读取参数 ---- */
    iSet    = s_calFilterDeep[p->Deep][0] - 1;                          // 排序外循环次数 = 窗口大小 - 1
    jSet    = s_calFilterDeep[p->Deep][0];                              // 窗口大小
    kSet    = s_calFilterDeep[p->Deep][2] + s_calFilterDeep[p->Deep][1];  // 累加结束索引（起始 + 个数）
    MoveNum = s_calFilterDeep[p->Deep][3];                              // 右移位数

    /* ---- 2. 插入新值：小于上次均值放尾部，否则放头部 ---- */
    if (NewValue < p->WinAvg)
    {
        p->FilterArray[s_calFilterDeep[p->Deep][0] - 1] = NewValue;
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
    for (i = s_calFilterDeep[p->Deep][1]; i < kSet; i++)
    {
        p->WinAvg += p->FilterArray[i];
    }
    /* 累加和右移实现除法，得到窗口均值（避免浮点运算） */
    p->WinAvg >>= MoveNum;

    return p->WinAvg;
}


/* ==================== 对外接口实现 ==================== */

/**
 * @brief  初始化标定模块：用默认值填充，然后尝试从 Flash 加载
 * @param  无
 * @retval None
 * @note   1. 先用 s_defaultCal / s_defaultRate 填充 s_cal / s_rate
 *         2. 调用 MyFlash_Init：
 *            - Flash 有数据 → 覆盖 s_cal / s_rate
 *            - Flash 空白   → 把当前默认值写入 Flash
 *         3. 更新各通道有效性标志 s_valid
 */
void Cal_Init(void)
{
    /* 1. 用默认值填充内部标定表 */
    memcpy(s_cal, s_defaultCal, sizeof(s_cal));
    memcpy(s_rate, s_defaultRate, sizeof(s_rate));

    /* 2. 尝试从 Flash 加载；若 Flash 空白，则把默认值写入 Flash */
    if (MyFlash_Init((uint32_t *)s_cal, s_rate))
    {
        printf("Calibration: Flash 加载成功\r\n");
    }
    else
    {
        printf("Calibration: Flash 加载失败，使用默认标定表\r\n");
    }

    /* 3. 更新各通道有效性标志 */
    for (uint8_t i = 0; i < ADC_DEV_NUM; i++)
    {
        s_valid[i] = Cal_IsValid((AdcDevNumTypedef)i);
        printf("Calibration: 通道 %d 有效性 = %d\r\n", i, s_valid[i]);
    }
}


/**
 * @brief  等待 ADC 稳定
 * @param  DevNum    通道号
 * @param  OutValue  输出参数，稳定后的 ADC 值（窗口滤波输出）
 * @retval 1=稳定，0=超时或参数错误
 * @note   流程：
 *         1. 初始化本通道滤波状态，深度设为 CAL_FILTER_DEEP
 *         2. 循环读取 ADC 原始值，送入 Cal_WindowAver 得到滤波输出
 *         3. 当连续 CAL_STABLE_CONFIRM_CNT 次滤波输出变化小于
 *            CAL_STABLE_THRESHOLD 时，认为稳定，输出最后一次滤波值
 *         4. 总时长超过 CAL_STABLE_TIMEOUT_MS 则超时返回 0
 *         仅在标定场景下调用，会阻塞主循环。
 *         ADC 数据率 20Hz，每点约 50ms。
 */
uint8_t Cal_WaitStable(AdcDevNumTypedef DevNum, int32_t *OutValue)
{
    int32_t  filtered      = 0;   // 本次窗口滤波输出
    int32_t  last_filtered = 0;   // 上一次窗口滤波输出，用于计算变化量
    int32_t  raw           = 0;   // 本次读取的原始 ADC 值
    uint8_t  stable_cnt    = 0;   // 连续稳定计数器，达到 CAL_STABLE_CONFIRM_CNT 判定稳定
    uint16_t total_cnt     = 0;   // 已循环次数，用于超时判断
    uint16_t max_total;           // 最大循环次数 = 预热次数 + 稳定判定次数
    uint8_t  warmup_cnt    = 0;   // 窗口预热计数，填满一个窗口后才开始判稳定
    uint8_t  warmup_target;       // 预热目标：窗口大小

    if (OutValue == NULL) return 0;
    if (DevNum >= ADC_DEV_NUM) return 0;

    /* 初始化本通道滤波状态 */
    memset(&s_calFilter[DevNum], 0, sizeof(CalFilterInfoTypedef));
    s_calFilter[DevNum].Deep = CAL_FILTER_DEEP;

    /* 预热目标 = 窗口大小；稳定判定预算 = CAL_STABLE_TIMEOUT_MS / 50 */
    warmup_target = s_calFilterDeep[CAL_FILTER_DEEP][0];
    max_total     = warmup_target + (CAL_STABLE_TIMEOUT_MS / 50);
    if (max_total <= warmup_target) max_total = warmup_target + 1;

    while (total_cnt < max_total)
    {
        /* 读一个原始值 */
        if (!Cal_ReadAdc(DevNum, &raw))
            return 0;   /* ADC 读取超时，直接失败 */

        /* 窗口滤波 */
        filtered = Cal_WindowAver(DevNum, raw);

        /* ---- 窗口预热：填满一个窗口后再判稳定 ---- */
        if (warmup_cnt < warmup_target)
        {
            warmup_cnt++;
            last_filtered = filtered;
            total_cnt++;
            continue;
        }

        /* ---- 连续稳定判定：两次滤波输出之差小于阈值，视为本次稳定 ---- */
        if (stable_cnt > 0 &&
            (filtered - last_filtered) < CAL_STABLE_THRESHOLD &&
            (last_filtered - filtered) < CAL_STABLE_THRESHOLD)
        {
            stable_cnt++;
            if (stable_cnt >= CAL_STABLE_CONFIRM_CNT)
            {
                *OutValue = filtered;   // 输出稳定后的滤波值
                return 1;
            }
        }
        else
        {
            stable_cnt = 1;             // 本次不稳定，重新开始计数
        }

        last_filtered = filtered;       // 保存本次输出，供下次比较
        total_cnt++;
    }

    return 0;   /* 超时 */
}
/**
 * @brief  标定零点：等待 ADC 稳定后写入标定表第 0 列
 * @param  DevNum  通道号
 * @retval 1=成功，0=失败（超时或参数错误）
 * @note   成功后更新该通道的有效性标志。
 */
uint8_t Cal_SetZero(AdcDevNumTypedef DevNum)
{
    int32_t stable_val;

    if (DevNum >= ADC_DEV_NUM) return 0;

    if (!Cal_WaitStable(DevNum, &stable_val))
        return 0;   /* 超时 */

    s_cal[DevNum][CalPoint_Zero] = stable_val;
    s_valid[DevNum] = Cal_IsValid(DevNum);   /* 更新有效性 */
    return 1;
}


/**
 * @brief  标定砝码点：等待 ADC 稳定后写入对应列
 * @param  DevNum  通道号
 * @param  Point   标定点（CalPoint_200g ~ CalPoint_4000g）
 * @retval 1=成功，0=失败（超时或参数错误）
 * @note   若 Point == CalPoint_Zero，内部转调 Cal_SetZero。
 *         成功后更新该通道的有效性标志。
 */
uint8_t Cal_SetPoint(AdcDevNumTypedef DevNum, CalPointTypedef Point)
{
    int32_t stable_val;

    if (DevNum >= ADC_DEV_NUM) return 0;
    if (Point >= CalPoint_Num) return 0;
    if (Point == CalPoint_Zero) return Cal_SetZero(DevNum);

    if (!Cal_WaitStable(DevNum, &stable_val))
        return 0;   /* 超时 */

    s_cal[DevNum][Point] = stable_val;
    s_valid[DevNum] = Cal_IsValid(DevNum);   /* 更新有效性 */
    return 1;
}


/**
 * @brief  将标定表保存到 Flash
 * @param  无
 * @retval 1=成功，0=失败
 * @note   内部依次调用 MyFlash_SetCal、MyFlash_SetRate、MyFlash_Save。
 *         s_cal 是 int32_t，MyFlash 接口是 uint32_t*，此处显式转换。
 */
uint8_t Cal_Save(void)
{
    if (!MyFlash_SetCal((uint32_t *)s_cal))  return 0;
    if (!MyFlash_SetRate(s_rate))            return 0;
    if (!MyFlash_Save())                     return 0;
    return 1;
}


/**
 * @brief  从 Flash 加载标定表
 * @param  无
 * @retval 1=成功，0=失败
 * @note   内部依次调用 MyFlash_GetCal、MyFlash_GetRate。
 */
uint8_t Cal_Load(void)
{
    if (!MyFlash_GetCal((uint32_t *)s_cal))  return 0;
    if (!MyFlash_GetRate(s_rate))            return 0;
    return 1;
}


/**
 * @brief  获取某通道某标定点的 ADC 值
 * @param  DevNum  通道号
 * @param  Point   标定点
 * @retval 对应的 ADC 值；参数越界返回 0
 */
int32_t Cal_GetAdc(AdcDevNumTypedef DevNum, CalPointTypedef Point)
{
    if (DevNum >= ADC_DEV_NUM) return 0;
    if (Point >= CalPoint_Num) return 0;
    return s_cal[DevNum][Point];
}


/**
 * @brief  获取某通道某标定点相对零点的 ADC 差值
 * @param  DevNum  通道号
 * @param  Point   标定点
 * @retval 差值 = Cal_GetAdc(Point) - Cal_GetAdc(Zero)；参数越界返回 0
 */
int32_t Cal_GetDiff(AdcDevNumTypedef DevNum, CalPointTypedef Point)
{
    if (DevNum >= ADC_DEV_NUM) return 0;
    if (Point >= CalPoint_Num) return 0;
    return s_cal[DevNum][Point] - s_cal[DevNum][CalPoint_Zero];
}


/**
 * @brief  判断某通道是否已完成全部标定
 * @param  DevNum  通道号
 * @retval 1=已标定，0=未标定或数据异常
 * @note   判定标准：
 *         - 零点非零
 *         - 所有砝码点非零，且严格递增（砝码越重 ADC 越大）
 */
uint8_t Cal_IsValid(AdcDevNumTypedef DevNum)
{
    if (DevNum >= ADC_DEV_NUM) return 0;

    /* 零点必须非零 */
    if (s_cal[DevNum][CalPoint_Zero] == 0) return 0;

    /* 所有砝码点必须非零且严格递增 */
    for (uint8_t i = CalPoint_200g; i < CalPoint_Num; i++)
    {
        if (s_cal[DevNum][i] == 0) return 0;
        if (s_cal[DevNum][i] <= s_cal[DevNum][i - 1]) return 0;
    }

    return 1;
}

/**
 * @brief  单点标定：放砝码后等待稳定，计算并保存 Rate
 * @param  DevNum  通道号
 * @param  Point   用于标定的砝码点（CalPoint_200g ~ CalPoint_4000g）
 * @retval 1=成功，0=失败（超时、参数错误、或 Rate 超限）
 * @note   计算方式：
 *             realdiff = 当前稳定 ADC 值 - 标定表零点
 *             caldiff  = 标定表该砝码点 - 标定表零点
 *             Rate     = caldiff / realdiff
 *         要求砝码越重 ADC 越大，realdiff 和 caldiff 必须为正。
 *         Rate 超过 CAL_RATE_MAX 视为异常，不更新。
 * @warning 调用前需确保标定表零点与对应砝码点已经通过多点标定写好。
 */
uint8_t Cal_SetRate(AdcDevNumTypedef DevNum, CalPointTypedef Point)
{
    int32_t stable_val;
    int32_t realdiff;
    int32_t caldiff;
    float   rate;

    if (DevNum >= ADC_DEV_NUM) return 0;
    if (Point <= CalPoint_Zero || Point >= CalPoint_Num) return 0;

    /* 等待 ADC 稳定 */
    if (!Cal_WaitStable(DevNum, &stable_val))
        return 0;

    /* 计算实际差值与标定差值 */
    realdiff = stable_val - s_cal[DevNum][CalPoint_Zero];
    caldiff  = s_cal[DevNum][Point] - s_cal[DevNum][CalPoint_Zero];

    if (realdiff <= 0 || caldiff <= 0) return 0;

    /* 计算 Rate，超限则拒绝 */
    rate = (float)caldiff / (float)realdiff;
    if (rate > CAL_RATE_MAX) return 0;

    s_rate[DevNum] = rate;
    return 1;
}

/**
 * @brief  获取某通道当前 Rate
 * @param  DevNum  通道号
 * @retval 当前 Rate；参数越界返回 1.0f
 */
float Cal_GetRate(AdcDevNumTypedef DevNum)
{
    if (DevNum >= ADC_DEV_NUM) return 1.0f;
    return s_rate[DevNum];
}
