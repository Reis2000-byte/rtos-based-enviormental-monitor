#ifndef bme280_H
#define bme280_H

#include <stdbool.h>
#include <stdint.h>
#include "stm32f4xx_hal.h"

#define BME280_ADDR (0x76 << 1)

void bme280_init(I2C_HandleTypeDef *hi2c);
bool bme280_read();
void bme280_compensate();

#endif