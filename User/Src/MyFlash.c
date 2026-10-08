/**
 *********************************************************************************
 * @file    MyFlash.c
 * @brief   片内 Flash 数据管理（快速模式，256B 块）
 * @note    内部维护一份 256B 的 buf 作为 Flash 的 RAM 镜像，
 *          所有 Set 只改 buf，Save 才真正擦写 Flash。
 *          数据本体（标定表、倍率）全部由外部通过指针传入。
 *********************************************************************************
 */

#include "MyFlash.h"
#include <string.h>


/* Flash 数据的 RAM 镜像，位于 .bss，不占栈 */
static uint8_t buf[MYFLASH_BLOCK_SIZE];

/* 结构体视图：方便按字段访问 buf，避免手算偏移 */
static MyFlashParamTypedef * const view = (MyFlashParamTypedef *)buf;

/**
 * @brief  把 buf 整体擦写进 Flash（一次擦 + 一次写）
 * @retval 1 成功，0 失败
 */
static uint8_t MyFlash_WriteBlock(void)
{
    FLASH_Status state;

    /* 擦除 256 字节（2 个 128B 快速页） */
    state = FLASH_ROM_ERASE(MYFLASH_ADDR, MYFLASH_BLOCK_SIZE);
    if (state != FLASH_COMPLETE)
    {
        printf("MyFlash: 擦除失败, status=0x%X\r\n", state);
        return 0;
    }

    /* 一次性写 256 字节：地址 128 对齐，长度 128 倍数，满足快速编程要求 */
    state = FLASH_ROM_WRITE(MYFLASH_ADDR, (uint32_t *)buf, MYFLASH_BLOCK_SIZE);
    if (state != FLASH_COMPLETE)
    {
        printf("MyFlash: 写入失败, status=0x%X\r\n", state);
        return 0;
    }

    return 1;
}

/* ============================ 公开接口 ============================ */

uint8_t MyFlash_Init(uint32_t *pCal, float *pRate)
{
    const MyFlashParamTypedef *flash = (const MyFlashParamTypedef *)MYFLASH_ADDR;

    if (pCal == NULL || pRate == NULL)
    {
        printf("MyFlash: Init 参数为空\r\n");
        return 0;
    }

    if (flash->magic == MYFLASH_MAGIC)
    {
        /* ---- 有数据：把 Flash 内容整体读到 buf ---- */
        memcpy(buf, (const void *)MYFLASH_ADDR, MYFLASH_BLOCK_SIZE);

        /* ---- 再分发到外部指针 ---- */
        memcpy(pCal, view->cal, MYFLASH_CAL_SIZE);
        memcpy(pRate, view->rate, sizeof(view->rate));

        printf("MyFlash: 从 Flash 加载\r\n");
        return 1;
    }
    else
    {
        /* ---- 新 Flash：用外部指针的当前值填 buf，再写回 ---- */
        memset(buf, 0xFF, sizeof(buf));
        view->magic = MYFLASH_MAGIC;
        memcpy(view->cal, pCal, MYFLASH_CAL_SIZE);
        memcpy(view->rate, pRate, sizeof(view->rate));

        printf("MyFlash: 新 Flash, 写入当前默认值\r\n");
        return MyFlash_WriteBlock();
    }
}

uint8_t MyFlash_Save(void)
{
    /* 保底再写一次 magic，防止外部误改 buf */
    view->magic = MYFLASH_MAGIC;

    return MyFlash_WriteBlock();
}

uint8_t MyFlash_SetCal(const uint32_t *pCal)
{
    if (pCal == NULL)
    {
        printf("MyFlash: SetCal 参数为空\r\n");
        return 0;
    }

    memcpy(view->cal, pCal, MYFLASH_CAL_SIZE);
    return 1;
}

uint8_t MyFlash_SetRate(const float *pRate)
{
    if (pRate == NULL)
    {
        printf("MyFlash: SetRate 参数为空\r\n");
        return 0;
    }
    memcpy(view->rate, pRate, sizeof(view->rate));
    return 1;
}

uint8_t MyFlash_GetCal(uint32_t *pCal)
{
    if (pCal == NULL)
    {
        printf("MyFlash: GetCal 参数为空\r\n");
        return 0;
    }

    memcpy(pCal, view->cal, MYFLASH_CAL_SIZE);
    return 1;
}

uint8_t MyFlash_GetRate(float *pRate)
{
    if (pRate == NULL)
    {
        printf("MyFlash: GetRate 参数为空\r\n");
        return 0;
    }

    memcpy(pRate, view->rate, sizeof(view->rate));
    return 1;
}

