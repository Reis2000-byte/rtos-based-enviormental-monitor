#ifndef bme280_H
#define bme280_H

#include <stdbool.h>
#include <stdint.h>
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_dma.h"

#define BME280_ADDR (0x76 << 1)

/** @brief Raw ADC counts from a single burst read of registers 0xF7-0xFE.
 *         Values are uncompensated — pass to bme280_compensate to get physical units. */
typedef struct sensor_readings
{
    uint32_t press;  /* 20-bit raw pressure ADC count */
    int32_t  temp;   /* 20-bit raw temperature ADC count */
    uint32_t hum;    /* 16-bit raw humidity ADC count */
}bme_raw;

/** @brief Compensated sensor output in physical units.
 *         Populated by bme280_compensate; ready to display or transmit. */
typedef struct configured_data
{
    uint32_t press;  /* Pressure in Pa (Q24.8 — divide by 256 for Pa) */
    int32_t  temp;   /* Temperature in 0.01 degC (e.g. 2345 = 23.45 degC) */
    uint32_t hum;    /* Relative humidity in Q22.10 (divide by 1024 for %RH) */
}bme_data;

/** @brief Factory calibration coefficients burned into the BME280's NVM.
 *         Read once during bme280_init; used by bme280_compensate to correct
 *         raw ADC counts for each chip's unique offset and gain. */
typedef struct calibration_params
{
    uint16_t dig_T1;  /* Temperature: unsigned */
    int16_t  dig_T2;  /* Temperature: signed */
    int16_t  dig_T3;  /* Temperature: signed */
    uint16_t dig_P1;  /* Pressure: unsigned */
    int16_t  dig_P2;  /* Pressure: signed */
    int16_t  dig_P3;  /* Pressure: signed */
    int16_t  dig_P4;  /* Pressure: signed */
    int16_t  dig_P5;  /* Pressure: signed */
    int16_t  dig_P6;  /* Pressure: signed */
    int16_t  dig_P7;  /* Pressure: signed */
    int16_t  dig_P8;  /* Pressure: signed */
    int16_t  dig_P9;  /* Pressure: signed */
    uint8_t  dig_H1;  /* Humidity: unsigned byte at 0xA1 */
    int16_t  dig_H2;  /* Humidity: signed */
    uint8_t  dig_H3;  /* Humidity: unsigned */
    int16_t  dig_H4;  /* Humidity: signed, packed across 0xE4/0xE5 */
    int16_t  dig_H5;  /* Humidity: signed, packed across 0xE5/0xE6 */
    int8_t   dig_H6;  /* Humidity: signed byte at 0xE7 */
}bme_calibration_params;

/**
 * @brief  Initialise the BME280: verify chip ID, soft reset, configure oversampling,
 *         and read all factory calibration coefficients from NVM into cal_param.
 * @param  hi2c       I2C peripheral handle
 * @param  cal_param  Calibration struct to populate; only valid if true is returned
 * @retval true on success, false if chip absent or returns wrong ID
 */
bool bme280_init(I2C_HandleTypeDef *hi2c, bme_calibration_params *cal_param);

/**
 * @brief  Burst-read all 8 raw ADC registers (0xF7-0xFE) in one I2C transaction
 *         and assemble them into raw press/temp/hum counts.
 * @param  hi2c  I2C peripheral handle
 * @param  raw   Output struct for raw ADC counts
 * @retval true on success, false on I2C error
 */
bool bme280_read_sensors(I2C_HandleTypeDef *hi2c, bme_raw *raw);

/**
 * @brief  Convert raw ADC counts to compensated physical values using
 *         Bosch's integer compensation formulas (datasheet §4.2.3).
 *         Must be called after bme280_init and bme280_read_sensors.
 * @param  cal_param  Calibration coefficients populated by bme280_init
 * @param  raw        Raw ADC counts from bme280_read_sensors
 * @param  data       Output: temperature in 0.01 degC, pressure in Pa, humidity in Q22.10
 */
void bme280_compensate(bme_calibration_params *cal_param, bme_raw *raw, bme_data *data);

#endif