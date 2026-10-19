/**
 *********************************************************************************
 * @file    KeyCal.c
 * @brief   按键步进标定流程（每按一次键只标定一个点）
 * @note    应用层业务：由 main.c 抽出，负责标定点顺序推进与提示、Flash 保存编排；
 *          具体标定算法由 Middleware 层的 Calibration 模块提供。
 *********************************************************************************
 */

#include "KeyCal.h"         // 自身头文件
#include "Project_Config.h" // 全局参数与设备号类型
#include "Calibration.h"    // 标定模块接口
#include "Weight.h"         // 标定完成后重新初始化重量

#include "debug.h"          // printf 调试输出
#include "ch32f10x.h"

/* 0 = 未在标定流程；1~6 = 已进入流程，下一次按键标定第 s_CalStep 个点（0 起算） */
static uint8_t s_CalStep = 0;

/* 标定点顺序：第 1 次按键→零点，第 2 次→200g，依次到第 6 次→4000g */
static const CalPointTypedef s_CalOrder[] = {
    CalPoint_Zero, CalPoint_200g, CalPoint_500g, CalPoint_1000g, CalPoint_2000g, CalPoint_4000g,
};

/* 各标定点对应的提示名称，与 s_CalOrder 一一对应 */
static const char *s_CalName[] = {
    "零点", "200g", "500g", "1000g", "2000g", "4000g",
};

/**
 * @brief  按键步进标定：每按一次键只标定一个点
 * @param  DevNum  通道号
 * @retval None
 * @note   顺序：第 1 次按键→零点，第 2 次→200g，第 3 次→500g，
 *         第 4 次→1000g，第 5 次→2000g，第 6 次→4000g。
 *         每次标定内部阻塞等待 ADC 稳定（Cal_SetPoint 内部调用 Cal_WaitStable），
 *         需先把砝码放好再按键。
 *         第 6 次完成后自动 Cal_Save 保存 Flash、Weight_Init 重新初始化，并退出标定流程。
 *         某次标定失败（超时）时停在该点，可再次按键重试。
 */
void KeyCal_Process(AdcDevNumTypedef DevNum)
{
    const uint8_t   cnt   = (uint8_t)(sizeof(s_CalOrder) / sizeof(s_CalOrder[0]));
    uint8_t         step  = s_CalStep;
    CalPointTypedef point = s_CalOrder[step];

    if (step == 0)
    {
        printf("进入按键标定流程：正在标定零点，请清空秤台并保持稳定...\r\n");
    }
    else
    {
        printf("按键标定：正在标定 %s，请保持稳定...\r\n", s_CalName[step]);
    }

    if (!Cal_SetPoint(DevNum, point))
    {
        printf("%s 标定失败（超时），可再次按键重试\r\n", s_CalName[step]);
        return;                         /* 失败停在该点，s_CalStep 保持不变 */
    }
    printf("{%s 标定完成：}%d\r\n", s_CalName[step], Cal_GetAdc(DevNum, point));

    s_CalStep = step + 1;

    if (s_CalStep >= cnt)
    {
        /* ---- 全部标定点完成：保存到 Flash 并用新标定值重新初始化 Weight ---- */
        if (Cal_Save())
        {
            printf("标定数据已保存到 Flash\r\n");
        }
        else
        {
            printf("标定数据保存失败\r\n");
        }
        Weight_Init(DevNum, Cal_GetAdc(DevNum, CalPoint_Zero));
        s_CalStep = 0;
        printf("标定流程结束\r\n");
    }
    else
    {
        printf("请放置 %s 砝码，再次按键标定\r\n", s_CalName[s_CalStep]);
    }
}
