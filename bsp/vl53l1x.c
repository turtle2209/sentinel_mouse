/**
 * @file    vl53l1x.c
 * @brief   BSP VL53L1X 实现 + 平台 I2C 适配层 (GPIO 位磕)
 *
 *  基于 github.com/wmdscjhdpy/VL53L1X_STM32_module 的 IOI2C 驱动, 已验证可用.
 *  PB10=SCL, PB11=SDA, 推挽输出, 不需要外部上拉.
 */

#include "vl53l1x.h"
#include "i2c.h"
#include "gpio.h"
#include "VL53L1X_api.h"

/* ====================================================================
 *  Part 1: 平台适配层 — HAL I2C
 * ==================================================================== */

#define I2C_TIMEOUT 100

int8_t VL53L1_WriteMulti(uint16_t dev, uint16_t index, uint8_t *pdata, uint32_t count)
{
    if (HAL_I2C_Mem_Write(&hi2c2, (uint16_t)dev, index,
                          I2C_MEMADD_SIZE_16BIT, pdata, count, I2C_TIMEOUT) == HAL_OK)
        return 0;
    return -1;
}

int8_t VL53L1_ReadMulti(uint16_t dev, uint16_t index, uint8_t *pdata, uint32_t count)
{
    if (HAL_I2C_Mem_Read(&hi2c2, (uint16_t)dev, index,
                         I2C_MEMADD_SIZE_16BIT, pdata, count, I2C_TIMEOUT) == HAL_OK)
        return 0;
    return -1;
}

int8_t VL53L1_WrByte(uint16_t dev, uint16_t index, uint8_t data)
{ return VL53L1_WriteMulti(dev, index, &data, 1); }

int8_t VL53L1_WrWord(uint16_t dev, uint16_t index, uint16_t data)
{
    uint8_t b[2]; b[0]=(uint8_t)(data>>8); b[1]=(uint8_t)data;
    return VL53L1_WriteMulti(dev, index, b, 2);
}

int8_t VL53L1_WrDWord(uint16_t dev, uint16_t index, uint32_t data)
{
    uint8_t b[4];
    b[0]=(uint8_t)(data>>24); b[1]=(uint8_t)(data>>16);
    b[2]=(uint8_t)(data>>8);  b[3]=(uint8_t)data;
    return VL53L1_WriteMulti(dev, index, b, 4);
}

int8_t VL53L1_RdByte(uint16_t dev, uint16_t index, uint8_t *pdata)
{ return VL53L1_ReadMulti(dev, index, pdata, 1); }

int8_t VL53L1_RdWord(uint16_t dev, uint16_t index, uint16_t *pdata)
{
    uint8_t b[2]; int8_t r=VL53L1_ReadMulti(dev,index,b,2);
    *pdata=((uint16_t)b[0]<<8)|b[1]; return r;
}

int8_t VL53L1_RdDWord(uint16_t dev, uint16_t index, uint32_t *pdata)
{
    uint8_t b[4]; int8_t r=VL53L1_ReadMulti(dev,index,b,4);
    *pdata=((uint32_t)b[0]<<24)|((uint32_t)b[1]<<16)|((uint32_t)b[2]<<8)|b[3];
    return r;
}

int8_t VL53L1_WaitMs(uint16_t dev, int32_t wait_ms)
{ (void)dev; HAL_Delay((uint32_t)wait_ms); return 0; }


/* ====================================================================
 *  Part 2: XSHUT 控制
 * ==================================================================== */

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} xshut_pin_t;

static const xshut_pin_t xshut_pins[VL53L1X_COUNT] = {
    { F_sensor_GPIO_Port,  F_sensor_Pin  },   /* 0: 正前 */
    { FL_sensor_GPIO_Port, FL_sensor_Pin },   /* 1: 左前 */
    { FR_sensor_GPIO_Port, FR_sensor_Pin },   /* 2: 右前 */
    { L_sensor_GPIO_Port,  L_sensor_Pin  },   /* 3: 正左 */
    { R_sensor_GPIO_Port,  R_sensor_Pin  },   /* 4: 正右 */
};

static const uint8_t i2c_addrs[VL53L1X_COUNT] = {
    0x30, 0x31, 0x32, 0x33, 0x34
};

static bool sensors_ready[VL53L1X_COUNT] = { false };
static int init_error = 0;  /* 0=成功, -1=探活, -2=SensorInit, -3=ID检查 */

