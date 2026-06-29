#include "bme280.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_def.h"
#include "stm32f4xx_hal_i2c.h"
#include <stdbool.h>
#include <stdint.h>

static int32_t t_fine;

bool bme280_init(I2C_HandleTypeDef *hi2c, bme_calibration_params_t *cal_param)
{
    uint8_t chip_id;
    HAL_I2C_Mem_Read(hi2c, BME280_ADDR, 0xD0, I2C_MEMADD_SIZE_8BIT, &chip_id, 1, HAL_MAX_DELAY);
    if (chip_id != 0x60) return false;

    uint8_t reset_cmd = 0xB6;
    HAL_I2C_Mem_Write(hi2c, BME280_ADDR, 0xE0, I2C_MEMADD_SIZE_8BIT, &reset_cmd, 1, HAL_MAX_DELAY);
    HAL_Delay(10);

    uint8_t hum_os = 0x01;
    HAL_I2C_Mem_Write(hi2c, BME280_ADDR, 0xF2, I2C_MEMADD_SIZE_8BIT, &hum_os, 1, HAL_MAX_DELAY);
    uint8_t ctrl = 0b00100111;
    HAL_I2C_Mem_Write(hi2c, BME280_ADDR, 0xF4, I2C_MEMADD_SIZE_8BIT, &ctrl, 1, HAL_MAX_DELAY);

    uint8_t cal_buffer[24];
    HAL_I2C_Mem_Read(hi2c, BME280_ADDR, 0x88, I2C_MEMADD_SIZE_8BIT, cal_buffer, 24, HAL_MAX_DELAY);

    cal_param->dig_T1 = (uint16_t)((uint16_t)cal_buffer[1] << 8 | cal_buffer[0]);
    cal_param->dig_T2 = (int16_t)((uint16_t)cal_buffer[3] << 8 | cal_buffer[2]);
    cal_param->dig_T3 = (int16_t)((uint16_t)cal_buffer[5] << 8 | cal_buffer[4]);

    cal_param->dig_P1 = (uint16_t)((uint16_t)cal_buffer[7] << 8 | cal_buffer[6]);
    cal_param->dig_P2 = (int16_t)((uint16_t)cal_buffer[9] << 8 | cal_buffer[8]);
    cal_param->dig_P3 = (int16_t)((uint16_t)cal_buffer[11] << 8 | cal_buffer[10]);
    cal_param->dig_P4 = (int16_t)((uint16_t)cal_buffer[13] << 8 | cal_buffer[12]);
    cal_param->dig_P5 = (int16_t)((uint16_t)cal_buffer[15] << 8 | cal_buffer[14]);
    cal_param->dig_P6 = (int16_t)((uint16_t)cal_buffer[17] << 8 | cal_buffer[16]);
    cal_param->dig_P7 = (int16_t)((uint16_t)cal_buffer[19] << 8 | cal_buffer[18]);
    cal_param->dig_P8 = (int16_t)((uint16_t)cal_buffer[21] << 8 | cal_buffer[20]);
    cal_param->dig_P9 = (int16_t)((uint16_t)cal_buffer[23] << 8 | cal_buffer[22]);

    HAL_I2C_Mem_Read(hi2c, BME280_ADDR, 0xA1, I2C_MEMADD_SIZE_8BIT, &cal_param->dig_H1, 1, HAL_MAX_DELAY);

    HAL_I2C_Mem_Read(hi2c, BME280_ADDR, 0xE1, I2C_MEMADD_SIZE_8BIT, cal_buffer, 7, HAL_MAX_DELAY);

    cal_param->dig_H2 = (int16_t)((uint16_t)cal_buffer[1] << 8 | cal_buffer[0]);
    cal_param->dig_H3 = cal_buffer[2];
    cal_param->dig_H4 = (int16_t)((uint16_t)cal_buffer[3] << 4 | (cal_buffer[4] & 0x0F));
    cal_param->dig_H5 = (int16_t)((uint16_t)cal_buffer[5] << 4 | (cal_buffer[4] >> 4));
    cal_param->dig_H6 = (int8_t)cal_buffer[6];

    return true;
}

