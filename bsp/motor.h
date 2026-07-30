/**
 * @file    motor.h
 * @brief   BSP 电机驱动接口 (TB6612 + TIM1 PWM)
 *
 * 使用方式:
 *   1. bsp_motor_init()   — 上电后调用一次
 *   2. 设置速度:
 *      bsp_motor_left_set(20000)   → 左轮正向, PWM=20000
 *      bsp_motor_left_set(-15000)  → 左轮反向, PWM=15000
 *   3. 停止:
 *      bsp_motor_stop()   → 刹车 (IN1=H, IN2=H)
 *      bsp_motor_coast()  → 惰行 (IN1=L, IN2=L)
 *
 * 引脚:
 *   左电机: PA8=PWM(TIM1_CH1), PA4=IN1, PA5=IN2
 *   右电机: PA11=PWM(TIM1_CH4), PB0=IN1, PB1=IN2
 *   STBY:   PB12 (H=使能, L=待机)
 */

#ifndef BSP_MOTOR_H
#define BSP_MOTOR_H

#include "main.h"
#include <stdint.h>

/* PWM 范围: TIM1 Period = 65535 */
#define MOTOR_PWM_MAX  65535

/* ========== 初始化 ========== */

/**
 * @brief 初始化电机驱动
 * @note  启动 TIM1 CH1/CH4 PWM, 拉高 STBY
 *         上电后调用一次即可
 */
void bsp_motor_init(void);

/* ========== 单电机控制 ========== */

/**
 * @brief 设置左电机速度
 * @param pwm  -MOTOR_PWM_MAX ~ +MOTOR_PWM_MAX
 *             正数 = 前进, 负数 = 后退, 0 = 刹车
 */
void bsp_motor_left_set(int32_t pwm);

/**
 * @brief 设置右电机速度
 * @param pwm  -MOTOR_PWM_MAX ~ +MOTOR_PWM_MAX
 *             正数 = 前进, 负数 = 后退, 0 = 刹车
 */
void bsp_motor_right_set(int32_t pwm);

/* ========== 双电机控制 ========== */

/**
 * @brief 同时设置左右电机
 * @param left   左轮 PWM
 * @param right  右轮 PWM
 */
void bsp_motor_both_set(int32_t left, int32_t right);

/**
 * @brief 刹车停止 (IN1=H, IN2=H, 电机短接制动)
 */
void bsp_motor_stop(void);

/**
 * @brief 惰行停止 (IN1=L, IN2=L, 电机自由滑行)
 */
void bsp_motor_coast(void);

#endif /* BSP_MOTOR_H */
