#ifndef __KEYCAL_H
#define __KEYCAL_H

#include "Project_Config.h"

/* ---------- 对外接口 ---------- */
void KeyCal_Process(AdcDevNumTypedef DevNum);   // 按键步进标定：每次按键标定一个点（目标通道由入参指定）

#endif
