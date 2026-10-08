/**
 *********************************************************************************
 * @file    SM16028E.c
 * @brief   SM16028E 灰度数码管显示驱动（GPIO 模拟两线时序）
 * @note    内部维护每通道的显示缓冲区，SM28_Update 时把缓冲区整帧发送到芯片。
 *********************************************************************************
 */

#include "SM16028E.h"
#include "Project_Config.h"
#include "debug.h"          // 用于 Delay_Us / Delay_Ms
#include <string.h>

/* ==================== 设备参数数组（静态） ==================== */
static SM28DeviceParamTypedef SM28DeviceParam[SM28_DEV_NUM];

/* ==================== 显示位号配置数组 ==================== */
// 对应实际数码管位置，数字位纯根据实际配置，不同接线对应不同数字位
// 最大力值显示：5个数字位（小数、个、十、百、千）
#define MAX_FORCE_DIGITS_LEN   5
static const uint8_t MaxForceDigits[MAX_FORCE_DIGITS_LEN] = {4, 2, 0, 15, 13};

// 游戏计数显示：5个数字位（个、十、百、千、万）
#define COUNT_DIGITS_LEN       5
static const uint8_t CountDigits[COUNT_DIGITS_LEN] = {5, 3, 1, 10, 8};

// 运行时间显示：6个数字位（个、十、百、千、万、十万）
#define TIME_DIGITS_LEN        6
static const uint8_t TimeDigits[TIME_DIGITS_LEN] = {11, 9, 7, 14, 12, 6};
/* ========================= 字模表 ========================= */
/**
 * @brief 字符字模表（共38个字符，每个字符8列，对应SM16028E的8个SEG输出）
 * @note  每行8个字节分别对应SEG1~SEG8（即LED的8列），0x00=灭，0xF5=亮。
 *        索引0~9：数字0~9；10~32：字母/符号；33~37：特殊图形。
 */
