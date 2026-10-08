/**
 *********************************************************************************
 * @file    HCT6521.c
 * @brief   HCT6521 高精度 ADC 驱动（GPIO 模拟 SPI，非阻塞状态机读取）
 * @note    每个通道使用独立的 SCK 控制引脚、共用的 SDO 数据引脚，
 *          通过模拟 SPI 时序完成寄存器配置与 24 位原始数据采集。
 *********************************************************************************
 */

#include "HCT6521.h"
#include "debug.h"
#include <string.h>     //提供memcpy函数

//电平操作宏
#define SDO(emDevNum)        SdoReadLevel(emDevNum)
#define SCLK_L(emDevNum)     SckSetLow(emDevNum)
#define SCLK_H(emDevNum)     SckSetHigh(emDevNum)

#define SDO_L(emDevNum)      SdoSetLow(emDevNum)
#define SDO_H(emDevNum)      SdoSetHigh(emDevNum)

// ADC 设备参数数组
static AdcDeviceParamTypedef AdcDeviceParam[ADC_DEV_NUM];


// 读取Sdo引脚电平
static uint8_t SdoReadLevel(AdcDevNumTypedef emDevNum)
{
    return GPIO_ReadInputDataBit(AdcDeviceParam[emDevNum].StaticParam.SdoPort,
                                 AdcDeviceParam[emDevNum].StaticParam.SdoPin);
}
// Sdo输出高电平
static void SdoSetHigh(AdcDevNumTypedef emDevNum)
{
    GPIO_SetBits(AdcDeviceParam[emDevNum].StaticParam.SdoPort,
                 AdcDeviceParam[emDevNum].StaticParam.SdoPin);
}
// Sdo输出低电平
static void SdoSetLow(AdcDevNumTypedef emDevNum)
{
    GPIO_ResetBits(AdcDeviceParam[emDevNum].StaticParam.SdoPort,
                   AdcDeviceParam[emDevNum].StaticParam.SdoPin);
}
// Sclk输出高电平
static void SckSetHigh(AdcDevNumTypedef emDevNum)
{
    GPIO_SetBits(AdcDeviceParam[emDevNum].StaticParam.SckPort,
                 AdcDeviceParam[emDevNum].StaticParam.SckPin);
}
// Sck输出低电平
static void SckSetLow(AdcDevNumTypedef emDevNum)
{
    GPIO_ResetBits(AdcDeviceParam[emDevNum].StaticParam.SckPort,
                   AdcDeviceParam[emDevNum].StaticParam.SckPin);
}

/**
  * @brief  SDO 切换为上拉输入
  * @param  DevNum  通道号
  */
static void Input(AdcDevNumTypedef DevNum)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.GPIO_Pin = AdcDeviceParam[DevNum].StaticParam.SdoPin;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(AdcDeviceParam[DevNum].StaticParam.SdoPort, &GPIO_InitStruct);
}

/**
  * @brief  SDO 切换为推挽输出
  * @param  DevNum  通道号
  */
static void Output(AdcDevNumTypedef DevNum)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.GPIO_Pin = AdcDeviceParam[DevNum].StaticParam.SdoPin;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(AdcDeviceParam[DevNum].StaticParam.SdoPort, &GPIO_InitStruct);
    SDO_H(DevNum);
}

/**
  * @brief  CRC7校验计算（针对HCT6521通信协议）
  * @param  data  待计算校验的8位数据
  * @retval 计算后的7位CRC值
  */
