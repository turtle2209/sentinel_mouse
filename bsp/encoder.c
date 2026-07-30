/**
 * @file    encoder.c
 * @brief   BSP 编码器实现 (TIM2/TIM3 编码器模式)
 *
 * 编码器模式说明:
 *   TIM2/TIM3 配置为 Encoder Mode TI12 (四倍频)
 *   物理编码器每圈 N 个脉冲 → CNT 计 4N 次
 *   CNT 为 16 位 (0~65535), 自动换向卷绕
 *
 * 溢出处理:
 *   用 int16_t 差值自动处理 16 位卷绕:
 *     int16_t delta = (int16_t)(curr - prev)
 *   无论 curr 和 prev 如何卷绕, delta 永远正确 (±32767 以内)
 */

#include "encoder.h"
#include "tim.h"

/* 软件扩展的 32 位累计值 (解决 16 位 CNT 频繁溢出的问题) */
static volatile int32_t enc_left_accum;   /* 左编码器累计 */
static volatile int32_t enc_right_accum;  /* 右编码器累计 */
static volatile uint16_t enc_left_last;   /* 上次 CNT 原始值 */
static volatile uint16_t enc_right_last;  /* 上次 CNT 原始值 */

/* ========== 初始化 ========== */

void bsp_encoder_init(void)
{
    /* CNT 归零 */
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    __HAL_TIM_SET_COUNTER(&htim3, 0);

    /* 软件累计归零 */
    enc_left_accum  = 0;
    enc_right_accum = 0;
    enc_left_last   = 0;
    enc_right_last  = 0;
}

/* ========== 内部: 读取并累积 ========== */

/**
 * @brief 读一次编码器, 累加溢出差值到 accum
 *
 * 原理:
 *   uint16_t raw = TIMx->CNT;
 *   int16_t  delta = (int16_t)(raw - last);
 *   accum += delta;
 *   last = raw;
 *   return accum;
 *
 * 为什么 int16_t 差值能处理卷绕:
 *   raw=65535, last=0     → delta = (int16_t)(65535) = -1   ✓
 *   raw=0,     last=65535 → delta = (int16_t)(-65535) = 1   ✓
 *   只要两次读取间实际位移 < 32768 个编码器 tick, 就不会出错
 */
static int32_t encoder_read(TIM_HandleTypeDef *htim,
                             volatile int32_t *accum,
                             volatile uint16_t *last)
{
    uint16_t raw = (uint16_t)__HAL_TIM_GET_COUNTER(htim);
    int16_t delta = (int16_t)(raw - *last);
    *accum += (int32_t)delta;
    *last = raw;
    return *accum;
}

/* ========== 绝对值读取 ========== */

int32_t bsp_encoder_left_get(void)
{
    return encoder_read(&htim2, &enc_left_accum, &enc_left_last);
}

int32_t bsp_encoder_right_get(void)
{
    return encoder_read(&htim3, &enc_right_accum, &enc_right_last);
}

/* ========== 差分读取 ========== */

int32_t bsp_encoder_left_delta(int32_t *prev)
{
    int32_t curr = bsp_encoder_left_get();
    int32_t delta = curr - *prev;
    *prev = curr;
    return delta;
}

int32_t bsp_encoder_right_delta(int32_t *prev)
{
    int32_t curr = bsp_encoder_right_get();
    int32_t delta = curr - *prev;
    *prev = curr;
    return delta;
}

/* ========== 归零 ========== */

void bsp_encoder_both_reset(void)
{
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    __HAL_TIM_SET_COUNTER(&htim3, 0);
    enc_left_accum  = 0;
    enc_right_accum = 0;
    enc_left_last   = 0;
    enc_right_last  = 0;
}
