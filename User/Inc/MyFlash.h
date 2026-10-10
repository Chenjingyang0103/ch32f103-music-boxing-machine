#ifndef __MYFLASH_H
#define __MYFLASH_H

#include "Project_Config.h"
#include "debug.h"

/* 数据区起始地址：最后一页 */
#define MYFLASH_ADDR    ((uint32_t)0x0800FC00)

/* 一次操作的块大小，必须是 128 的整数倍（快速擦写要求） */
#define MYFLASH_BLOCK_SIZE  256

/* 魔数：用于判断 Flash 是否已写过有效数据 */
#define MYFLASH_MAGIC       0x44345615

/* 数据区长度（字节）：6*6*4 = 144 */
#define MYFLASH_CAL_SIZE   (ADC_DEV_NUM * ADC_CAL_POINTS * 4)

/* ============================ Flash 布局 ============================ */
/*
 * 仅作为 buf / Flash 的"视图"使用，不是数据本体。
 * 加字段时只改这里，reserved 自动缩水，其它代码不动。
 *
 *  偏移 0    : magic   4B
 *  偏移 4    : cal     144B  (ADC_DEV_NUM × ADC_CAL_POINTS × 4)
 *  偏移 148  : rate    4B
 *  偏移 152  : reserved 104B  (补到 256B)
 */
typedef struct
{
    uint32_t magic;         // 魔数, 检测第一个是不是设定好的 MYFLASH_MAGIC 值, 如果不是则为新的Flash
    uint32_t cal[ADC_DEV_NUM][ADC_CAL_POINTS];      // 五点标定参数表, 不包括零点, 零点上电的时候自动存入
    float    rate[ADC_DEV_NUM];                       // 单点标定参数表
    uint8_t  reserved[MYFLASH_BLOCK_SIZE
                      - sizeof(uint32_t)                     /* magic */
                      - (ADC_DEV_NUM * ADC_CAL_POINTS * sizeof(uint32_t))   /* cal   */
                      - (ADC_DEV_NUM * sizeof(float))];                    /* rate  */
} MyFlashParamTypedef;

/* ============================ 接口函数 ============================ */

/**
 * @brief  上电初始化
 * @param  pCal   外部标定表（6×6），作为默认值 / 读回目标
 * @param  pRate  外部倍率变量，作为默认值 / 读回目标
 * @retval 1 成功，0 失败
 * @note   - Flash 有数据 → 读到 pCal / *pRate
 *         - Flash 空白   → 用 pCal / *pRate 当前值写入
 */
uint8_t MyFlash_Init(uint32_t *pCal, float *pRate);

/**
 * @brief  把 buf 整体擦写回 Flash
 * @retval 1 成功，0 失败
 * @note   任何 Set 之后都要调一次才能落到 Flash。
 */
uint8_t MyFlash_Save(void);

/**
 * @brief  把外部标定表同步到 buf（不碰 Flash）
 * @param  pCal  外部 6×6 标定表
 * @retval 1 成功，0 失败
 */
uint8_t MyFlash_SetCal(const uint32_t *pCal);

/**
 * @brief  把倍率同步到 buf（不碰 Flash）
 * @param  rate  单点标定倍率
 * @retval 1 成功，0 失败
 */
uint8_t MyFlash_SetRate(const float *pRate);

/**
 * @brief  从 buf 取回标定表到外部数组
 * @param  pCal  外部 6×6 标定表
 * @retval 1 成功，0 失败
 */
uint8_t MyFlash_GetCal(uint32_t *pCal);

/**
 * @brief  从 buf 取回倍率
 * @param  pRate 输出倍率
 * @retval 1 成功，0 失败
 */
uint8_t MyFlash_GetRate(float *pRate);

#endif /* __MYFLASH_H */
