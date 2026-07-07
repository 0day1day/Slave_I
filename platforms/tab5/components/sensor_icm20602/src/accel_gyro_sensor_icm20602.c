/**********************************************************************
 3-axis accel & 3-axis gyro sensor icm20602 driver

 @date: 2024/01/01

**********************************************************************/
#include "accel_gyro_sensor_icm20602.h"
#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define I2C_MASTER_TIMEOUT_MS 50  // 超时时间

#define I2C_DEV_ADDR_ICM20602 0x68
#define ICM_REG_ACCEL_XOUT_H  0x3B
#define ICM_REG_GYRO_XOUT_H   0x43

static i2c_master_dev_handle_t i2c_dev_handle_icm20602;

/**
 * 初始化
 */
esp_err_t accel_gyro_sensor_icm20602_init(i2c_master_bus_handle_t bus_handle)
{
    uint8_t write_buf[2] = {0};

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = I2C_DEV_ADDR_ICM20602,
        .scl_speed_hz    = 400000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &i2c_dev_handle_icm20602));

    write_buf[0] = 0x6B;
    write_buf[1] = 0x00;
    i2c_master_transmit(i2c_dev_handle_icm20602, write_buf, 2, I2C_MASTER_TIMEOUT_MS);  // sleep=0
    write_buf[0] = 0x6A;
    write_buf[1] = 0x00;
    i2c_master_transmit(i2c_dev_handle_icm20602, write_buf, 2, I2C_MASTER_TIMEOUT_MS);  // mst_en=0
    write_buf[0] = 0x38;
    write_buf[1] = 0x00;
    i2c_master_transmit(i2c_dev_handle_icm20602, write_buf, 2, I2C_MASTER_TIMEOUT_MS);  // interrupt off
    write_buf[0] = 0x19;
    write_buf[1] = 0x07;
    i2c_master_transmit(i2c_dev_handle_icm20602, write_buf, 2, I2C_MASTER_TIMEOUT_MS);  // 1KHz
    write_buf[0] = 0x1A;
    write_buf[1] = 0x03;
    i2c_master_transmit(i2c_dev_handle_icm20602, write_buf, 2, I2C_MASTER_TIMEOUT_MS);  // 44Hz filter
    write_buf[0] = 0x1B;
    write_buf[1] = 0x10;
    i2c_master_transmit(i2c_dev_handle_icm20602, write_buf, 2,
                        I2C_MASTER_TIMEOUT_MS);  // gyro +-1000dps --> 32.8 LSB/°/S
    write_buf[0] = 0x1C;
    write_buf[1] = 0x08;
    i2c_master_transmit(i2c_dev_handle_icm20602, write_buf, 2, I2C_MASTER_TIMEOUT_MS);  // accel +-4g --> 8192 LSB/g
    write_buf[0] = 0x24;
    write_buf[1] = 0x13;
    i2c_master_transmit(i2c_dev_handle_icm20602, write_buf, 2, I2C_MASTER_TIMEOUT_MS);  // 400khz

    return ESP_OK;
}

/**
 * 读取加速度(unit: m/s^2)
 */
esp_err_t accel_gyro_sensor_icm20602_read_accel(float *x, float *y, float *z)
{
    uint8_t write_buf[1] = {ICM_REG_ACCEL_XOUT_H};
    uint8_t read_buf[6];
    int accel_x, accel_y, accel_z;

    /* 读取数据 */
    // i2c_master_write_read_device(_i2c_port, I2C_ADDR_ICM20602, write_buf, 1, read_buf, 6,
    // pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    i2c_master_transmit_receive(i2c_dev_handle_icm20602, write_buf, 1, read_buf, 6, I2C_MASTER_TIMEOUT_MS);
    accel_x = (int16_t)((read_buf[0] << 8) | read_buf[1]);
    accel_y = (int16_t)((read_buf[2] << 8) | read_buf[3]);
    accel_z = (int16_t)((read_buf[4] << 8) | read_buf[5]);

    /* 根据设置量程转换单位 */
    *x = accel_x / 8192.0 * 9.8;
    *y = accel_y / 8192.0 * 9.8;
    *z = accel_z / 8192.0 * 9.8;

    return ESP_OK;
}

/**
 * 读取角速度(unit: °/s)
 */