static void xshut_set(uint8_t id, bool high)
{
    HAL_GPIO_WritePin(xshut_pins[id].port, xshut_pins[id].pin,
                      high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ====================================================================
 *  Part 3: 初始化
 * ==================================================================== */

/**
 * @brief 初始化单路传感器
 * @param id  传感器 ID
 * @return    true=成功
 *
 * 步骤: XSHUT HIGH → 自动探测地址(0x52/0x29) → SensorInit → SetDistanceMode
 *       → SetTimingBudget → StartRanging
 *
 * 注意: 探活失败直接返回 false, 避免 VL53L1X_SensorInit 内部
 *       CheckForDataReady 死循环 (无传感器时永远等不到数据)
 *
 * 自动地址探测: 先试 0x52 (VL53L1X 标准默认), 再试 0x29 (部分国产模块)
 */
static bool sensor_init_one(uint8_t id, uint8_t addr)
{
    uint8_t state = 0;
    uint16_t id_val = 0;

    /* 候选默认地址: 0x52=标准, 0x29=部分国产模块 */
    static const uint8_t candidate_addrs[] = { 0x52, 0x29 };
    uint8_t default_addr = 0;
    bool found = false;

    /* 拉高 XSHUT, 唤醒传感器 */
    xshut_set(id, true);
    HAL_Delay(50); /* 传感器重新上电需要更长启动时间 */

    /* 自动探测: HAL_I2C_IsDeviceReady (不位移, 和诊断一致) */
    for (int i = 0; i < (int)(sizeof(candidate_addrs)); i++) {
        if (HAL_I2C_IsDeviceReady(&hi2c2, candidate_addrs[i], 2, 50) == HAL_OK) {
            default_addr = candidate_addrs[i];
            found = true;
            break;
        }
    }

    if (!found) { init_error = -1; xshut_set(id, false); return false; }

    /* 加载固件 & 初始化 (Arduino 验证过 SensorInit 能通过) */
    if (VL53L1X_SensorInit(default_addr) != 0) { init_error = -2; xshut_set(id, false); return false; }

    /* 改为唯一 I2C 地址 */
    if (VL53L1X_SetI2CAddress(default_addr, addr) != 0)
        return false;

    /* 验证新地址通信正常 */
    if (VL53L1X_BootState(addr, &state) != 0 || state == 0)
        return false;

    /* 配置: 短距离模式 (1.3m 够用, 抗环境光更强) */
    VL53L1X_SetDistanceMode(addr, 1);   /* 1=short */

    /* 配置: 20ms 时序预算 (每路 50Hz, 五路 250Hz 总带宽) */
    VL53L1X_SetTimingBudgetInMs(addr, 20);

    /* 开始连续测距 */
    if (VL53L1X_StartRanging(addr) != 0)
        return false;

    return true;
}

bool bsp_vl53l1x_init(void)
{
    bool all_ok = true;

    /* Step 0: 所有 XSHUT 拉低, 传感器全部休眠 */
    for (uint8_t i = 0; i < VL53L1X_COUNT; i++) {
        xshut_set(i, false);
        sensors_ready[i] = false;
    }
    HAL_Delay(1);  /* 确保全部进入关机状态 */

    /* Step 1~5: 逐个唤醒 → 初始化 → 改名 → 启动 */
    for (uint8_t i = 0; i < VL53L1X_COUNT; i++) {
        if (sensor_init_one(i, i2c_addrs[i])) {
            sensors_ready[i] = true;
        } else {
            all_ok = false;
            /* 失败的保持 XSHUT LOW, 其他继续初始化 */
            xshut_set(i, false);
        }
    }

    return all_ok;
}

/* ====================================================================
 *  Part 4: 数据读取
 * ==================================================================== */

uint16_t bsp_vl53l1x_read(uint8_t id)
{
    uint8_t ready = 0;
    uint16_t distance = 0;

    if (id >= VL53L1X_COUNT || !sensors_ready[id])
        return 0xFFFF;

    uint8_t addr = i2c_addrs[id];

    /* 检查是否有新数据 */
    if (VL53L1X_CheckForDataReady(addr, &ready) != 0 || ready == 0)
        return 0xFFFF;

    /* 读取距离 */
    if (VL53L1X_GetDistance(addr, &distance) != 0)
        return 0xFFFF;

    /* 清除中断标志, 准备下一次测量 */
    VL53L1X_ClearInterrupt(addr);

    return distance;
}

void bsp_vl53l1x_read_all(uint16_t distances[VL53L1X_COUNT])
{
    for (uint8_t i = 0; i < VL53L1X_COUNT; i++) {
        distances[i] = bsp_vl53l1x_read(i);
    }
}

/* ========== 状态查询 ========== */

bool bsp_vl53l1x_is_ready(uint8_t id)
{
    if (id >= VL53L1X_COUNT) return false;
    return sensors_ready[id];
}

bool bsp_vl53l1x_all_ready(void)
{
    for (uint8_t i = 0; i < VL53L1X_COUNT; i++) {
        if (!sensors_ready[i]) return false;
    }
    return true;
}

int bsp_vl53l1x_init_error(void)
{
    return init_error;
}