static uint32_t Crc7(uint32_t data)
{
    uint8_t i;
    uint32_t crc7;
    uint32_t shift_reg;
    shift_reg = data << 24;  // 数据左移24位，对齐最高位（适配32位运算）
    crc7 = 0x5A;             // CRC初始值（HCT6521协议约定）
    for (i = 0; i < 8; i++)   // 逐位处理8位数据
    {
        // 比较CRC最高位与数据最高位
        if ((crc7 >> 6) != (shift_reg >> 31))
            crc7 = ((crc7 << 1) ^ 0x09) & 0x7F;  // 位不匹配：左移+异或多项式0x09，保留7位
        else
            crc7 = (crc7 << 1) & 0x7F;           // 位匹配：仅左移，保留7位
        shift_reg <<= 1;                       // 数据左移1位，处理下一位
    }
    return crc7;
}

/**
  * @brief  模拟SPI写入1个字节（高位先传，匹配HCT6521时序）
  * @param  DevNum  通道号
  * @param  data  待写入的8位命令/数据
  * @retval None
  */
static void IO_WriteByte(AdcDevNumTypedef DevNum, uint8_t data)
{
    uint8_t i;
    for (i = 0; i < 8; i++)   // 逐位写入（高位先传，SPI标准时序）
    {
        // 判断当前最高位（0x80为第8位掩码）
        if (data & 0x80)
            SDO_H(DevNum);  // 最高位为1，SDO置高
        else
            SDO_L(DevNum);  // 最高位为0，SDO置低
        data <<= 1;            // 数据左移1位，准备下一位
        SCLK_H(DevNum);                // SCK置高，锁存数据（HCT6521采样时机）
        Delay_Us(100);         // 高电平延时，匹配HCT6521时序要求
        SCLK_L(DevNum);                // SCK置低，准备下一位
        Delay_Us(5);           // 低电平延时，保证时序稳定
    }
}

/**
  * @brief  向HCT6521写入命令并进行CRC校验（确保命令传输正确）
  * @param  DevNum  通道号
  * @param  data  待写入的8位命令（如初始化、配置命令）
  * @retval None
  */
static void SpiWriteComCheck(AdcDevNumTypedef DevNum, uint8_t data)
{
    uint8_t crc7;

    Output(DevNum);                  // 配置SDO为输出模式（写命令需输出数据）
    SCLK_H(DevNum);                  // SCK初始高电平（HCT6521通信起始条件）
    Delay_Us(200);                   // 时序等待，确保设备就绪
    SCLK_L(DevNum);                  // SCK拉低，开始通信（HCT6521起始信号）
    Delay_Us(1);

    IO_WriteByte(DevNum, 0xA5);      // 写入命令前缀（HCT6521协议约定，标识命令帧）
    IO_WriteByte(DevNum, data);      // 写入目标命令（如初始化命令）
    crc7 = Crc7(data);       // 计算命令的CRC7校验值
    // 校验值左移1位 + 最低位1（HCT6521协议要求的校验格式）
    IO_WriteByte(DevNum, (crc7 << 1) | 0x01);
    SDO_H(DevNum);
    SCLK_H(DevNum);                  // 结束通信，SCK置高
    Delay_Us(1);
}

/**
  * @brief  模拟SPI读取1个字节（高位先传，用于读取命令响应）
  * @param  DevNum  通道号
  * @retval 读取到的8位响应数据
  */
static uint8_t IO_ReadByte(AdcDevNumTypedef DevNum)
{
    uint8_t i;
    uint8_t data = 0;

    for (i = 0; i < 8; i++)   // 逐位读取（高位先传）
    {
        SCLK_H(DevNum);                // SCK置高，HCT6521输出数据（采样时机）
        Delay_Us(100);         // 延时等待数据稳定（避免采样错误）
        SCLK_L(DevNum);                // SCK拉低，准备下一位采样

        data <<= 1;            // 数据左移1位，空出最低位
        // 读取SDO引脚状态，为高则最低位置1
        if (SDO(DevNum))
            data += 1;

        Delay_Us(5);           // 低电平延时，保证时序稳定
    }
    return data;
}

/**
  * @brief  读取HCT6521命令响应并校验CRC（确保响应数据正确）
  * @param  DevNum  通道号
  * @retval 有效响应数据（校验失败返回0xFF）
  */