static const uint8_t CharSegments[][8] = {
    {0xF5, 0xF5, 0xF5, 0xF5, 0xF5, 0xF5, 0x00, 0x00},   // 索引0: '0'
    {0x00, 0xF5, 0xF5, 0x00, 0x00, 0x00, 0x00, 0x00},   // 索引1: '1'
    {0xF5, 0xF5, 0x00, 0xF5, 0xF5, 0x00, 0xF5, 0x00},   // 索引2: '2'
    {0xF5, 0xF5, 0xF5, 0xF5, 0x00, 0x00, 0xF5, 0x00},   // 索引3: '3'
    {0x00, 0xF5, 0xF5, 0x00, 0x00, 0xF5, 0xF5, 0x00},   // 索引4: '4'
    {0xF5, 0x00, 0xF5, 0xF5, 0x00, 0xF5, 0xF5, 0x00},   // 索引5: '5'
    {0xF5, 0x00, 0xF5, 0xF5, 0xF5, 0xF5, 0xF5, 0x00},   // 索引6: '6'
    {0xF5, 0xF5, 0xF5, 0x00, 0x00, 0x00, 0x00, 0x00},   // 索引7: '7'
    {0xF5, 0xF5, 0xF5, 0xF5, 0xF5, 0xF5, 0xF5, 0x00},   // 索引8: '8'
    {0xF5, 0xF5, 0xF5, 0xF5, 0x00, 0xF5, 0xF5, 0x00},   // 索引9: '9'

    {0xF5, 0xF5, 0xF5, 0x00, 0xF5, 0xF5, 0xF5, 0x00},   // 索引10: 'A'
    {0x00, 0x00, 0xF5, 0xF5, 0xF5, 0xF5, 0xF5, 0x00},   // 索引11: 'b'
    {0xF5, 0x00, 0x00, 0xF5, 0xF5, 0xF5, 0x00, 0x00},   // 索引12: 'C'
    {0x00, 0x00, 0xF5, 0xF5, 0xF5, 0x00, 0xF5, 0x00},   // 索引13: 'c'
    {0x00, 0xF5, 0xF5, 0xF5, 0xF5, 0x00, 0xF5, 0x00},   // 索引14: 'd'
    {0xF5, 0x00, 0x00, 0xF5, 0xF5, 0xF5, 0xF5, 0x00},   // 索引15: 'E'
    {0xF5, 0x00, 0x00, 0x00, 0xF5, 0xF5, 0xF5, 0x00},   // 索引16: 'F'
    {0x00, 0xF5, 0xF5, 0x00, 0xF5, 0xF5, 0xF5, 0x00},   // 索引17: 'H'
    {0x00, 0x00, 0xF5, 0x00, 0xF5, 0xF5, 0xF5, 0x00},   // 索引18: 'h'
    {0x00, 0x00, 0xF5, 0x00, 0x00, 0x00, 0x00, 0x00},   // 索引19: 'i'

    {0x00, 0xF5, 0xF5, 0xF5, 0x00, 0x00, 0x00, 0x00},   // 索引20: 'J'
    {0x00, 0x00, 0x00, 0xF5, 0xF5, 0xF5, 0x00, 0x00},   // 索引21: 'L'
    {0xF5, 0xF5, 0xF5, 0x00, 0xF5, 0xF5, 0x00, 0x00},   // 索引22: 'N'
    {0x00, 0x00, 0xF5, 0x00, 0xF5, 0x00, 0xF5, 0x00},   // 索引23: 'n'
    {0xF5, 0xF5, 0xF5, 0xF5, 0xF5, 0xF5, 0x00, 0x00},   // 索引24: 'O'
    {0xF5, 0xF5, 0x00, 0x00, 0xF5, 0xF5, 0xF5, 0x00},   // 索引25: 'p'
    {0xF5, 0xF5, 0xF5, 0x00, 0x00, 0xF5, 0xF5, 0x00},   // 索引26: 'q'
    {0x00, 0x00, 0x00, 0x00, 0xF5, 0x00, 0xF5, 0x00},   // 索引27: 'r'
    {0xF5, 0x00, 0xF5, 0xF5, 0x00, 0xF5, 0xF5, 0x00},   // 索引28: 'S'
    {0x00, 0x00, 0x00, 0xF5, 0xF5, 0xF5, 0xF5, 0x00},   // 索引29: 't'

    {0xF5, 0xF5, 0xF5, 0xF5, 0xF5, 0xF5, 0x00, 0x00},   // 索引30: 'U'
    {0x00, 0x00, 0xF5, 0xF5, 0xF5, 0x00, 0x00, 0x00},   // 索引31: 'u'
    {0x00, 0xF5, 0xF5, 0xF5, 0x00, 0xF5, 0xF5, 0x00},   // 索引32: 'y'
    {0xF5, 0xF5, 0xF5, 0xF5, 0xF5, 0xF5, 0xF5, 0xF5},   // 索引33: 'ALL'（全亮）
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // 索引34: 'OFF'（全灭）
    {0xF5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // 索引35: '￣'（上横线）
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF5, 0x00},   // 索引36: '-'（中横线）
    {0x00, 0x00, 0x00, 0xF5, 0x00, 0x00, 0x00, 0x00}    // 索引37: '_'（下横线）
};


/* ==================== 静态辅助函数（底层硬件操作） ==================== */
/**
 * @brief 发送一个字节（模拟双线协议，高位先传）
 * @param pSta 设备静态参数指针
 * @param Byte 待发送的8位数据
 */
