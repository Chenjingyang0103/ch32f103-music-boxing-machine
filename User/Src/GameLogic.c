/**
 *********************************************************************************
 * @file    GameLogic.c
 * @brief   游戏逻辑状态机（选靶、判定击中/超时、回合流转）
 * @note    依赖 PunchDetect 提供击中事件、Timer 提供节拍、SM16028E 提供显示。
 *********************************************************************************
 */

#include "GameLogic.h"
#include "Project_Config.h"
#include "PunchDetect.h"
#include "Timer.h"
#include "SM16028E.h"       // 数码管显示
#include "Led.h"            // 靶子指示 LED
#include "Weight.h"         // 每轮重记零点
#include "debug.h"
// #include "Display.h"
// #include "LEDMusic.h"


static GameDeviceParamTypedef GameDeviceParam;


/**
 * @brief  游戏模块初始化：清零运行参数、设置静态参数、产生随机种子
 * @param  无
 * @retval None
 * @note   随机种子混合 TIM2 计数值、GameTick 和固定常数，保证每次启动序列不同。
 *         Display_Init / LEDMusic_Init 当前被注释，暂未启用。
 */
void Game_Init(void)
{
    // 获取运行参数指针和静态参数指针
    GameRunningParamTypedef *pRun = &GameDeviceParam.RunningParam;
    GameStaticParamTypedef  *pSta = &GameDeviceParam.StaticParam;

    // ---- 初始化运行参数 ----
    pRun->TargetNum     = 0;                     // 默认靶子号0
    pRun->GameState     = GameState_Idle;        // 初始为空闲状态
    pRun->TotalRounds   = 0;                     // 总回合数清零
    pRun->HitCount      = 0;                     // 击中次数清零
    pRun->LastTarget    = 255;                   // 上次靶子设为无效值，保证首轮不重复

    // ---- 初始化静态参数 ----
    pSta->RandSeed      = TIM_GetCounter(TIM2) ^ (uint32_t)(GetGameTick() << 16) ^ 0xDEADBEEF; // 混合定时器与Tick产生随机种子
    pSta->MaxRound      = MAX_ROUNDS;            // 最大回合数（来自Project_Config.h）
    pSta->TimeOut_Time  = TIMEOUT_TIME;          // 超时次数

    // Display_Init();   // 显示模块初始化（暂未启用）
    // LEDMusic_Init();  // 音乐模块初始化（暂未启用）
}

/**
 * @brief  生成随机靶子号（0 ~ GAME_TARGET_NUM-1）
 * @param  无
 * @retval 随机靶子号（0 ~ GAME_TARGET_NUM-1）
 * @note   - 使用 LCG（线性同余）算法，种子在初始化时混合硬件定时器值，
 *           保证每次启动序列不同。
 *         - 取种子高 16 位进行取模，避免低位周期性差的问题。
 *         - 若 GAME_CONSECUTIVE_TARGET=0，则循环直到生成与上次不同的靶子号。
 */
static uint8_t Game_RandomTarget(void)
{
    uint8_t TargetNum;
    GameRunningParamTypedef *pRun = &GameDeviceParam.RunningParam;
    GameStaticParamTypedef  *pSta = &GameDeviceParam.StaticParam;

    #if !GAME_CONSECUTIVE_TARGET    // 预编译宏, 不允许连续两次出现相同靶子
        do
        {
            // LCG 算法：标准 glibc 使用的参数
            pSta->RandSeed = pSta->RandSeed * 1103515245 + 12345;
            // printf("随机种子:%d\n", pSta->RandSeed);
            // 取高 16 位，高位随机性更好
            TargetNum = (pSta->RandSeed >> 16) % GAME_TARGET_NUM;
        } while (TargetNum == pRun->LastTarget);   // 避免连续相同
        pRun->LastTarget = TargetNum;               // 记录本次靶号供下次比较
    #else       // 预编译宏, 允许连续两次出现相同靶子
        {
            pSta->RandSeed = pSta->RandSeed * 1103515245 + 12345;
            // 取高 16 位，高位随机性更好
            TargetNum = (pSta->RandSeed >> 16) % GAME_TARGET_NUM;
        }
    #endif
    return TargetNum;
}

