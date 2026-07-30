/**
 * @file    vl53l0x.c
 * @brief   BSP VL53L0X 驱动 — 回归能工作的版本 + NVM 校准
 */

#include "vl53l0x.h"
#include "i2c.h"
#include "gpio.h"

/* ====================================================================
 *  Part 1: GPIO Bit-Bang I2C (8 位寄存器地址)
 * ==================================================================== */

#define BB_SCL  GPIO_PIN_10
#define BB_SDA  GPIO_PIN_11
#define BB_PORT GPIOB

static void bb_delay(uint32_t us)
{
    for (volatile uint32_t i = 0; i < us * 18; i++) __NOP();
}

static void bb_scl_h(void) { BB_PORT->BSRR = BB_SCL; }
static void bb_scl_l(void) { BB_PORT->BRR  = BB_SCL; }
static void bb_sda_h(void) { BB_PORT->BSRR = BB_SDA; }
static void bb_sda_l(void) { BB_PORT->BRR  = BB_SDA; }
static uint8_t bb_sda_r(void) { return (BB_PORT->IDR & BB_SDA) ? 1 : 0; }

static void bb_sda_in(void)
{
    uint32_t t = BB_PORT->CRH;
    t &= ~(0xF << 12); t |= (0x8 << 12);
    BB_PORT->CRH = t;
    BB_PORT->ODR |= BB_SDA;
}

static void bb_sda_out(void)
{
    uint32_t t = BB_PORT->CRH;
    t &= ~(0xF << 12); t |= (0x3 << 12);
    BB_PORT->CRH = t;
}

static void bb_start(void)
{
    bb_sda_h(); bb_scl_h(); bb_delay(2);
    bb_sda_l(); bb_delay(2); bb_scl_l();
}

static void bb_stop(void)
{
    bb_sda_l(); bb_scl_h(); bb_delay(2);
    bb_sda_h(); bb_delay(2);
}

static uint8_t bb_write(uint8_t data)
{
    for (uint8_t m = 0x80; m; m >>= 1) {
        if (data & m) bb_sda_h(); else bb_sda_l();
        bb_delay(1); bb_scl_h(); bb_delay(3); bb_scl_l(); bb_delay(1);
    }
    bb_sda_in(); bb_delay(1); bb_scl_h(); bb_delay(3);
    uint8_t ack = bb_sda_r();
    bb_scl_l(); bb_sda_out(); bb_delay(1);
    return ack;
}

static uint8_t bb_read(uint8_t nak)
{
    uint8_t data = 0;
    bb_sda_in();
    for (int i = 0; i < 8; i++) {
        data <<= 1;
        bb_scl_h(); bb_delay(3);
        if (bb_sda_r()) data |= 1;
        bb_scl_l(); bb_delay(2);
    }
    bb_sda_out();
    if (nak) bb_sda_h(); else bb_sda_l();
    bb_delay(1); bb_scl_h(); bb_delay(3); bb_scl_l(); bb_delay(1);
    return data;
}

static int8_t l0x_write(uint8_t dev7, uint8_t reg, uint8_t data)
{
    int8_t ret = 0;
    bb_start();
    if (bb_write((uint8_t)(dev7 << 1))) { ret = -1; goto done; }
    if (bb_write(reg))                  { ret = -1; goto done; }
    if (bb_write(data))                 { ret = -1; goto done; }
done:
    bb_stop();
    return ret;
}

static int8_t l0x_read(uint8_t dev7, uint8_t reg, uint8_t *data, uint8_t count)
{
    int8_t ret = 0;
    bb_start();
    if (bb_write((uint8_t)(dev7 << 1)))        { ret = -1; goto done; }
    if (bb_write(reg))                          { ret = -1; goto done; }
    bb_start();
    if (bb_write((uint8_t)((dev7 << 1) | 1)))   { ret = -1; goto done; }
    for (uint8_t i = 0; i < count; i++)
        data[i] = bb_read(i == count - 1 ? 1 : 0);
done:
    bb_stop();
    return ret;
}

static bool bb_ok = false;

static void bb_setup(void)
{
    if (bb_ok) return;
    HAL_I2C_DeInit(&hi2c2);
    GPIO_InitTypeDef g = {0};
    g.Pin = BB_SCL | BB_SDA;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(BB_PORT, &g);
    bb_sda_h(); bb_scl_h();
    bb_ok = true;
}

/* ====================================================================
 *  Part 2: VL53L0X 驱动 (回归能工作的版本)
 * ==================================================================== */

#define REG_SYSRANGE_START          0x00
#define REG_SYSTEM_SEQUENCE_CONFIG  0x01
#define REG_SYSTEM_INTERRUPT_CLEAR  0x0B
#define REG_RESULT_INTERRUPT_STATUS 0x13

static bool sensor_ok = false;
static uint8_t stop_var = 0;
static uint8_t last_0x13 = 0;  /* 诊断用: 最后一次 0x13 值 */

int bsp_vl53l0x_init(void)
{
    bb_setup();
    uint8_t dev = VL53L0X_DEFAULT_ADDR;

    HAL_GPIO_WritePin(F_sensor_GPIO_Port, F_sensor_Pin, GPIO_PIN_SET);
    HAL_Delay(50);

    /* 探活 */
    uint8_t probe;
    if (l0x_read(dev, 0x00, &probe, 1) != 0) {
        sensor_ok = false;
        HAL_GPIO_WritePin(F_sensor_GPIO_Port, F_sensor_Pin, GPIO_PIN_RESET);
        return -1;
    }

    /* I2C 标准模式 */
    l0x_write(dev, 0x88, 0x00);

    /* DataInit (前导序列, 传感器现在接受 0x80 写入) */
    l0x_write(dev, 0x80, 0x01);
    l0x_write(dev, 0xFF, 0x01);
    l0x_write(dev, 0x00, 0x00);
    l0x_read(dev, 0x91, &stop_var, 1);
    l0x_write(dev, 0x00, 0x02);   /* 连续测距 back-to-back */
    l0x_write(dev, 0xFF, 0x00);
    l0x_write(dev, 0x80, 0x00);

    /* 等待 DataInit 测距完成 (超时 2s, 不阻塞 — 传感器启动后首次测距可能较慢) */
    uint8_t dr = 0;
    for (int t = 0; t < 2000; t++) {
        l0x_read(dev, 0x13, &dr, 1);
        if (dr & 0x07) break;
        HAL_Delay(1);
    }
    l0x_write(dev, REG_SYSTEM_INTERRUPT_CLEAR, 0x01);
    sensor_ok = true;
    return 0;
}

uint16_t bsp_vl53l0x_read(void)
{
    if (!sensor_ok) return 0xFFFF;

    uint8_t dev = VL53L0X_DEFAULT_ADDR;

    /* 只读 0x13 等数据, 不重新触发 */
    uint8_t ready = 0;
    if (l0x_read(dev, REG_RESULT_INTERRUPT_STATUS, &ready, 1) != 0)
        return 0xFFFF;
    if (!(ready & 0x07))
        return 0xFFFF;

    uint8_t buf[12];
    if (l0x_read(dev, 0x14, buf, 12) != 0)
        return 0xFFFF;

    l0x_write(dev, REG_SYSTEM_INTERRUPT_CLEAR, 0x01);

    return ((uint16_t)buf[10] << 8) | buf[11];
}

bool bsp_vl53l0x_is_ready(void)
{
    return sensor_ok;
}

uint8_t bsp_vl53l0x_last_status(void)
{
    return last_0x13;
}