static void SM28_SendByte(SM28StaticParamTypedef *pSta, uint8_t Byte)
{
    GPIO_TypeDef *sdaPort = pSta->SdaPort;
    uint16_t sdaPin = pSta->SdaPin;
    GPIO_TypeDef *sclPort = pSta->SclPort;
    uint16_t sclPin = pSta->SclPin;

    uint8_t i;
    // 起始：SCL低，SDA低
    GPIO_ResetBits(sclPort, sclPin);
    GPIO_ResetBits(sdaPort, sdaPin);

    for (i = 0; i < 8; i++)
    {
        GPIO_SetBits(sclPort, sclPin);   // SCL高，芯片采样
        GPIO_SetBits(sdaPort, sdaPin);   // 先拉高，再根据数据位决定

        if (Byte & 0x80)
        {
            GPIO_SetBits(sdaPort, sdaPin);  // 发送1
        }
        else
        {
            GPIO_ResetBits(sdaPort, sdaPin); // 发送0
        }

        GPIO_ResetBits(sclPort, sclPin);   // SCL低，完成一位
        Byte <<= 1;
        GPIO_ResetBits(sdaPort, sdaPin);   // 拉低 SDA，为下一位准备
    }
}

/**
 * @brief 初始化SM16028E通信接口的GPIO引脚（模拟双线协议）
 * @param pSta  设备静态参数指针（包含 SDA/SCL 端口和引脚）
 * @note
 *        初始化后默认输出高电平，符合SM16028E的空闲电平要求。
 *        该函数应在系统启动时调用一次。
 */
static void SM28_GPIO_Init(SM28StaticParamTypedef *pSta)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    // 使能对应端口时钟
    if (pSta->SdaPort == GPIOA) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    else if (pSta->SdaPort == GPIOB) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    else if (pSta->SdaPort == GPIOC) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    else if (pSta->SdaPort == GPIOD) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);
    else if (pSta->SdaPort == GPIOE) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE);

    if (pSta->SclPort != pSta->SdaPort)
    {
        if (pSta->SclPort == GPIOA) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
        else if (pSta->SclPort == GPIOB) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
        else if (pSta->SclPort == GPIOC) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
        else if (pSta->SclPort == GPIOD) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);
        else if (pSta->SclPort == GPIOE) RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE);
    }

    // 配置 SDA 推挽输出
    GPIO_InitStruct.GPIO_Pin = pSta->SdaPin;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(pSta->SdaPort, &GPIO_InitStruct);

    // 配置 SCL 推挽输出
    GPIO_InitStruct.GPIO_Pin = pSta->SclPin;
    GPIO_Init(pSta->SclPort, &GPIO_InitStruct);

    GPIO_SetBits(pSta->SdaPort, pSta->SdaPin);   // 空闲高
    GPIO_SetBits(pSta->SclPort, pSta->SclPin);   // 空闲高
}

/* ==================== 内部缓冲区操作函数 ==================== */
/**
 * @brief 清空显示缓冲区（全部熄灭）
 * @param pRun  运行参数指针
 */
static void SM28_ClearBuffer(SM28RunningParamTypedef *pRun)
{
    memset(pRun->Buffer + SM28_DATA_START, 0x00, SM28_DATA_LEN);
}


/**
 * @brief 设置缓冲区中某个字节的值（用于点亮/熄灭某段）
 * @param pRun      运行参数指针
 * @param byteIdx   灰度数据索引（8~135）
 * @param value     写入值（0x00 熄灭，0xF5 点亮）
 */
static void SM28_SetBufferByte(SM28RunningParamTypedef *pRun, uint8_t byteIdx, uint8_t value)
{
    if (byteIdx >= SM28_DATA_START && byteIdx < SM28_BUFFER_SIZE)
    {
        pRun->Buffer[byteIdx] = value;
    }
}


/**
 * @brief 在指定数字位显示字符（从字模表复制）
 * @param pRun      运行参数指针
 * @param digit     数字位（0~15）
 * @param charIdx   字符索引（0~37）
 */
