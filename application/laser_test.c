/**
 * @file    laser_test.c
 * @brief   VL53L1X + HAL I2C (100kHz, 外部上拉)
 */

#include "laser_test.h"
#include "main.h"
#include "i2c.h"
#include "vl53l1x.h"

#define THRESHOLD_MM         100
#define BLINK_FAST_MS        100
#define BLINK_SLOW_MS        400
#define BLINK_FAIL_MS        300

static inline void led_on(void)
{ HAL_GPIO_WritePin(LED_board_GPIO_Port, LED_board_Pin, GPIO_PIN_RESET); }
static inline void led_off(void)
{ HAL_GPIO_WritePin(LED_board_GPIO_Port, LED_board_Pin, GPIO_PIN_SET); }
static void led_blink(int n, uint32_t on, uint32_t off)
{
    for (int i = 0; i < n; i++) {
        led_on(); HAL_Delay(on); led_off();
        if (i < n - 1) HAL_Delay(off);
    }
}

void laser_test(void)
{
    led_off(); HAL_Delay(300);
    led_on();  HAL_Delay(100);
    led_off(); HAL_Delay(100);

    /* 强制复位 I2C2 */
    __HAL_RCC_I2C2_FORCE_RESET();
    HAL_Delay(1);
    __HAL_RCC_I2C2_RELEASE_RESET();
    MX_I2C2_Init();

    /* HAL 诊断: 同时测 0x29 和 0x52, 确定正确的地址格式 */
    HAL_GPIO_WritePin(F_sensor_GPIO_Port, F_sensor_Pin, GPIO_PIN_SET);
    HAL_Delay(10);
    HAL_StatusTypeDef s1 = HAL_I2C_IsDeviceReady(&hi2c2, 0x29, 3, 100);
    HAL_StatusTypeDef s2 = HAL_I2C_IsDeviceReady(&hi2c2, 0x52, 3, 100);
    HAL_GPIO_WritePin(F_sensor_GPIO_Port, F_sensor_Pin, GPIO_PIN_RESET);

    /* 0x29 通 → 不移位. 0x52 通 → 需要移位. 都通则默认不移位 */
    int hal_shift = (s1 != HAL_OK && s2 == HAL_OK) ? 1 : 0;

    if (s1 == HAL_OK || s2 == HAL_OK)
        led_blink(5, 150, 200);  /* HAL 找到传感器 */
    else
        led_blink(6, 150, 150);  /* HAL 没找到 */

    HAL_Delay(2000);

    bsp_vl53l1x_init();

    if (!bsp_vl53l1x_is_ready(VL53L1X_FRONT)) {
        int err = -bsp_vl53l1x_init_error();
        if (err == 0) err = 4;
        while (1) { led_blink(err, 400, 300); HAL_Delay(2000); }
    }

    led_blink(3, 150, 150);

    while (1) {
        uint16_t d = bsp_vl53l1x_read(VL53L1X_FRONT);
        if (d == 0xFFFF) { led_blink(2, BLINK_FAIL_MS, BLINK_FAIL_MS); continue; }
        if (d <= THRESHOLD_MM) {
            led_on(); HAL_Delay(BLINK_FAST_MS); led_off(); HAL_Delay(BLINK_FAST_MS);
        } else {
            led_on(); HAL_Delay(BLINK_SLOW_MS); led_off(); HAL_Delay(BLINK_SLOW_MS);
        }
    }
}