static uint32_t SpiReadComCheck(AdcDevNumTypedef DevNum)
{
    uint32_t data;
    uint8_t data_temp;
    uint8_t check;

    SCLK_H(DevNum);                         // SCK初始高电平
    Delay_Us(200);                  // 时序等待，确保设备就绪
    SCLK_L(DevNum);                         // SCK拉低，开始通信
    Delay_Us(1);

    Output(DevNum);                       // 先配置SDO为输出，发送读命令前缀
    IO_WriteByte(DevNum, 0xB5);             // 写入读命令前缀（HCT6521协议约定，标识读帧）
    Input(DevNum);                        // 切换SDO为输入，准备读取响应数据
    Delay_Us(1);

    data = IO_ReadByte(DevNum);               // 读取HCT6521的响应数据
    check = Crc7(data);                 // 计算响应数据的CRC7校验值
    data_temp = IO_ReadByte(DevNum) >> 1;     // 读取校验字节，右移1位对齐校验值

    SCLK_H(DevNum);                  // 结束通信，SCK置高
    Delay_Us(1);

    // CRC校验：一致则返回数据，否则返回0xFF（校验失败）
    if (data_temp != check)
        return 0xFF;
    return data;
}

/**
  * @brief  快速读取HCT6521 ADC数据字节（短延时，适配数据快速输出）
  * @param  DevNum  通道号
  * @retval 读取到的8位ADC数据
  */
static uint8_t IO_ReadByte_Data(AdcDevNumTypedef DevNum)
{
    uint8_t i;
    uint8_t data = 0;

    for (i = 0; i < 8; i++)   // 逐位读取（高位先传，快速时序）
    {
        SCLK_H(DevNum);        // SCK置高，HCT6521输出ADC数据
        Delay_Us(1);           // 短延时（ADC数据快速稳定，无需100us）
        SCLK_L(DevNum);        // SCK拉低，采样数据
        Delay_Us(1);

        data <<= 1;            // 数据左移1位
        // 读取SDO引脚状态，为高则最低位置1
        if (SDO(DevNum) == 1)
            data += 1;

        Delay_Us(1);           // 短延时，保证时序效率
    }
    return data;
}

/**
  * @brief  HCT6521初始化（重复读写命令，确保配置成功）
  * @param  DevNum  通道号
  * @param  AdcGain 增益选择
  * @param  AdcDr 数据速率
  * @param  AdcRef 基准源
  * @param  AdcShi 输入短路
  * @param  AdcMode MODE 位
  * @retval None
  */
static void HCT6521_Config(AdcDevNumTypedef DevNum, AdcGainTypedef AdcGain, AdcDrTypedef AdcDr, AdcRefTypedef AdcRef, AdcShiTypedef AdcShi, AdcModeTypedef AdcMode)
{
    uint8_t cmd = 0;              // 指令码
    uint8_t reset_time = 0;       // 重试计数器
    uint32_t conf_data = 0;       // 读取的配置回显数据
    cmd = AdcGain | AdcDr | AdcRef | AdcShi | AdcMode;

    // 首次写入命令并读取回显
    SpiWriteComCheck(DevNum, cmd);
    conf_data = SpiReadComCheck(DevNum);
    Delay_Ms(1);

    // 校验配置：若回显与命令不一致，重试最多10次
    while ((conf_data != cmd) && (reset_time < 10))
    {
        reset_time++;
        Delay_Ms(1);
        SpiWriteComCheck(DevNum, cmd);    // 重新写入命令
        conf_data = SpiReadComCheck(DevNum);    // 重新读取回显
    }
}

/**
  * @brief  初始化指定设备的 SPI 引脚（SCK + SDO），根据设备号从结构体读取端口引脚
  * @param  DevNum  设备号（0~15）
  * @retval None
  */
