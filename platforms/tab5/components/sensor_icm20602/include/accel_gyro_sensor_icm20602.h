#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "driver/i2c_master.h"

/**
 * 初始化
 */
esp_err_t accel_gyro_sensor_icm20602_init(i2c_master_bus_handle_t bus_handle);

/**
 * 读取加速度(unit: m/s^2)
 */
esp_err_t accel_gyro_sensor_icm20602_read_accel(float *x, float *y, float *z);

/**
 * 读取角速度(unit: °/s)
 */
esp_err_t accel_gyro_sensor_icm20602_read_gyro(float *x, float *y, float *z);

#ifdef __cplusplus
}
#endif
