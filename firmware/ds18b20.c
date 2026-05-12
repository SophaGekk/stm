#include "ds18b20.h"

volatile uint32_t msTicks;

void ds18b20_PortInit(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    GPIOB->CRH |= GPIO_CRH_MODE11;
    GPIOB->CRH |= GPIO_CRH_CNF11_0;
    GPIOB->CRH &= ~GPIO_CRH_CNF11_1;
}

uint8_t ds18b20_Reset(void) {
    uint16_t status;
    GPIOB->BSRR = GPIO_BSRR_BR11;
    Delay(480);
    GPIOB->BSRR = GPIO_BSRR_BS11;	
    Delay(60);
    status = GPIOB->IDR & GPIO_IDR_IDR11;
    Delay(480);
    return (status ? 1 : 0);
}

uint8_t ds18b20_ReadBit(void) {
    uint8_t bit = 0;
    GPIOB->BSRR = GPIO_BSRR_BR11;
    Delay(1);
    GPIOB->BSRR = GPIO_BSRR_BS11;
    Delay(14);
    bit = (GPIOB->IDR & GPIO_IDR_IDR11 ? 1 : 0); 
    Delay(45);
    return bit;
}

uint8_t ds18b20_ReadByte(void) {
    uint8_t data = 0;
    for (uint8_t i = 0; i <= 7; i++)
        data |= ds18b20_ReadBit() << i;
    return data;
}

void ds18b20_WriteBit(uint8_t bit) {
    GPIOB->BSRR = GPIO_BSRR_BR11;
    Delay(bit ? 1 : 60);
    GPIOB->BSRR = GPIO_BSRR_BS11; 
    Delay(bit ? 60 : 1);	
}

void ds18b20_WriteByte(uint8_t data) {
    for (uint8_t i = 0; i < 8; i++) {
        ds18b20_WriteBit(data >> i & 1); 	
        Delay(5);
    }
}

void ds18b20_MatchRom(uint8_t* address) {
    uint8_t i;
    ds18b20_Reset();
    ds18b20_WriteByte(MATCH_ROM);
    for(i = 0; i < 8; i++) {
        ds18b20_WriteByte(address[i]);
    }
}

void ds18b20_Init(uint8_t mode, uint8_t* address, uint8_t Th, uint8_t Tl, uint8_t resolution) {
    uint8_t i;
    uint8_t rom_data [8] = {0};
    ds18b20_Reset();
    if(mode == 0) {
        ds18b20_WriteByte(SKIP_ROM);
    } else {
        ds18b20_MatchRom(address);
    }
    ds18b20_WriteByte(WRITE_SCRATCHPAD); 
    ds18b20_WriteByte(Th);
    ds18b20_WriteByte(Tl);
    ds18b20_WriteByte(resolution);
	ds18b20_Reset();
}

void ds18b20_ConvertTemp(uint8_t mode, uint8_t* address) {
    ds18b20_Reset();
    if(mode == 0) {
        ds18b20_WriteByte(SKIP_ROM);
    } else {
        ds18b20_MatchRom(address);
    }
    ds18b20_WriteByte(CONVERT_TEMP);
}

void ds18b20_ReadStratchpad(uint8_t mode, uint8_t *Data, uint8_t* address) {
    uint8_t i;
    ds18b20_Reset();
    if(mode == 0) {
        ds18b20_WriteByte(SKIP_ROM);
    } else {
        ds18b20_MatchRom(address); 
    }
    ds18b20_WriteByte(READ_SCRATCHPAD);
    for(i = 0; i < 9; i++) {
        Data[i] = ds18b20_ReadByte();
    }
}

void ds18b20_ReadROM(uint8_t *Data) {
    uint8_t i;
    ds18b20_Reset();
    ds18b20_WriteByte(READ_ROM);
    for(i = 0; i < 8; i++) {
        Data[i] = ds18b20_ReadByte();
    }
}

uint8_t Compute_CRC8(uint8_t* data, uint8_t length) {
    uint8_t polynomial = 0x8C, crc = 0x0, i = 0, j = 0, lsb = 0, inbyte = 0;
    while (length--) {
        inbyte = data[j];
        for (i = 0; i < 8; i++) {
            lsb = (crc ^ inbyte) & 0x01;
            crc >>= 1;
            if (lsb) 
                crc ^= polynomial;
            inbyte >>= 1;
        }
        j++;
    }
    return crc; 
}