static void HCT6521_SPI_Init(AdcDevNumTypedef DevNum)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    AdcStaticParamTypedef *pParam = &AdcDeviceParam[DevNum].StaticParam;

    // -------- 1. 使能对应 GPIO 端口的时钟（根据端口指针动态判断） --------
    if (pParam->SckPort == GPIOA)
    {
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    }
    else if (pParam->SckPort == GPIOB)
    {
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    }
    else if (pParam->SckPort == GPIOC)
    {
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    }
    else if (pParam->SckPort == GPIOD)
    {
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);
    }
    else if (pParam->SckPort == GPIOE)
    {
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE);
    }
    // 若 SDO 与 SCK 不同端口，也要使能 SDO 的端口时钟
    if (pParam->SdoPort != pParam->SckPort)
    {
        if (pParam->SdoPort == GPIOA)
        {
            RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
        }
        else if (pParam->SdoPort == GPIOB)
        {
            RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
        }
        else if (pParam->SdoPort == GPIOC)
        {
            RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
        }
        else if (pParam->SdoPort == GPIOD)
        {
            RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);
        }
        else if (pParam->SdoPort == GPIOE)
        {
            RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE);
        }
    }

    // -------- 2. 配置 SCK 为推挽输出（空闲高） --------
    GPIO_InitStruct.GPIO_Pin = pParam->SckPin;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(pParam->SckPort, &GPIO_InitStruct);
    SCLK_H(DevNum);   // 空闲高电平

    // -------- 3. 配置 SDO 为上拉输入（默认） --------
    GPIO_InitStruct.GPIO_Pin = pParam->SdoPin;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(pParam->SdoPort, &GPIO_InitStruct);
}

/**
  * @brief  初始化 ADC 设备的静态参数（将配置存入结构体数组）
  * @param  pStaticInit  静态参数结构体指针
  * @param  DevNum   设备号（0~15）
  * @retval None
  */
static void AdcDeviceStaticInit(AdcStaticParamTypedef *pStaticInit, AdcDevNumTypedef DevNum)
{
    if (DevNum >= ADC_DEV_NUM) return;
    // 将 pStaticInit 的内容拷贝到本文件静态数组 AdcDeviceParam 中
    memcpy(&AdcDeviceParam[DevNum].StaticParam, pStaticInit, sizeof(AdcStaticParamTypedef));
}

/**
  * @brief  初始化全部 HCT6521 ADC 设备（硬件引脚 + 芯片寄存器）
  * @param  无
  * @retval 无
  * @note   该函数按以下顺序完成初始化：
  *         1. 逐一配置各通道的 SCK/SDO 引脚参数（端口和引脚号），
  *            并调用 AdcDeviceStaticInit() 存入全局结构体数组 AdcDeviceParam。
  *         2. 循环调用 HCT6521_SPI_Init()，根据全局结构体配置，初始化各通道 GPIO 外设。
  *            包括使能 GPIO 时钟、配置 SCK 为推挽输出、SDO 为上拉输入。
  *         3. 循环调用 HCT6521_Config()，向各芯片写入配置命令。
  *            （例如: 0x42 对应：增益 64 倍、数据速率 20Hz、使用外部基准，具体见手册 ANA_REGIN）
  * @warning 调用本函数前，需确保系统时钟、延时函数（Delay_Init）已初始化，
  *          且本文件静态数组 AdcDeviceParam 已定义。
  */