esp_err_t accel_gyro_sensor_icm20602_read_gyro(float *x, float *y, float *z)
{
    uint8_t write_buf[1] = {ICM_REG_GYRO_XOUT_H};
    uint8_t read_buf[6];
    int gyro_x, gyro_y, gyro_z;

    /* 读取数据 */
    // i2c_master_write_read_device(_i2c_port, I2C_ADDR_ICM20602, write_buf, 1, read_buf, 6,
    // pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    i2c_master_transmit_receive(i2c_dev_handle_icm20602, write_buf, 1, read_buf, 6, I2C_MASTER_TIMEOUT_MS);
    gyro_x = (int16_t)((read_buf[0] << 8) | read_buf[1]);
    gyro_y = (int16_t)((read_buf[2] << 8) | read_buf[3]);
    gyro_z = (int16_t)((read_buf[4] << 8) | read_buf[5]);

    /* 根据设置量程转换单位 */
    *x = gyro_x / 32.8;
    *y = gyro_y / 32.8;
    *z = gyro_z / 32.8;

    return ESP_OK;
}

// void MahonyAHRSupdateIMU(float gx, float gy, float gz, float ax, float ay, float az)
// {
//     float recipNorm;
//     float halfvx, halfvy, halfvz;
//     float halfex, halfey, halfez;
//     float qa, qb, qc;

//     // Compute feedback only if accelerometer measurement valid (avoids NaN in accelerometer normalisation)
//     if(!((ax == 0.0f) && (ay == 0.0f) && (az == 0.0f))) {

//         // Normalise accelerometer measurement
//         recipNorm = invSqrt(ax * ax + ay * ay + az * az);
//         ax *= recipNorm;
//         ay *= recipNorm;
//         az *= recipNorm;

//         // Estimated direction of gravity and vector perpendicular to magnetic flux
//         halfvx = q1 * q3 - q0 * q2;
//         halfvy = q0 * q1 + q2 * q3;
//         halfvz = q0 * q0 - 0.5f + q3 * q3;

//         // Error is sum of cross product between estimated and measured direction of gravity
//         halfex = (ay * halfvz - az * halfvy);
//         halfey = (az * halfvx - ax * halfvz);
//         halfez = (ax * halfvy - ay * halfvx);

//         // Compute and apply integral feedback if enabled
//         if(twoKi > 0.0f) {
//             integralFBx += twoKi * halfex * (1.0f / sampleFreq);    // integral error scaled by Ki
//             integralFBy += twoKi * halfey * (1.0f / sampleFreq);
//             integralFBz += twoKi * halfez * (1.0f / sampleFreq);
//             gx += integralFBx;  // apply integral feedback
//             gy += integralFBy;
//             gz += integralFBz;
//         }
//         else {
//             integralFBx = 0.0f; // prevent integral windup
//             integralFBy = 0.0f;
//             integralFBz = 0.0f;
//         }

//         // Apply proportional feedback
//         gx += twoKp * halfex;
//         gy += twoKp * halfey;
//         gz += twoKp * halfez;
//     }

//     // Integrate rate of change of quaternion
//     gx *= (0.5f * (1.0f / sampleFreq));     // pre-multiply common factors
//     gy *= (0.5f * (1.0f / sampleFreq));
//     gz *= (0.5f * (1.0f / sampleFreq));
//     qa = q0;
//     qb = q1;
//     qc = q2;
//     q0 += (-qb * gx - qc * gy - q3 * gz);
//     q1 += (qa * gx + qc * gz - q3 * gy);
//     q2 += (qa * gy - qb * gz + q3 * gx);
//     q3 += (qa * gz + qb * gy - qc * gx);

//     // Normalise quaternion
//     recipNorm = invSqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
//     q0 *= recipNorm;
//     q1 *= recipNorm;
//     q2 *= recipNorm;
//     q3 *= recipNorm;

//     roll = atan2f(q0*q1 + q2*q3, 0.5f - q1*q1 - q2*q2) * 57.3f;
//     pitch = asinf(-2.0f * (q1*q3 - q0*q2)) * 57.3f;
//     yaw = atan2f(q1*q2 + q0*q3, 0.5f - q2*q2 - q3*q3) * 57.3f;

//     tst_q0 = q0 * 100;
//     tst_q1 = q1 * 100;
//     tst_q2 = q2 * 100;
//     tst_q3 = q3 * 100;

//     tst_pitch = pitch * 100;
//     tst_roll = roll*100;
//     tst_yaw = yaw*100;
// }
