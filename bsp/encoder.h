/**
 * @file    encoder.h
 * @brief   BSP 编码器接口 (TIM2/TIM3 编码器模式)
 *
 * 使用方式:
 *   1. bsp_encoder_init()       — 上电后调用一次, 计数归零
 *   2. 读当前值:
 *      int32_t L = bsp_encoder_left_get();
 *      int32_t R = bsp_encoder_right_get();
 *   3. 差分读取 (适合 PID 循环):
 *      static int32_t prev_L = 0;
 *      int32_t delta = bsp_encoder_left_delta(&prev_L);
 *   4. bsp_encoder_both_reset() — 两路同时归零
 *
 * 引脚:
 *   左编码器: PA0=TIM2_CH1, PA1=TIM2_CH2
 *   右编码器: PA6=TIM3_CH1, PA7=TIM3_CH2
 *
 * 编码器模式: TI12 (双边沿 ×4)
 *   物理 N 脉冲/圈 → 编码器计 4N 次/圈
 */

#ifndef BSP_ENCODER_H
#define BSP_ENCODER_H

#include "main.h"
#include <stdint.h>

/* ========== 初始化 ========== */

/**
 * @brief 初始化编码器, 启动 TIM2/TIM3 计数
 * @note  上电后调用一次, 计数从 0 开始
 */
void bsp_encoder_init(void);

/* ========== 绝对值读取 ========== */

/**
 * @brief 读取左编码器累计计数值
 * @return 当前计数值 (int32_t, 正=前进, 负=后退)
 */
int32_t bsp_encoder_left_get(void);

/**
 * @brief 读取右编码器累计计数值
 * @return 当前计数值 (int32_t, 正=前进, 负=后退)
 */
int32_t bsp_encoder_right_get(void);

/* ========== 差分读取 ========== */

/**
 * @brief 读取左编码器增量并更新基准值
 * @param prev  基准值指针 (传入上次的值, 传出本次的值)
 * @return      两次读取间的增量
 *
 * 典型用法 (PID 循环中):
 *   static int32_t prev_L = 0;
 *   int32_t delta_L = bsp_encoder_left_delta(&prev_L);
 */
int32_t bsp_encoder_left_delta(int32_t *prev);

/**
 * @brief 读取右编码器增量并更新基准值
 */
int32_t bsp_encoder_right_delta(int32_t *prev);

/* ========== 归零 ========== */

/**
 * @brief 左右编码器同时归零
 */
void bsp_encoder_both_reset(void);

#endif /* BSP_ENCODER_H */
