#ifndef __GAMELOGIC_H
#define __GAMELOGIC_H

#include "ch32f10x.h"

// 已废弃：统一使用 Project_Config.h 中的 AdcDevNumTypedef，本枚举不再使用
// typedef enum
// {
//     GameDevNum0        = 0,    // GameDevNum0 = 0,
//     GameDevNum1,               // GameDevNum1 = 1,
//     GameDevNum2,
//     GameDevNum3,
// }GameDevNumTypedef;

typedef enum
{
    GameState_Idle,         // 空闲（等待按键启动）
    GameState_Ready,        // 准备（选随机靶子，重置击打检测；音乐模块当前未启用）
    GameState_Playing,      // 游戏中（检测击中或超时）
    GameState_WaitAfterHit, // 击中后等待超时的过渡状态
    GameState_Display,      // 显示本次力度
    GameState_Result,       // 结果显示（击中/未击中）
    GameState_Over          // 游戏结束（总次数到）
} GameStateTypedef;


typedef struct
{
    GameStateTypedef GameState;     // 当前状态

    uint16_t TotalRounds;       // 当前回合数
    uint16_t HitCount;          // 当前击中次数
    uint8_t  LastTarget;        // 上一次的靶子号
    uint8_t  TargetNum;         // 当前靶子号
}
GameRunningParamTypedef;

typedef struct
{
    uint32_t RandSeed;          // 随机种子
    uint16_t MaxRound;          // 总回合数
    uint8_t  TimeOut_Time;      // 计数超时次数, 单位:150Ms
}
GameStaticParamTypedef;

typedef struct
{
    GameRunningParamTypedef    RunningParam;   // 运行参数
    GameStaticParamTypedef     StaticParam;    // 静态参数
} GameDeviceParamTypedef;


void Game_Init(void);                    // 初始化游戏状态与随机种子
void Game_Task(void);                    // 游戏状态机主循环，主循环每圈调用一次
void Game_Start(void);                   // 启动游戏（仅 Idle 状态下有效）
GameStateTypedef Game_GetState(void);    // 获取当前游戏状态
uint16_t Game_GetTotalRounds(void);       // 获取已完成回合数
uint16_t Game_GetHitCount(void);          // 获取累计击中次数

#endif