/**
 * @brief  游戏状态机主循环，主循环每圈调用一次
 * @param  无
 * @retval None
 * @note   状态流转（取决于 GAME_SWITCH_AFTER_HIT 宏）：
 *         - Idle：等待按键触发 Game_Start()
 *         - Ready：生成随机靶子号，重置击打检测，进入 Playing
 *         - Playing：检测击中或超时
 *             击中且 GAME_SWITCH_AFTER_HIT=1 → Display → Result
 *             击中且 GAME_SWITCH_AFTER_HIT=0 → WaitAfterHit → Result
 *             超时 → Result
 *         - Display：显示本次力度峰值
 *         - WaitAfterHit：击中后等待超时结束
 *         - Result：更新显示，判断继续下一轮或结束
 *         - Over：打印最终成绩，回到 Idle
 */
void Game_Task(void)
{
    GameRunningParamTypedef *pRun = &GameDeviceParam.RunningParam;
    GameStaticParamTypedef  *pSta = &GameDeviceParam.StaticParam;
    // printf("进入Game_Task()\n");
    switch (pRun->GameState)
    {
        case GameState_Idle:
        {
            // printf("当前状态:Idle\n");
            // 等待按键触发 Game_Start()
            break;
        }
        case GameState_Ready:
        {
            // 播放准备音乐，选靶子，然后进入 PLAYING
            // （这里可以加 500ms 延时做过渡）
            pRun->TargetNum = Game_RandomTarget();          // 生成随机靶子号
            Led_On((LedDevNumTypedef)pRun->TargetNum);      // 点亮当前选中靶子对应的 LED
            /* 每轮选靶后，用该通道最新滤波 ADC 值重记零点，消除通道零点漂移 */
            Weight_SetZero((AdcDevNumTypedef)pRun->TargetNum,
                           Weight_GetAdcValue((AdcDevNumTypedef)pRun->TargetNum));
            printf("{当前靶号}%d\r\n", (pRun->TargetNum + 1));
            printf("{当前靶号:%d, 击中次数%d, 总回合数:}%d\r\n", (pRun->TargetNum + 1), pRun->HitCount, pRun->TotalRounds);
            // printf("当前状态Ready Num:%d\n", pRun->TargetNum);
            // LEDMusic_TargetOn(s_TargetNum);
            pRun->GameState = GameState_Playing;
            ClearGameTick();                                // 重新计时（150ms节拍清零）

            Punch_Reset((AdcDevNumTypedef)pRun->TargetNum);                   // 清空击打标志位
            Punch_ClearPeakForce((AdcDevNumTypedef)pRun->TargetNum);          // 清除上次峰值

            break;
        }
        case GameState_Playing:
        {
            // printf("当前状态:Playing  Num:%d\n", pRun->TargetNum);

            if (Punch_IsHit((AdcDevNumTypedef)pRun->TargetNum)) // 检测是否击中（边沿触发，读取后自动清零）
            {
                float PeakForce = Punch_GetPeakForce((AdcDevNumTypedef)pRun->TargetNum);  // 获取当前轮次力度峰值
                // Punch_ClearPeakForce(pRun->TargetNum);
                /* 本次为静态载（松手前锁存）打印锁存的稳定读数；动态冲拳打印本回合峰值力度 */
                float PrintForce = PeakForce;
                if (Punch_GetLastFilterMode((AdcDevNumTypedef)pRun->TargetNum) == FilterMode_Static)
                {
                    PrintForce = Punch_GetStaticWeight((AdcDevNumTypedef)pRun->TargetNum);
                }
                printf("{击中靶子}%d\r\n", (pRun->TargetNum + 1));
                printf("击中靶子%d,力度:%f\r\n", (pRun->TargetNum + 1), PrintForce);
                // printf("击中靶子%d,力度:%f\r\n", (pRun->TargetNum + 1), PeakForce);   // 旧的打印峰值力度（保留备查）
                pRun->HitCount++;       // 击中次数+1
                pRun->TotalRounds++;    // 总局数+1
                Led_Off((LedDevNumTypedef)pRun->TargetNum);  // 击中后熄灭对应 LED
                // LEDMusic_PlayHit(force);
                // Display_ShowForce(force);
                #if GAME_SWITCH_AFTER_HIT
                    // 立即切换
                    pRun->GameState = GameState_Display;
                #else
                    // 等待超时再切换
                    pRun->GameState = GameState_WaitAfterHit;
                    ClearGameTick();          // 重新计时
                #endif
                break;
            }

            #if GAME_TIMEOUT_ENABLE
                // 检测是否超时（超时次数 TimeOut_Time * 150ms）
            else if (GetGameTick() >= pSta->TimeOut_Time)
                {
                    // printf("Time:%d\n", GetGameTick());
                    pRun->TotalRounds++;
                    Led_Off((LedDevNumTypedef)pRun->TargetNum);  // 超时后熄灭对应 LED
                    // printf("击打超时!!!!!!!!!!!!!\n");
                    // LEDMusic_PlayMiss();
                    // Display_ShowForce(0);
                    // Punch_ClearPeakForce(PunchDevNum0);

                    pRun->GameState = GameState_Result;
                }
            #endif
            break;
        }

        case GameState_WaitAfterHit:
        {
            // 等待超时结束
            if (GetGameTick() >= pSta->TimeOut_Time)
            {
                pRun->GameState = GameState_Result;
            }
            break;
        }
        case GameState_Display:
        {
            float PeakForce = Punch_GetPeakForce((AdcDevNumTypedef)pRun->TargetNum);  // 获取当前轮次力度峰值
            /* 本次为静态载（松手前锁存）显示锁存的稳定读数；动态冲拳显示本回合峰值力度 */
            float ShowValue = PeakForce;
            if (Punch_GetLastFilterMode((AdcDevNumTypedef)pRun->TargetNum) == FilterMode_Static)
            {
                ShowValue = Punch_GetStaticWeight((AdcDevNumTypedef)pRun->TargetNum);
            }
            SM28_ShowMaxForce(SM28DevNum0, ShowValue, 0);   // 显示模式相关值（消隐前导零）
            // SM28_ShowMaxForce(SM28DevNum0, PeakForce, 0);   // 旧的显示本次力度峰值（消隐前导零，保留备查）
            pRun->GameState = GameState_Result;
            break;

        }
        case GameState_Result:
        {
            // printf("当前状态:Result\n");
            // printf("击中次数:%d,总次数:%d\n", pRun->HitCount, pRun->TotalRounds);
            // 显示结果（持续 800ms 后进入下一轮或结束）

            SM28_ShowGameCount(SM28DevNum0, pRun->TotalRounds, 0);  //显示的是当前总回合数（消隐前导零）

            SM28_Update(SM28DevNum0);

            if (pRun->TotalRounds >= pSta->MaxRound) // 当前局数 >= 总局数
            {
                pRun->GameState = GameState_Over;   // 游戏结束
            }
            else // 当前局数 < 总局数
            {
                pRun->GameState = GameState_Ready;  // 继续下一轮
            }
            break;
        }
        case GameState_Over:
        {
            // 游戏结束，显示最终得分
            // Display_ShowFinal(pRun->HitCount, pRun->TotalRounds);
            // LEDMusic_PlayGameOver();
            printf("击中次数:%d,总次数:%d\r\n", pRun->HitCount, pRun->TotalRounds);
            Led_AllOff();   // 游戏结束，熄灭全部靶子指示
            pRun->GameState = GameState_Idle;   // 回到空闲模式,等待按键触发Game_Start()开始游戏
            printf("总运行时间:%d毫秒\r\n", GetTempTickMs());
            break;
        }
    }
}