uint8_t Search_ROM(char command, Sensor *sensors) {
    uint8_t i = 0, sensor_num = 0;
    char DS1820_done_flag = 0;
    int DS1820_last_descrepancy = 0;
    char DS1820_search_ROM[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int descrepancy_marker, ROM_bit_index;
    char return_value, Bit_A, Bit_B;
    char byte_counter, bit_mask;
    return_value = 0;
    while (!DS1820_done_flag) {
        if (ds18b20_Reset()) {
            return 0;
        } else {
            ROM_bit_index = 1;
            descrepancy_marker = 0;
            char command_shift = command;
            for (int n = 0; n < 8; n++) {
                ds18b20_WriteBit(command_shift & 0x01);
                command_shift = command_shift >> 1;
            } 
            byte_counter = 0;
            bit_mask = 0x01;
            while (ROM_bit_index <= 64) {
                Bit_A = ds18b20_ReadBit();
                Bit_B = ds18b20_ReadBit();
                if (Bit_A & Bit_B) {
                    descrepancy_marker = 0;
                    ROM_bit_index = 0xFF;
                } else {
                    if (Bit_A | Bit_B) {
                        if (Bit_A) {
                            DS1820_search_ROM[byte_counter] = DS1820_search_ROM[byte_counter] | bit_mask;
                        } else {
                            DS1820_search_ROM[byte_counter] = DS1820_search_ROM[byte_counter] & ~bit_mask;
                        }
                    } else {
                        if (ROM_bit_index == DS1820_last_descrepancy) {
                            DS1820_search_ROM[byte_counter] = DS1820_search_ROM[byte_counter] | bit_mask;
                        } else {
                            if (ROM_bit_index > DS1820_last_descrepancy) {
                                DS1820_search_ROM[byte_counter] = DS1820_search_ROM[byte_counter] & ~bit_mask;
                                descrepancy_marker = ROM_bit_index;
                            } else {
                                if ((DS1820_search_ROM[byte_counter] & bit_mask) == 0x00)
                                    descrepancy_marker = ROM_bit_index;
                            }
                        }
                    }
                    ds18b20_WriteBit(DS1820_search_ROM[byte_counter] & bit_mask);
                    ROM_bit_index++;
                    if (bit_mask & 0x80) {
                        byte_counter++;
                        bit_mask = 0x01;
                    } else {
                        bit_mask = bit_mask << 1;
                    }
                }
            }
            DS1820_last_descrepancy = descrepancy_marker;
            for (i = 0; i < 8; i++) {
                sensors[sensor_num].ROM_code[i] = DS1820_search_ROM[i];
            }
            sensor_num++;
        }
        if (DS1820_last_descrepancy == 0)
            DS1820_done_flag = 1;
    }
    return sensor_num;
}

void ds18b20_ReadConfig(uint8_t mode, uint8_t* address, uint8_t* th, uint8_t* tl, uint8_t* config) {
    uint8_t scratchpad[9];
    ds18b20_ReadStratchpad(mode, scratchpad, address);
    *th = scratchpad[2];
    *tl = scratchpad[3];
    *config = scratchpad[4];
}

void ds18b20_SetConfig(uint8_t mode, uint8_t* address, uint8_t th, uint8_t tl, uint8_t resolution) {
    ds18b20_Reset();
    if(mode == 0) {
        ds18b20_WriteByte(SKIP_ROM);
    } else {
        ds18b20_MatchRom(address);
    }
    ds18b20_WriteByte(WRITE_SCRATCHPAD);
    ds18b20_WriteByte(th);
    ds18b20_WriteByte(tl);
    ds18b20_WriteByte(resolution);
}

void ds18b20_CopyScratchpad(uint8_t mode, uint8_t* address) {
    ds18b20_Reset();
    if(mode == 0) {
        ds18b20_WriteByte(SKIP_ROM);
    } else {
        ds18b20_MatchRom(address);
    }
    ds18b20_WriteByte(COPY_SCRATCHPAD);
    Delay(10000); 
}

void ds18b20_RecallEEPROM(uint8_t mode, uint8_t* address) {
    ds18b20_Reset();
    if(mode == 0) {
        ds18b20_WriteByte(SKIP_ROM);
    } else {
        ds18b20_MatchRom(address);
    }
    ds18b20_WriteByte(RECALL_EEPROM);
    Delay(10000);
}


uint8_t TestDS18B20Genuine(Sensor *sensor) {
    uint8_t original_th, original_tl, original_res;
    uint8_t read_th, read_tl, read_res;
    uint8_t i, all_passed = 1;
    
    ds18b20_ReadStratchpad(1, sensor->scratchpad_data, sensor->ROM_code);
    original_th = sensor->scratchpad_data[2];
    original_tl = sensor->scratchpad_data[3];
    original_res = sensor->scratchpad_data[4];
    
    uint8_t test_th[] = {0x1E, 0x50, 0x00};
    for(i = 0; i < 3; i++) {
        ds18b20_SetConfig(1, sensor->ROM_code, test_th[i], original_tl, original_res);
        ds18b20_CopyScratchpad(1, sensor->ROM_code);
        ds18b20_ReadStratchpad(1, sensor->scratchpad_data, sensor->ROM_code);
        read_th = sensor->scratchpad_data[2];
        if(read_th != test_th[i]) all_passed = 0;
    }
    
    uint8_t test_tl[] = {0x00, 0xE2, 0x1E};
    for(i = 0; i < 3; i++) {
        ds18b20_SetConfig(1, sensor->ROM_code, original_th, test_tl[i], original_res);
        ds18b20_CopyScratchpad(1, sensor->ROM_code);
        ds18b20_ReadStratchpad(1, sensor->scratchpad_data, sensor->ROM_code);
        read_tl = sensor->scratchpad_data[3];
        if(read_tl != test_tl[i]) all_passed = 0;
    }
    
    uint8_t test_res[] = {RESOLUTION_9BIT, RESOLUTION_10BIT, RESOLUTION_11BIT, RESOLUTION_12BIT};
    for(i = 0; i < 4; i++) {
        ds18b20_SetConfig(1, sensor->ROM_code, original_th, original_tl, test_res[i]);
        ds18b20_CopyScratchpad(1, sensor->ROM_code);
        ds18b20_ReadStratchpad(1, sensor->scratchpad_data, sensor->ROM_code);
        read_res = sensor->scratchpad_data[4];
        if(read_res != test_res[i]) all_passed = 0;
    }
    
    ds18b20_SetConfig(1, sensor->ROM_code, original_th, original_tl, original_res);
    ds18b20_CopyScratchpad(1, sensor->ROM_code);
    
    return all_passed;
}