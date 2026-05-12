#ifndef DS18B20_H
#define DS18B20_H

#include "stm32f10x.h"

#define SKIP_ROM            0xCC
#define MATCH_ROM           0x55
#define READ_ROM            0x33
#define SEARCH_ROM          0xF0
#define CONVERT_TEMP        0x44
#define WRITE_SCRATCHPAD    0x4E
#define READ_SCRATCHPAD     0xBE
#define COPY_SCRATCHPAD     0x48
#define RECALL_EEPROM       0xB8
#define READ_POWER_SUPPLY   0xB4

#define RESOLUTION_9BIT     0x1F 
#define RESOLUTION_10BIT    0x3F
#define RESOLUTION_11BIT    0x5F
#define RESOLUTION_12BIT    0x7F

typedef struct {
    uint8_t ROM_code[8];
    uint16_t raw_temp;
    float temp;
    uint8_t scratchpad_data[9];
    uint8_t crc8_rom;
    uint8_t crc8_data;
    uint8_t crc8_rom_error;
    uint8_t crc8_data_error;
	  uint8_t isGenuine;
} Sensor;

extern volatile uint32_t msTicks;

void SysTick_Handler(void);
void DelayM(uint32_t dlyTicks);

void ds18b20_PortInit(void);
uint8_t ds18b20_Reset(void);
uint8_t ds18b20_ReadBit(void);
uint8_t ds18b20_ReadByte(void);
void ds18b20_WriteBit(uint8_t bit);
void ds18b20_WriteByte(uint8_t data);
void ds18b20_MatchRom(uint8_t* address);
void ds18b20_Init(uint8_t mode, uint8_t* address, uint8_t Th, uint8_t Tl, uint8_t resolution);
void ds18b20_ConvertTemp(uint8_t mode, uint8_t* address);
void ds18b20_ReadStratchpad(uint8_t mode, uint8_t *Data, uint8_t* address);
void ds18b20_ReadROM(uint8_t *Data);
uint8_t Compute_CRC8(uint8_t* data, uint8_t length);
uint8_t Search_ROM(char command, Sensor *sensors);

void ds18b20_ReadConfig(uint8_t mode, uint8_t* address, uint8_t* th, uint8_t* tl, uint8_t* config);
void ds18b20_SetConfig(uint8_t mode, uint8_t* address, uint8_t th, uint8_t tl, uint8_t resolution);
void ds18b20_CopyScratchpad(uint8_t mode, uint8_t* address);
void ds18b20_RecallEEPROM(uint8_t mode, uint8_t* address);
uint8_t TestDS18B20Genuine(Sensor *sensor);
#endif // DS18B20_H