bool bme280_read_sensors(I2C_HandleTypeDef *hi2c, bme_raw_t *data)
{
    uint8_t buf[8];
    if(HAL_I2C_Mem_Read(hi2c, BME280_ADDR, 0xF7, I2C_MEMADD_SIZE_8BIT, buf, 8, HAL_MAX_DELAY) != HAL_OK)
    {
        return false;
    }
    data->press = (uint32_t)buf[0] << 12 | (uint32_t)buf[1] << 4 | buf[2] >> 4;
    data->temp = (uint32_t)buf[3] << 12 | (uint32_t)buf[4] << 4 | buf[5] >> 4;
    data->hum = (uint32_t)buf[6] << 8 | (uint32_t)buf[7];
    return true;
}

/* Bosch temperature compensation (datasheet §4.2.3).
 * Sets the file-level t_fine used by pressure and humidity compensation.
 * Returns temperature in units of 0.01°C (e.g. 2345 = 23.45°C). */
static int32_t compensate_temp(bme_calibration_params_t *cal, bme_raw_t *raw)
{
    int32_t var1, var2;
    var1  = ((((raw->temp>>3) - ((int32_t)cal->dig_T1<<1))) * ((int32_t)cal->dig_T2)) >> 11;
    var2  = (((((raw->temp>>4) - ((int32_t)cal->dig_T1)) * ((raw->temp>>4) - ((int32_t)cal->dig_T1))) >> 12) *    ((int32_t)cal->dig_T3)) >> 14;
    t_fine = var1 + var2;
    return (t_fine * 5 + 128) >> 8;
}

/* Bosch pressure compensation (datasheet §4.2.3).
 * Requires t_fine to be set by compensate_temp first.
 * Returns pressure in Pa as a Q24.8 fixed-point value (divide by 256 for Pa). */
static uint32_t compensate_press(bme_calibration_params_t *cal, bme_raw_t *raw)
{
    int64_t var1, var2, p; 
    var1 = ((int64_t)t_fine) - 128000;  
    var2 = var1 * var1 * (int64_t)cal->dig_P6;  
    var2 = var2 + ((var1*(int64_t)cal->dig_P5)<<17);  
    var2 = var2 + (((int64_t)cal->dig_P4)<<35);  
    var1 = ((var1 * var1 * (int64_t)cal->dig_P3)>>8) + ((var1 * (int64_t)cal->dig_P2)<<12);  
    var1 = (((((int64_t)1)<<47)+var1))*((int64_t)cal->dig_P1)>>33;  
    if (var1 == 0)  {   
        return  0; // avoid exception caused by division by zero  
    }
    
    p = 1048576-raw->press;  
    p = (((p<<31)-var2)*3125)/var1;  
    var1 = (((int64_t)cal->dig_P9) * (p>>13) * (p>>13)) >> 25;  
    var2 = (((int64_t)cal->dig_P8) * p) >> 19;  
    p = ((p + var1 + var2) >> 8) + (((int64_t)cal->dig_P7)<<4);  
    return (uint32_t)p;
}

/* Bosch humidity compensation (datasheet §4.2.3).
 * Requires t_fine to be set by compensate_temp first.
 * Returns relative humidity in Q22.10 fixed-point (divide by 1024 for %RH). */
static uint32_t compensate_hum(bme_calibration_params_t *cal, bme_raw_t *raw)
{
    int32_t v_x1_u32r;    
    v_x1_u32r = (t_fine - ((int32_t)76800)); 
    v_x1_u32r = ((((((int32_t)raw->hum << 14) - (((int32_t)cal->dig_H4) << 20) - (((int32_t)cal->dig_H5) * 
        v_x1_u32r)) + ((int32_t)16384)) >> 15) * (((((((v_x1_u32r * 
        ((int32_t)cal->dig_H6)) >> 10) * (((v_x1_u32r * ((int32_t)cal->dig_H3)) >> 11) + 
        ((int32_t)32768))) >> 10) + ((int32_t)2097152)) * ((int32_t)cal->dig_H2) + 
        8192) >> 14));
    v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)cal->dig_H1)) >> 4));  
    v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);   
    v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);   
    return (uint32_t)(v_x1_u32r>>12); 
}

void bme280_compensate(bme_calibration_params_t *cal_param, bme_raw_t *raw, bme_data_t *data)
{
    data->temp =  compensate_temp(cal_param,raw);
    data->press = compensate_press(cal_param,raw);
    data->hum = compensate_hum(cal_param, raw);
}