static void SM28_SetChar(SM28RunningParamTypedef *pRun, uint8_t digit, uint8_t charIdx)
{
    if (digit >= SM28_DIGIT_NUM || charIdx >= (sizeof(CharSegments)/sizeof(CharSegments[0])))
        return;

    uint16_t base = SM28_DATA_START + digit * SM28_BYTES_PER_DIGIT;
    for (uint8_t i = 0; i < 8; i++)   // 只复制前8个SEG
    {
        if (base + i < SM28_BUFFER_SIZE)
        {
            pRun->Buffer[base + i] = CharSegments[charIdx][i];
        }
    }
}


/* ==================== 对外接口实现 ==================== */


/**
 * @brief 初始化 SM16028E 设备（引脚 + 缓冲区 + 默认亮度）
 * @param DevNum 设备号（0 ~ SM28_DEV_NUM-1）
 */
void SM28_Init(SM28DevNumTypedef DevNum)
{
    if (DevNum >= SM28_DEV_NUM) return;

    SM28StaticParamTypedef  *pSta = &SM28DeviceParam[DevNum].StaticParam;
    SM28RunningParamTypedef *pRun = &SM28DeviceParam[DevNum].RunningParam;

    pSta->SclPort   = GPIOC;
    pSta->SclPin    = GPIO_Pin_14;
    pSta->SdaPort   = GPIOC;
    pSta->SdaPin    = GPIO_Pin_15;

    // 1. 初始化 GPIO
    SM28_GPIO_Init(pSta);

    // 2. 清空缓冲区
    SM28_ClearBuffer(pRun);

    // 3. 设置默认亮度（中等）
    pRun->Brightness = 0x55;   // 对应原寄存器2的值（可根据需要调整）

    // 4. 其他运行参数清零
    pRun->DotPos = SM28_DOT_NONE;
    pRun->IsNegative = 0;
    pRun->UpdateCounter = 0;

    // 5. 首次刷新硬件（清屏）
    SM28_Update(DevNum);
}

/**
 * @brief 清空显示（所有LED熄灭）
 * @param DevNum 设备号
 */
void SM28_Clear(SM28DevNumTypedef DevNum)
{
    if (DevNum >= SM28_DEV_NUM) return;
    SM28RunningParamTypedef *pRun = &SM28DeviceParam[DevNum].RunningParam;
    SM28_ClearBuffer(pRun);
    SM28_Update(DevNum);
}


/**
 * @brief 全屏点亮（所有LED亮起）
 * @param DevNum 设备号
 */
void SM28_AllOn(SM28DevNumTypedef DevNum)
{
    if (DevNum >= SM28_DEV_NUM) return;
    SM28RunningParamTypedef *pRun = &SM28DeviceParam[DevNum].RunningParam;
    for (uint16_t i = SM28_DATA_START; i < SM28_BUFFER_SIZE; i++)
    {
        pRun->Buffer[i] = 0xF5;
    }
    SM28_Update(DevNum);
}


/**
 * @brief 在指定数字位显示一个字符（来自字模表）
 * @param DevNum  设备号
 * @param Digit   数字位（0~15，0为最左）
 * @param CharIdx 字符索引（Char_xxx）
 */
void SM28_ShowChar(SM28DevNumTypedef DevNum, uint8_t Digit, SM28CharIndexTypedef CharIdx)
{
    if (DevNum >= SM28_DEV_NUM) return;
    SM28RunningParamTypedef *pRun = &SM28DeviceParam[DevNum].RunningParam;
    SM28_SetChar(pRun, Digit, (uint8_t)CharIdx);
    // 注意：不立即更新，由外部调用 SM28_Update 统一发送
}


/**
 * @brief 显示最大力值（带一位小数）
 * @param DevNum      设备号
 * @param ForceValue  力值（浮点数，例如 1234.5）
 * @param LeadingZero 前导零消隐：0=消隐，1=显示
 * @note  显示格式为：4位整数 + 1位小数（如 1234.5），输入范围 -9999.9 ~ 9999.9，
 *        当前仅显示绝对值，负号未处理。
 */
