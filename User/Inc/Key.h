#ifndef __KEY_H
#define __KEY_H

#include <stdint.h>

void Key_Init(void);            // 初始化按键 GPIO 与 EXTI 中断
uint8_t Key_GetNum(void);       // 读取按键事件（按下返回1，无按键返回0，读取即清零）

#endif
