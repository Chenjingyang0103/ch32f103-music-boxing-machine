#ifndef __TIMER_H
#define __TIMER_H

#include <stdint.h>

uint32_t GetGameTick(void);         // 获取游戏节拍计数（每 GAME_TICK_PERIOD_MS 递增一次）
void ClearGameTick(void);           // 清零游戏节拍计数（每轮游戏开始时调用）
uint32_t GetTempTickMs(void);       // 获取上电以来的累计毫秒数（可清零，用于测时长）
void ClearTempTickMs(void);         // 清零累计毫秒数（开始测量某段过程耗时时调用）
void Timer2_Init(uint16_t arr, uint16_t psc);   // 初始化 TIM2 作为系统时间基准（arr=重装载值，psc=预分频）

#endif