void SM28_ShowMaxForce(SM28DevNumTypedef DevNum, float ForceValue, uint8_t LeadingZero)
{
    // 限幅到可显示范围（-9999.9 ~ 9999.9）
    if (ForceValue > 9999.9f) ForceValue = 9999.9f;
    if (ForceValue < -9999.9f) ForceValue = -9999.9f;

    // 1. 处理负数（取绝对值，如需显示负号可在此处扩展）
    if (ForceValue < 0) ForceValue = -ForceValue;

    // 2. 分离整数部分和小数部分（四舍五入到1位小数）
    int32_t intPart  = (int32_t)ForceValue;
    uint8_t fracPart = (uint8_t)((ForceValue - intPart) * 10 + 0.5f);
    if (fracPart >= 10)
    {
        fracPart = 0;
        intPart += 1;
    }

    // 3. 提取整数部分的各位数字，低位存放个位（digits[1]=个位, digits[2]=十位, digits[3]=百位, digits[4]=千位）
    //    同时 digits[0] 存放小数位
    uint8_t digits[MAX_FORCE_DIGITS_LEN] = {0};
    digits[0] = fracPart;              // 小数位
    int32_t temp = intPart;
    for (int i = 1; i < 5; i++)
    {
        digits[i] = temp % 10;
        temp /= 10;
    }

    // 4. 前导零消隐（只消隐千、百、十位，个位始终显示，小数位不消隐）
    if (LeadingZero == 0)
    {
        // 依次检查千位(digits[4])、百位(digits[3])、十位(digits[2])，个位(digits[1])不消隐
        for (uint8_t i = MAX_FORCE_DIGITS_LEN - 1; i > 1; i--)
        {
            if (digits[i] == 0)
            {
                digits[i] = Char_OFF;       // 用空白代替0
            }
            else
            {
                break;                      // 遇到第一个非零数字后停止
            }
        }
        // 个位(digits[1])始终显示，小数位(digits[0])也始终显示
    }

    // 5. 显示到对应数码管位置（使用 MaxForceDigits 布局数组，顺序：小数、个、十、百、千）
    for (uint8_t i = 0; i < MAX_FORCE_DIGITS_LEN; i++)
    {
        SM28_ShowChar(DevNum, MaxForceDigits[i], (SM28CharIndexTypedef)digits[i]);
    }

    // 6. 点亮小数点（个位对应的位置是 MaxForceDigits[1]，使用 SM28_SetPixel）
    //    每个数字位占用8个SEG，小数点位于该位的第8个SEG（偏移7）
    uint8_t dotByteIdx = MaxForceDigits[1] * SM28_BYTES_PER_DIGIT + SM28_DOT_OFFSET;
    SM28_SetPixel(DevNum, dotByteIdx, 1);

    // 注意：本函数只修改缓冲区，实际刷新需调用 SM28_Update(DevNum)
}

/**
 * @brief 显示游戏计数（例如击中次数或总轮数）
 * @param DevNum      设备号
 * @param CountValue  要显示的计数值（0~99999）
 * @param LeadingZero 前导零消隐：0=消隐，1=显示
 * @note  显示最多5位数字（个、十、百、千、万），超出范围钳位到最大值。
 *        使用 CountDigits 数组映射到具体数码管位置。
 */
