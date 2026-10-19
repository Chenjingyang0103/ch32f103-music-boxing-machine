#ifndef __SM16028E_H
#define __SM16028E_H

#include "Project_Config.h"
#include "ch32f10x.h"
#include <stdint.h>

/* ==================== 常量定义 ==================== */

/// @brief 显示缓冲区大小（指令头8字节 + 灰度数据128字节）
#define SM28_BUFFER_SIZE        136

/// @brief 灰度数据起始索引（前8字节为指令头）
#define SM28_DATA_START         8

/// @brief 灰度数据字节数（16段 × 8位）
#define SM28_DATA_LEN           128

/// @brief 数字位数量（LED数码管位数）
#define SM28_DIGIT_NUM          16

/// @brief 每个数字位占用的灰度字节数（SEG数量）
#define SM28_BYTES_PER_DIGIT    8

/// @brief 小数点偏移（每个数字位的第8字节，即索引+7）
#define SM28_DOT_OFFSET         7

/// @brief 特殊符号索引（灰度数据区尾部）
#define SM28_SYMBOL_MINUS_INDEX 123 // 负号（当前未使用）


/// @brief          设备号枚举
typedef enum
{
    SM28DevNum0        = 0,    // SM28DevNum0 = 0,
    SM28DevNum1,               // SM28DevNum1 = 1,
    SM28DevNum2,
    SM28DevNum3,
} SM28DevNumTypedef;

/* ==================== 字符索引枚举 ==================== */

/// @brief 字模表索引（用于 SM28_ShowChar 等函数）
typedef enum
{
    Char_0 = 0,
    Char_1,
    Char_2,
    Char_3,
    Char_4,
    Char_5,
    Char_6,
    Char_7,
    Char_8,
    Char_9,
    Char_A,
    Char_b,
    Char_C,
    Char_c,
    Char_d,
    Char_E,
    Char_F,
    Char_H,
    Char_h,
    Char_i,
    Char_J,
    Char_L,
    Char_N,
    Char_n,
    Char_O,
    Char_p,
    Char_q,
    Char_r,
    Char_S,
    Char_t,
    Char_U,
    Char_u,
    Char_y,
    Char_ALL,      // 全亮
    Char_OFF,      // 全灭
    Char_TOP_BAR,  // 上横线
    Char_MID_BAR,  // 中横线
    Char_BOT_BAR,  // 下横线
    Char_MAX
} SM28CharIndexTypedef;

/// @brief 小数点/符号位置枚举（当前部分函数未实现）
typedef enum
{
    SM28_DOT_NONE       = 0xFF,   // 不显示小数点
    SM28_DOT_POS1       = 0,      // 第1位（最左侧）
    SM28_DOT_POS2       = 1,
    SM28_DOT_POS3       = 2,
    SM28_DOT_POS4       = 3,
    SM28_DOT_POS5       = 4,
    SM28_DOT_POS6       = 5,
    SM28_DOT_POS7       = 6,      // 第7位（最右侧）
    SM28_SYMBOL_MINUS   = 7, // 负号
    SM28_SYMBOL_G       = 8, // 单位 g
    SM28_SYMBOL_CT      = 9, // 单位 ct
    SM28_SYMBOL_DOT     = 10, // 小数点（需配合位置参数）
} SM28SymbolTypedef;

/* ==================== 设备参数结构体 ==================== */

/// @brief 静态参数（硬件引脚配置，初始化后不变）
typedef struct
{
    GPIO_TypeDef    *SdaPort;       // SDA 数据线端口
    uint16_t        SdaPin;         // SDA 数据线引脚
    GPIO_TypeDef    *SclPort;       // SCL 时钟线端口
    uint16_t        SclPin;         // SCL 时钟线引脚
} SM28StaticParamTypedef;

/// @brief 运行参数（动态显示状态）
typedef struct
{
    uint8_t         Buffer[SM28_BUFFER_SIZE];   // 显示缓冲区
    uint8_t         Brightness;                      // 当前亮度等级（0~255）
    uint8_t         DotPos;                          // 当前小数点位置（当前未使用）
    uint8_t         IsNegative;                      // 负号显示标志（当前未使用）
    uint32_t        UpdateCounter;                   // 刷新计数（调试用）
} SM28RunningParamTypedef;

/// @brief 完整设备参数
typedef struct
{
    SM28StaticParamTypedef     StaticParam;
    SM28RunningParamTypedef    RunningParam;
} SM28DeviceParamTypedef;

/* ==================== 对外接口 ==================== */

void SM28_Init(SM28DevNumTypedef DevNum);   // 初始化 SM16028E 设备（引脚 + 显示缓冲区）
void SM28_Clear(SM28DevNumTypedef DevNum);  // 清空显示（所有 LED 熄灭）
void SM28_AllOn(SM28DevNumTypedef DevNum);  // 全屏点亮（所有 LED 亮起）
void SM28_ShowChar(SM28DevNumTypedef DevNum, uint8_t Digit, SM28CharIndexTypedef CharIdx);  // 在指定数字位显示一个字符
void SM28_ShowNum(SM28DevNumTypedef DevNum, int32_t Value, SM28SymbolTypedef DotPos, uint8_t LeadingZero);  // 显示数值（当前未实现）

void SM28_ShowMaxForce(SM28DevNumTypedef DevNum, float ForceValue, uint8_t LeadingZero);    // 显示最大力值（带一位小数）
void SM28_ShowGameCount(SM28DevNumTypedef DevNum, int32_t Value, uint8_t LeadingZero);  // 显示游戏计数
void SM28_ShowRunTime(SM28DevNumTypedef DevNum, int32_t Value, uint8_t LeadingZero);    // 显示运行时间
void SM28_ShowSymbol(SM28DevNumTypedef DevNum, SM28SymbolTypedef Symbol, uint8_t State, uint8_t DotPos);    // 控制特殊符号（当前未实现）
void SM28_SetPixel(SM28DevNumTypedef DevNum, uint8_t ByteIdx, uint8_t State);   // 直接控制单个 LED 点
void SM28_Update(SM28DevNumTypedef DevNum); // 刷新显示（将缓冲区数据发送到芯片）
void SM28_SetBrightness(SM28DevNumTypedef DevNum, uint8_t Brightness);// 设置亮度
uint8_t* SM28_GetBuffer(SM28DevNumTypedef DevNum);  // 获取显示缓冲区指针
const SM28DeviceParamTypedef* SM28_GetParam(SM28DevNumTypedef DevNum);  // 获取设备参数指针（调试用）

#endif /* __SM16028E_H */

