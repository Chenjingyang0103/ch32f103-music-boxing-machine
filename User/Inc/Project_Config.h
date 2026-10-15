#ifndef __PROJECT_CONFIG_H
#define __PROJECT_CONFIG_H

/* ==================== 设备号全局枚举（唯一出处） ==================== */
typedef enum
{
    AdcDevNum0 = 0,
    AdcDevNum1,
    AdcDevNum2,
    AdcDevNum3,
    AdcDevNum4,
    AdcDevNum5,
    // 如果后续拓展，直接在此添加，但必须保证数量 <= ADC_DEV_NUM
} AdcDevNumTypedef;

/* ==================== HCT6521 参数配置 ==================== */
#define ADC_DEV_NUM                 6       // 设备数量
#define ADC_CAL_POINTS              6       // 标定点数量
#define ADC_ZERO_READ_TIMEOUT       20000   // 上电读取空载零点超时次数，超出后按默认零点 0 处理

/* ==================== Filter 参数配置 ==================== */
#define FILTER_STATIC_DEEP          1       // 静态窗口滤波档位，对应 Calibration 档位1 = 窗口15/取索引7起2个/右移1位
// #define WINDOW_SIZE     3       // 窗口大小
// #define SPIKE_RATIO     1.0       // 毛刺判定比例, 单位: %
// #define SPIKE_ABS_THRESHOLD  50   // 最大和次大差超过 50 就算毛刺

/* ==================== Weight / Calibration 参数配置 ==================== */
#define CAL_TIMES                   5       // 校准点数量

/* ==================== PunchDetect 参数配置 ==================== */
#define TRIGGER_THRESHOLD           200     // 开始追踪的门槛（重量 g）
#define RELEASE_THRESHOLD           150      // 松手复位门槛（重量 g）
#define RELEASE_COUNT               2       // 释放计数：连续 N 点低于释放阈值判定松手
#define STATIC_CNT_LIMIT            14       // 连续超过触发阈值的采样点数达到该值即判定为静态加载

/* ==================== GameLogic 参数配置 ==================== */
#define GAME_TARGET_NUM             4       // 游戏使用的靶子数量（逻辑靶）

#define GAME_TIMEOUT_ENABLE         0       // 是否开启超时切换,超时会切换下一个随机靶子：1=开启，0=关闭
#define GAME_SWITCH_AFTER_HIT       1       // 击打后是否立即切换下一个靶子：1=立即切换，0=等待超时后再切换
#define GAME_CONSECUTIVE_TARGET     0       // 是否允许连续两次出现相同靶子：1=允许，0=不允许

#define MAX_ROUNDS                  10000   // 总回合数,值不能大于65535
#define TIMEOUT_TIME                10      // 超时次数,默认计时150Ms,超时时间 = 超时次数 * 150Ms, 数值越小越快,最快150Ms

/* ==================== Timer 参数配置 ==================== */
#define GAME_TICK_PERIOD_MS         100     // 游戏逻辑计数周期，单位毫秒（每150ms GameTick加1）

/* ==================== SM16028E 参数配置 ==================== */
#define SM28_DEV_NUM            1       // 设备数量

/* ==================== Led 参数配置 ==================== */
#define LED_DEV_NUM             6       // LED 设备数量（与靶子数一致，对应 AdcDevNum0~5）

#endif