void SM28_ShowGameCount(SM28DevNumTypedef DevNum, int32_t CountValue, uint8_t LeadingZero)
{
    // 限制显示范围（0~99999），负数按0处理
    if (CountValue < 0) CountValue = 0;
    if (CountValue > 99999) CountValue = 99999;
    // if (CountValue < 0) CountValue = -CountValue;   // 负数转正（计数不应为负）

    // digits[0] = 个位, digits[1] = 十位, digits[2] = 百位, digits[3] = 千位, digits[4] = 万位
    uint8_t digits[COUNT_DIGITS_LEN] = {0};  // 只取5位，因为 CountDigits 只有5个元素
    int32_t temp = CountValue;

    // 提取每一位数字（低位优先，个位存入 digits[0]）
    for (int i = 0; i < 5; i++)
    {
        digits[i] = temp % 10;
        temp /= 10;
    }

    // ---- 前导零消隐（从最高位（万位）向下检查，个位不消隐） ----
    if (LeadingZero == 0)
    {
        for (int i = COUNT_DIGITS_LEN - 1; i > 0; i--)   // i=4 万位, i=3 千位, i=2 百位, i=1 十位；跳过个位 (i=0)
        {
            if (digits[i] == 0)
            {
                digits[i] = Char_OFF;   // 用空白代替0
            }
            else
            {
                break;                  // 遇到第一个非零数字，停止消隐
            }
        }
        // 个位 (digits[0]) 始终显示，即使为0也显示 '0'
    }

    // 显示到对应数码管位置（CountDigits[0]=个位, [1]=十位, [2]=百位, [3]=千位, [4]=万位）
    for (uint8_t i = 0; i < COUNT_DIGITS_LEN; i++)
    {
        SM28_ShowChar(DevNum, CountDigits[i], (SM28CharIndexTypedef)digits[i]);
    }

}


/**
 * @brief 显示运行时间（单位毫秒，显示最多6位数字）
 * @param DevNum      设备号
 * @param TimeValue   时间值（0~999999）
 * @param LeadingZero 前导零消隐：0=消隐，1=显示
 * @note  使用 TimeDigits 数组映射到具体数码管位置。
 *        显示最多6位：个、十、百、千、万、十万。
 */
void SM28_ShowRunTime(SM28DevNumTypedef DevNum, int32_t TimeValue, uint8_t LeadingZero)
{
    // 限制显示范围（0~999999），负数按0处理
    if (TimeValue < 0) TimeValue = 0;
    if (TimeValue > 999999) TimeValue = 999999;
    // if (TimeValue < 0) TimeValue = -TimeValue;   // 负数转正（时间不应为负）

    // digits[0] = 个位, digits[1] = 十位, digits[2] = 百位, digits[3] = 千位, digits[4] = 万位, digits[5] = 十万位
    uint8_t digits[TIME_DIGITS_LEN] = {0};  // 因为 TimeDigits 有6个元素
    int32_t temp = TimeValue;

    // 提取每一位数字（低位优先，个位存入 digits[0]）
    for (int i = 0; i < 6; i++)
    {
        digits[i] = temp % 10;
        temp /= 10;
    }

    // ---- 前导零消隐（从最高位（十万位）向下检查，个位不消隐） ----
    if (LeadingZero == 0)
    {
        for (int i = TIME_DIGITS_LEN - 1; i > 0; i--)   // i=5 十万位, i=4 万位, ... i=1 十位；跳过个位 (i=0)
        {
            if (digits[i] == 0)
            {
                digits[i] = Char_OFF;   // 用空白代替0
            }
            else
            {
                break;                  // 遇到第一个非零数字，停止消隐
            }
        }
        // 个位 (digits[0]) 始终显示，即使为0也显示 '0'
    }

    // 显示到对应数码管位置（TimeDigits[0]=个位, [1]=十位, [2]=百位, [3]=千位, [4]=万位, [5]=十万位）
    for (uint8_t i = 0; i < TIME_DIGITS_LEN; i++)
    {
        SM28_ShowChar(DevNum, TimeDigits[i], (SM28CharIndexTypedef)digits[i]);
    }

}


/**
 * @brief 直接控制单个LED点
 * @param DevNum   设备号
 * @param ByteIdx  灰度数据索引（0~127）
 * @param State    1=点亮，0=熄灭
 */
