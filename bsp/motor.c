/**
 * @file    motor.c
 * @brief   BSP 电机驱动实现 (TB6612 + TIM1 PWM)
 */

#include "motor.h"
#include "tim.h"

/* ====================================================================
 *  TB6612 控制逻辑 (以左电机为例):
 *
 *    IN1   IN2   PWM    效果
 *   ─────────────────────────
 *    H     L     >0     正转 (前进)
 *    L     H     >0     反转 (后退)
 *    H     H     —      刹车 (短接制动)
 *    L     L     —      惰行 (自由滑行)
 *
 *   STBY = LOW  → 全部电机停转 (进入待机)
 * ==================================================================== */

/* ========== 初始化 ========== */

void bsp_motor_init(void)
{
    /* 启动 PWM 输出 */
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);  /* 左电机 PA8 */
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);  /* 右电机 PA11 */

    /* 拉高 STBY, 使能电机驱动 */
    HAL_GPIO_WritePin(TB_standby_GPIO_Port, TB_standby_Pin, GPIO_PIN_SET);

    /* 初始状态: 惰行 (IN1=L, IN2=L) */
    bsp_motor_coast();
}

/* ========== 内部辅助 ========== */

/**
 * @brief 设置单个电机的方向和占空比
 * @param in1_port  IN1 端口
 * @param in1_pin   IN1 引脚
 * @param in2_port  IN2 端口
 * @param in2_pin   IN2 引脚
 * @param channel   TIM1 PWM 通道
 * @param pwm       带符号 PWM 值
 */
static void motor_set(GPIO_TypeDef *in1_port, uint16_t in1_pin,
                      GPIO_TypeDef *in2_port, uint16_t in2_pin,
                      uint32_t channel, int32_t pwm)
{
    if (pwm > 0) {
        /* 正转: IN1=H, IN2=L */
        HAL_GPIO_WritePin(in1_port, in1_pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(in2_port, in2_pin, GPIO_PIN_RESET);
        __HAL_TIM_SET_COMPARE(&htim1, channel, (uint32_t)pwm);
    } else if (pwm < 0) {
        /* 反转: IN1=L, IN2=H */
        HAL_GPIO_WritePin(in1_port, in1_pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(in2_port, in2_pin, GPIO_PIN_SET);
        __HAL_TIM_SET_COMPARE(&htim1, channel, (uint32_t)(-pwm));
    } else {
        /* 刹车: IN1=H, IN2=H */
        HAL_GPIO_WritePin(in1_port, in1_pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(in2_port, in2_pin, GPIO_PIN_SET);
        __HAL_TIM_SET_COMPARE(&htim1, channel, 0);
    }
}

/* ========== 单电机控制 ========== */

void bsp_motor_left_set(int32_t pwm)
{
    /* 限幅 */
    if (pwm > MOTOR_PWM_MAX)  pwm = MOTOR_PWM_MAX;
    if (pwm < -MOTOR_PWM_MAX) pwm = -MOTOR_PWM_MAX;

    motor_set(Motor_L_IN1_GPIO_Port, Motor_L_IN1_Pin,
              Motor_L_IN2_GPIO_Port, Motor_L_IN2_Pin,
              TIM_CHANNEL_1, pwm);
}

void bsp_motor_right_set(int32_t pwm)
{
    /* 限幅 */
    if (pwm > MOTOR_PWM_MAX)  pwm = MOTOR_PWM_MAX;
    if (pwm < -MOTOR_PWM_MAX) pwm = -MOTOR_PWM_MAX;

    motor_set(Motor_R_IN1_GPIO_Port, Motor_R_IN1_Pin,
              Motor_R_IN2_GPIO_Port, Motor_R_IN2_Pin,
              TIM_CHANNEL_4, pwm);
}

/* ========== 双电机控制 ========== */

void bsp_motor_both_set(int32_t left, int32_t right)
{
    bsp_motor_left_set(left);
    bsp_motor_right_set(right);
}

void bsp_motor_stop(void)
{
    /* 左右同时刹车: IN1=H, IN2=H, PWM=0 */
    HAL_GPIO_WritePin(Motor_L_IN1_GPIO_Port, Motor_L_IN1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(Motor_L_IN2_GPIO_Port, Motor_L_IN2_Pin, GPIO_PIN_SET);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);

    HAL_GPIO_WritePin(Motor_R_IN1_GPIO_Port, Motor_R_IN1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(Motor_R_IN2_GPIO_Port, Motor_R_IN2_Pin, GPIO_PIN_SET);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, 0);
}

void bsp_motor_coast(void)
{
    /* 左右同时惰行: IN1=L, IN2=L, PWM=0 */
    HAL_GPIO_WritePin(Motor_L_IN1_GPIO_Port, Motor_L_IN1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(Motor_L_IN2_GPIO_Port, Motor_L_IN2_Pin, GPIO_PIN_RESET);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);

    HAL_GPIO_WritePin(Motor_R_IN1_GPIO_Port, Motor_R_IN1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(Motor_R_IN2_GPIO_Port, Motor_R_IN2_Pin, GPIO_PIN_RESET);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, 0);
}