/**
 * @brief  启动一局游戏（由按键触发）
 * @param  无
 * @retval None
 * @note   仅在 Idle 状态下生效；会重置 TotalRounds 和 HitCount，
 *         并进入 Ready 状态开始新一轮。
 */
void Game_Start(void)
{
    GameRunningParamTypedef *pRun = &GameDeviceParam.RunningParam;

    // 只有空闲状态才允许启动，防止干扰正在进行的游戏
    if (pRun->GameState == GameState_Idle)
    {
        pRun->TotalRounds = 0;      // 重置总回合数
        pRun->HitCount = 0;         // 重置击中次数
        pRun->GameState = GameState_Ready;  // 切换到准备状态，触发新游戏
    }
}

/**
 * @brief 获取当前游戏状态
 * @retval 当前状态（GameStateTypedef）
 */
GameStateTypedef Game_GetState(void)
{
    GameRunningParamTypedef *pRun = &GameDeviceParam.RunningParam;
    return pRun->GameState;
}

/**
 * @brief 获取当前已完成的回合总数
 * @retval 回合数（0~MAX_ROUNDS）
 */
uint16_t Game_GetTotalRounds(void)
{
    GameRunningParamTypedef *pRun = &GameDeviceParam.RunningParam;
    return pRun->TotalRounds;
}

/**
 * @brief 获取当前累计击中次数
 * @retval 击中次数
 */
uint16_t Game_GetHitCount(void)
{
    GameRunningParamTypedef *pRun = &GameDeviceParam.RunningParam;
    return pRun->HitCount;
}
