/**
 * @file    vl53l1x.h
 * @brief   BSP VL53L1X 五路激光测距接口
 *
 * 硬件:
 *   I2C2 共享总线 (PB10=SCL, PB11=SDA)
 *   五个 XSHUT 独立控制:
 *     PC0 = 正前方 (F)
 *     PC1 = 左前45° (LF)
 *     PC2 = 右前45° (RF)
 *     PC3 = 正左   (L)
 *     PC4 = 正右   (R)
 *
 * I2C 地址分配 (上电初始化后):
 *   0x30 = 正前方, 0x31 = 左前, 0x32 = 右前, 0x33 = 左, 0x34 = 右
 *
 * 使用方式:
 *   bsp_vl53l1x_init();                  // 上电后调用一次
 *   uint16_t d = bsp_vl53l1x_read(VL53L1X_FRONT);  // 单路读取 (mm)
 *   bsp_vl53l1x_read_all(distances);     // 五路批量读取
 */

#ifndef BSP_VL53L1X_H
#define BSP_VL53L1X_H

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

/* ========== 传感器 ID ========== */

#define VL53L1X_FRONT       0   /* 正前方 */
#define VL53L1X_LEFT_FRONT  1   /* 左前 45° */
#define VL53L1X_RIGHT_FRONT 2   /* 右前 45° */
#define VL53L1X_LEFT        3   /* 正左 */
#define VL53L1X_RIGHT       4   /* 正右 */
#define VL53L1X_COUNT       5   /* 传感器总数 */

/* ========== I2C 地址 ========== */

#define VL53L1X_DEFAULT_ADDR  0x29   /* 出厂默认地址 (部分变体用 0x29) */

/* 分配后的地址 */
#define VL53L1X_ADDR_FRONT       0x30
#define VL53L1X_ADDR_LEFT_FRONT  0x31
#define VL53L1X_ADDR_RIGHT_FRONT 0x32
#define VL53L1X_ADDR_LEFT        0x33
#define VL53L1X_ADDR_RIGHT       0x34

/* ========== 初始化 ========== */

/**
 * @brief 初始化全部五路 VL53L1X
 * @return true=全部成功, false=某一路初始化失败
 *
 * 流程: 逐个拉高 XSHUT → 改 I2C 地址 → SensorInit → StartRanging
 *       初始化完成后五路同时在线, 各自以唯一地址应答
 */
bool bsp_vl53l1x_init(void);

/* ========== 单路读取 ========== */

/**
 * @brief 读取指定传感器的距离
 * @param id  传感器 ID (VL53L1X_FRONT ~ VL53L1X_RIGHT)
 * @return    距离 (mm), 0xFFFF 表示读取失败/数据未就绪
 *
 * 内部自动轮询 CheckForDataReady, 非阻塞
 */
uint16_t bsp_vl53l1x_read(uint8_t id);

/* ========== 批量读取 ========== */

/**
 * @brief 一次性读取全部五路传感器
 * @param distances  输出数组 [5], 存放五路距离 (mm)
 *                   顺序: [前, 左前, 右前, 左, 右]
 *                   某路读取失败填 0xFFFF
 */
void bsp_vl53l1x_read_all(uint16_t distances[VL53L1X_COUNT]);

/* ========== 状态查询 ========== */

/**
 * @brief 查询指定传感器是否已初始化成功
 * @param id  传感器 ID (VL53L1X_FRONT ~ VL53L1X_RIGHT)
 * @return    true=该路在线
 */
bool bsp_vl53l1x_is_ready(uint8_t id);

/**
 * @brief 查询全部传感器是否都已初始化成功
 * @return true=全部在线
 */
bool bsp_vl53l1x_all_ready(void);

/** 返回初始化错误码: 0=成功, -1=探活, -2=SensorInit, -3=ID */
int bsp_vl53l1x_init_error(void);

#endif /* BSP_VL53L1X_H */
