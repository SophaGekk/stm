#ifndef LM75A_H
#define LM75A_H

#include "stm32f10x.h"

#define LM75B_Conf       0x01
#define LM75B_Temp       0x00
#define LM75B_Tos        0x03
#define LM75B_Thyst      0x02
#define LM75A_ADDR_START 0x48
#define LM75A_ADDR_END   0x4F

#define I2C_OWNADDRESS1_7BIT 0x00004000U
#define I2C_MODE_I2C         0x00000000U
#define SLAVE_OWN_ADDRESS 0x90
#define I2C_REQUEST_WRITE    0x00
#define I2C_REQUEST_READ     0x01
#define LM75B_Tos_WRITE   0x03
#define LM75B_Thyst_WRITE 0x02

typedef struct {
    uint8_t address;
    float temp;
    uint16_t raw_temp;
    uint8_t present;
} LM75A_Sensor;

extern volatile uint32_t error_counter;

void I2C_Init(void);
void I2C_Reset(void);
uint8_t I2C_WriteData(uint8_t addr, uint8_t *buf, uint16_t bytes_count);
uint8_t I2C_ReadData(uint8_t addr, uint8_t *buf, uint16_t bytes_count);
uint8_t I2C_IsDeviceReady(uint8_t devAddr);
uint8_t LM75A_CheckAnyDevice(void);
uint8_t LM75A_ReadTemperature(uint8_t addr, float *temp);
uint8_t LM75A_IsConnected(uint8_t addr);

#endif // LM75A_H