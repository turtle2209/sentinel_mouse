/**
 * @file    laser_test.h
 * @brief   单路 VL53L1X 激光模块测试
 *
 * 测试硬件: 1× VL53L1X 连接至 PC0 (正前方), 板载 LED PC7
 *
 * LED 反馈逻辑:
 *   - 启动确认 → 灭 300ms → 闪 1 下
 *   - 软件 I2C 诊断 (GPIO 推挽, 不依赖上拉):
 *       3 闪 = 传感器在线 (HAL I2C 缺上拉)
 *       4 闪 = 传感器离线 (接线/供电问题)
 *   - 读到激光数据 → 快闪 3 下 → 灭 1 秒
 *   - 距离 ≤ 10cm → 快闪 (周期 ~200ms)
 *   - 距离 > 10cm → 慢闪 (周期 ~800ms)
 *   - 读不到数据   → 闪 2 下 (周期 ~600ms), 持续重试
 *
 * 使用方式: 在 main.c 中调用 laser_test() 即可
 */

#ifndef LASER_TEST_H
#define LASER_TEST_H

void laser_test(void);

#endif /* LASER_TEST_H */