void HCT6521_Init(void)
{

    AdcStaticParamTypedef StaticInit;
    // ====================== 设备1 ======================
    StaticInit.SckPort = GPIOA;
    StaticInit.SckPin   = GPIO_Pin_12;
    StaticInit.SdoPort = GPIOA;
    StaticInit.SdoPin   = GPIO_Pin_11;
    AdcDeviceStaticInit(&StaticInit, AdcDevNum4);


    // ====================== 设备2 ======================
    StaticInit.SckPort = GPIOB;
    StaticInit.SckPin   = GPIO_Pin_4;
    StaticInit.SdoPort = GPIOB;
    StaticInit.SdoPin   = GPIO_Pin_3;
    AdcDeviceStaticInit(&StaticInit, AdcDevNum1);


    // ====================== 设备3 ======================
    StaticInit.SckPort = GPIOB;
    StaticInit.SckPin   = GPIO_Pin_6;
    StaticInit.SdoPort = GPIOB;
    StaticInit.SdoPin   = GPIO_Pin_5;
    AdcDeviceStaticInit(&StaticInit, AdcDevNum2);

    // ====================== 设备4 ======================
    StaticInit.SckPort = GPIOB;
    StaticInit.SckPin   = GPIO_Pin_14;
    StaticInit.SdoPort = GPIOB;
    StaticInit.SdoPin   = GPIO_Pin_13;
    AdcDeviceStaticInit(&StaticInit, AdcDevNum3);

    // ====================== 设备5 ======================
    StaticInit.SckPort = GPIOA;
    StaticInit.SckPin   = GPIO_Pin_8;
    StaticInit.SdoPort = GPIOB;
    StaticInit.SdoPin   = GPIO_Pin_15;
    AdcDeviceStaticInit(&StaticInit, AdcDevNum0);

    // ====================== 设备6 ======================
    StaticInit.SckPort = GPIOA;
    StaticInit.SckPin   = GPIO_Pin_10;
    StaticInit.SdoPort = GPIOA;
    StaticInit.SdoPin   = GPIO_Pin_9;
    AdcDeviceStaticInit(&StaticInit, AdcDevNum5);

    // ====================== 硬件引脚初始化 ======================
    for (uint8_t i = 0; i < ADC_DEV_NUM; i++)
    {
        HCT6521_SPI_Init((AdcDevNumTypedef)i);
    }

    // ====================== 芯片寄存器配置 ======================
    for (uint8_t i = 0; i < ADC_DEV_NUM; i++)
    {
        HCT6521_Config((AdcDevNumTypedef)i, AdcGain_1, AdcDr_20Hz, AdcRef_External, AdcShi_Disable, AdcMode_Disable);
        SCLK_L((AdcDevNumTypedef)i);    // 让芯片变为AD转换状态
    }

    /* ====================== 状态机初始化 ====================== */
    for (uint8_t i = 0; i < ADC_DEV_NUM; i++)
    {
        AdcDeviceParam[i].RunningParam.AdcFsmParam.AdcFsmState = AdcFsmState_Idle;
        AdcDeviceParam[i].RunningParam.AdcFsmParam.TimeOutCount = 0;
        AdcDeviceParam[i].RunningParam.RawValue = 0;
    }
}

/* ================= 非阻塞状态机 ================= */

/**
  * @brief  读取 24 位 ADC 原始数据
  * @param  DevNum  通道号
  * @retval 24 位原始 ADC 值
  */
static uint32_t Read24Bit(AdcDevNumTypedef DevNum)
{
    uint8_t buffer[3];
    for (uint8_t i = 0; i < 3; i++)
        buffer[i] = IO_ReadByte_Data(DevNum);
    return ((buffer[0] << 16) + (buffer[1] << 8) + (buffer[2])); // buffer[0]=高字节, buffer[1]=中字节, buffer[2]=低字节
}


/**
  * @brief  HCT6521 非阻塞状态机任务，主循环每圈调用一次
  * @param  DevNum    通道号
  * @param  OutValue  输出参数，读取完成时写入 24 位原始 ADC 值
  * @retval 1=本次读取完成，0=尚未完成
  */