void SM28_SetPixel(SM28DevNumTypedef DevNum, uint8_t ByteIdx, uint8_t State)
{
    ByteIdx = ByteIdx + 8; // 跳过前八个字节的命令头
    if (DevNum >= SM28_DEV_NUM) return;
    SM28RunningParamTypedef *pRun = &SM28DeviceParam[DevNum].RunningParam;
    SM28_SetBufferByte(pRun, ByteIdx, State ? 0xF5 : 0x00);
}

/**
 * @brief 设置亮度
 * @param DevNum    设备号
 * @param Brightness 亮度值（0~255，对应寄存器2的值）
 */
void SM28_SetBrightness(SM28DevNumTypedef DevNum, uint8_t Brightness)
{
    if (DevNum >= SM28_DEV_NUM) return;
    SM28DeviceParam[DevNum].RunningParam.Brightness = Brightness;
}


/**
 * @brief 刷新显示（将缓冲区数据发送到芯片）
 * @param DevNum 设备号
 */
void SM28_Update(SM28DevNumTypedef DevNum)
{
    if (DevNum >= SM28_DEV_NUM) return;
    SM28StaticParamTypedef  *pSta   = &SM28DeviceParam[DevNum].StaticParam;
    SM28RunningParamTypedef *pRun   = &SM28DeviceParam[DevNum].RunningParam;

    uint8_t *buf = pRun->Buffer;
    uint16_t sum = 0;
    uint8_t i;

    // ---- 组装指令头（前8字节） ----
    buf[0] = 0xAA;  // Byte0: 写指令识别码（固定0xAA）
    buf[1] = 0x00;  // Byte1: 起始地址（从0x00开始）
    buf[2] = 0x04;  // Byte2: 寄存器数量 M=4（表示写入5个寄存器，即寄存器0~4）
    buf[3] = 0x07;  // Byte3: 寄存器0 配置
                    //         bit[3:0] = 扫描行数-1 (0x07 → 8扫)
                    //         bit[6:5] = PWM频率 (0b00 → 3.7KHz)
                    //         bit[7]   = RGB模式 (0 → 非RGB)
    buf[4] = 0xFD;  // Byte4: 寄存器1 配置
                    //         bit[7:4] = 0xF
                    //         bit[3]   = REXT选择 (1 → 内置电阻)
                    //         bit[2]   = 1 (默认值)
                    //         bit[1:0] = 更新模式 (0b01 → 数据帧直接更新)
    buf[5] = pRun->Brightness;// Byte5: 亮度寄存器
    buf[6] = 0x00;  // Byte6: 寄存器2 配置（非RGB模式下的电流增益）
    buf[7] = 0x00;  // Byte7: 寄存器4 配置（RGB模式B灯增益，非RGB模式不用）

    // ---- 计算校验和（对前136字节求和取反） ----
    for (i = 0; i < SM28_BUFFER_SIZE; i++)
    {
        sum += buf[i];
    }
    sum = ~sum;

    // ---- 发送数据 ----
    for (i = 0; i < SM28_BUFFER_SIZE; i++)
    {
        SM28_SendByte(pSta, buf[i]);
    }
    SM28_SendByte(pSta, (uint8_t)(sum & 0xFF));
    SM28_SendByte(pSta, 0x00);   // 尾随字节

    pRun->UpdateCounter++;
}

/**
 * @brief 获取当前显示缓冲区指针（用于直接操作）
 * @param DevNum 设备号
 * @retval 指向显示缓冲区的指针
 */
uint8_t* SM28_GetBuffer(SM28DevNumTypedef DevNum)
{
    if (DevNum >= SM28_DEV_NUM) return NULL;
    return SM28DeviceParam[DevNum].RunningParam.Buffer;
}

/**
 * @brief 获取设备参数指针（调试用）
 * @param DevNum 设备号
 * @retval 指向该设备参数结构体的常量指针
 */
const SM28DeviceParamTypedef* SM28_GetParam(SM28DevNumTypedef DevNum)
{
    if (DevNum >= SM28_DEV_NUM) return NULL;
    return &SM28DeviceParam[DevNum];
}


