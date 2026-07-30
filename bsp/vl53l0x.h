/**
 * @file    vl53l0x.h
 * @brief   BSP VL53L0X 激光测距驱动接口
 *
 * 硬件:
 *   I2C 共享总线 (PB10=SCL, PB11=SDA, 推挽 bit-bang)
 *   XSHUT: PC0 (正前方)
 *   默认地址: 0x29
 *
 * 使用方式:
 *   bsp_vl53l0x_init();                    // 上电后调用一次
 *   uint16_t d = bsp_vl53l0x_read();       // 单次读取 (mm)
 *   bool ok = bsp_vl53l0x_is_ready();      // 查询传感器是否就绪
 *
 * 与 VL53L1X 的区别:
 *   - 8 位寄存器地址 (L1X 是 16 位)
 *   - 默认地址 0x29 (L1X 是 0x52)
 *   - 测距范围 40~1200mm (短距离模式)
 */

#ifndef BSP_VL53L0X_H
#define BSP_VL53L0X_H

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

/* VL53L0X 默认 I2C 地址 */
#define VL53L0X_DEFAULT_ADDR  0x29

/* ========== 初始化 ========== */

/**
 * @brief 初始化 VL53L0X 传感器
 * @return 0=成功, -1=探活失败, -2=DataInit失败, -3=StaticInit失败,
 *         -4=RefCalibration失败, -5=SPAD管理失败, -6=启动失败
 *
 * 流程: 拉高 XSHUT → 等待启动 → DataInit → StaticInit
 *       → RefCalibration → SPAD管理 → 设置连续测距模式
 */
int bsp_vl53l0x_init(void);

/* ========== 数据读取 ========== */

/**
 * @brief 读取当前距离
 * @return 距离 (mm), 0xFFFF 表示读取失败
 */
uint16_t bsp_vl53l0x_read(void);

/* ========== 状态查询 ========== */

/**
 * @brief 查询传感器是否初始化成功
 */
bool bsp_vl53l0x_is_ready(void);

/**
 * @brief 返回最后一次读取时寄存器 0x13 的值 (用于诊断)
 */
uint8_t bsp_vl53l0x_last_status(void);

#endif /* BSP_VL53L0X_H */