uint8_t HCT6521_Task(AdcDevNumTypedef DevNum, int32_t *OutValue)
{
    AdcRunningParamTypedef *pRun = &AdcDeviceParam[DevNum].RunningParam;
    AdcFsmParamTypedef     *pFsm = &pRun->AdcFsmParam;

    switch (pFsm->AdcFsmState)
    {
        /* ========== 等 SDO 变低 ========== */
        case AdcFsmState_Idle:
        {
            if (SDO(DevNum) == 0)   // 数据就绪
            {
                Input(DevNum);
                SCLK_L(DevNum);
                pFsm->AdcFsmState = AdcFsmState_Reading;
            }
            break;
        }

        /* ========== 读 24 位 ========== */
        case AdcFsmState_Reading:
        {
            pRun->RawValue = Read24Bit(DevNum);   // 阻塞 48us

            /* 发 SCK pulse，让 SDO 回高阻（数据手册 4.2 节要求） */
            SCLK_H(DevNum);
            Delay_Us(5);
            SCLK_L(DevNum);
            Delay_Us(5);

            pFsm->AdcFsmState  = AdcFsmState_WaitHigh;
            pFsm->TimeOutCount = 0;
            break;
        }

        /* ========== 等 SDO 回高 ========== */
        case AdcFsmState_WaitHigh:
        {
            if (SDO(DevNum) == 1)   // SDO 回高，本次读完
            {
                *OutValue         = pRun->RawValue;
                pFsm->AdcFsmState  = AdcFsmState_Idle;
                pFsm->TimeOutCount = 0;
                return 1;           // 返回 1 表示本次读取完成
            }

            pFsm->TimeOutCount++;
            if (pFsm->TimeOutCount > 10000)
            {
                pFsm->AdcFsmState  = AdcFsmState_Idle;   // 超时，强制回 IDLE
                pFsm->TimeOutCount = 0;
            }
            break;
        }
    }
    return 0;   // 未完成
}


/* 注意：该功能当前已注释，暂未启用 */
// /**
//  * @brief  控制 HCT6521 芯片进入或退出低功耗模式（睡眠模式）
//  * @param  DevNum  通道号（0~15）
//  * @param  Enable  ENABLE 进入睡眠，DISABLE 唤醒
//  * @note   进入：SCK 拉高 ≥200us，芯片进入睡眠（功耗 0.3uA，VDD_OUT 关闭）
//  *         退出：SCK 拉低 ≥200us，芯片进入转换模式，随后重新初始化寄存器
//  *         退出后第一次读取的数据可能为 0，软件自动丢弃（内部标记）
//  */
// void HCT6521_LowPower(AdcDevNumTypedef DevNum, FunctionalState Enable)
// {
//     if (DevNum >= ADC_DEV_NUM) return;

//     // 获取当前通道的运行参数指针
//     AdcRunningParamTypedef *pRun = &AdcDeviceParam[DevNum].RunningParam;

//     if (Enable == ENABLE) {
//         if (pRun->LowPowerFlag) return;   // 已睡眠则返回

//         Input(DevNum); // SDO 切回输入，避免与芯片高阻态冲突
//         SckSetHigh(DevNum);  // SCK 拉高 ≥100us，进入睡眠
//         Delay_Us(200);

//         pRun->LowPowerFlag = 1;
//         pRun->FirstReadAfterWake = 0;
//     } else {
//         if (!pRun->LowPowerFlag) return;

//         SckSetLow(DevNum); // SCK 拉低 ≥100us，退出睡眠
//         Delay_Us(200);

//         HCT6521_SPI_Init(DevNum);  // 重新初始化 GPIO
//         HCT6521_Config(DevNum, AdcGain_64, AdcDr_20Hz, AdcRef_External, AdcShi_Disable, AdcMode_Disable);  // 重新配置芯片寄存器

//         pRun->LowPowerFlag = 0;
//         pRun->FirstReadAfterWake = 1;
//     }
// }

//// 获取状态函数
//uint8_t HCT6521_IsLowPower(AdcDevNumTypedef DevNum)
//{
//    return AdcDeviceParam[DevNum].RunningParam.LowPowerFlag;
//}
