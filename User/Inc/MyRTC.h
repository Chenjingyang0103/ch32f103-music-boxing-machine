#ifndef __MYRTC_H
#define __MYRTC_H

#include <time.h>
#include <stdint.h>

void MyRTC_Init(void);                                      // 初始化 RTC（LSI 时钟源，1Hz 计数）
void MyRTC_Alarm_Init(uint32_t seconds);                    // 使能闹钟中断并设置首次闹钟（当前值 + seconds）
void MyRTC_Alarm_Reload(uint32_t seconds);                  // 闹钟触发后重载下一次闹钟
void MyRTC_SetCounter(time_t Count);                        // 设置 RTC 计数值
void MyRTC_TimeToDate(time_t *TimeStamp, struct tm *tm);    // 时间戳转换为北京时间


#endif

