#include "bme280.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_def.h"
#include "stm32f4xx_hal_i2c.h"
#include <stdint.h>

void bme280_init(I2C_HandleTypeDef *hi2c)
{
    /* Job 1 - Verify chip identity
     * Register 0xD0 (chip_id) is found in the BME280 datasheet register map (Section 5.2).
     * It always returns 0x60 on a genuine BME280.
     * uint8_t because the register is exactly 8 bits wide.
     * Size = 1 because we only need that single byte.
     * I2C_MEMADD_SIZE_8BIT because BME280 register addresses are 8 bits wide.
     * Blocking read (no _IT/_DMA) because nothing can proceed until we confirm the device is present. */
    uint8_t chip_id;
    HAL_I2C_Mem_Read(hi2c, BME280_ADDR, 0xD0, I2C_MEMADD_SIZE_8BIT, &chip_id, 1, HAL_MAX_DELAY);
    if (chip_id != 0x60) return;

    /* Job 2 - Reset the device
     * Register 0xE0 (reset) is found in the BME280 datasheet register map (Section 5.2).
     * Writing 0xB6 is the only value that triggers a reset - any other value is ignored by the chip.
     * This clears all registers to factory defaults, ensuring a clean state regardless of
     * what the sensor was doing before (e.g. after an MCU crash or repeated re-flashing).
     * HAL_Delay(10) gives the chip time to finish its internal restart - datasheet specifies
     * ~2ms startup time, 10ms gives comfortable margin before we try to configure it. */
    uint8_t reset_cmd = 0xB6;
    HAL_I2C_Mem_Write(hi2c, BME280_ADDR, 0xE0, I2C_MEMADD_SIZE_8BIT, &reset_cmd, 1, HAL_MAX_DELAY);
    HAL_Delay(10);

    /* Job 3 - Configure oversampling and operating mode
     * Oversampling controls how many raw ADC readings the chip averages before returning a result.
     * More samples = less noise but slower measurement and higher power draw.
     * x1 is sufficient for an environmental monitor since temperature, humidity, and pressure
     * change slowly — no benefit to higher oversampling until noise in the output is observed.
     *
     * ctrl_hum (0xF2) bits [2:0] set humidity oversampling (001 = x1).
     * ctrl_meas (0xF4) bits [7:5] = temperature oversampling (001 = x1),
     *                  bits [4:2] = pressure oversampling    (001 = x1),
     *                  bits [1:0] = mode                     (11  = normal, continuous).
     * 0b00100111 → temp x1, pressure x1, normal mode.
     * ctrl_hum MUST be written before ctrl_meas — the humidity setting only takes
     * effect once ctrl_meas is written (BME280 datasheet §5.4.3). */
    uint8_t hum_os = 0x01;
    HAL_I2C_Mem_Write(hi2c, BME280_ADDR, 0xF2, I2C_MEMADD_SIZE_8BIT, &hum_os, 1, HAL_MAX_DELAY);

    uint8_t ctrl = 0b00100111;
    HAL_I2C_Mem_Write(hi2c, BME280_ADDR, 0xF4, I2C_MEMADD_SIZE_8BIT, &ctrl, 1, HAL_MAX_DELAY);
}
bool bme280_read();
void bme280_compensate();