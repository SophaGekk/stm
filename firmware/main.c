#include "main.h"

Sensor sensors[MAX_SENSORS];
Sensor previousSensors[MAX_SENSORS];  // Store previous sensor state
uint8_t actualSensorCount = 0;
uint8_t previousSensorCount = 0;

static unsigned char received_string[100];
static int char_index = 0;
static int string_received = 0;

static uint32_t lastDataSend = 0;
static uint32_t last_blink_time = 0;
static uint8_t blinking_active = 0;
static uint8_t last_rgb_present = 0;
static uint32_t lm75a_error_counter = 0;  
static uint32_t rgb_error_counter = 0;   
static uint8_t last_lm75a_present = 0;
LM75A_Sensor lm75a_sensor = {0, 0.0f, 0, 0};

void SysTick_Handler(void) {
    msTicks++;
}

void Delay(uint32_t dlyTicks) {
    uint32_t curTicks;
    curTicks = msTicks;
    while ((msTicks - curTicks) < dlyTicks) { __NOP(); }
}

void SystemCoreClockConfigure(void) {
    RCC->CR |= ((uint32_t)RCC_CR_HSEON);                    
    while ((RCC->CR & RCC_CR_HSERDY) == 0);                  

    RCC->CFGR = RCC_CFGR_SW_HSE;                             
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSE);  
    
    RCC->CFGR = RCC_CFGR_HPRE_DIV1;
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV1;
    RCC->CFGR |= RCC_CFGR_PPRE2_DIV1;

    RCC->CR &= ~RCC_CR_PLLON;                                

    RCC->CFGR &= ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL);
    RCC->CFGR |= (RCC_CFGR_PLLSRC_HSE | RCC_CFGR_PLLMULL9);

    RCC->CR |= RCC_CR_PLLON;                                
    while((RCC->CR & RCC_CR_PLLRDY) == 0) __NOP();           

    RCC->CFGR &= ~RCC_CFGR_SW;                               
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);  
}

void LED_Init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    GPIOA->CRL &= ~GPIO_CRL_CNF5;
    GPIOA->CRL &= ~GPIO_CRL_MODE5;
    GPIOA->CRL |= GPIO_CRL_MODE5_1;
    GPIOA->CRL &= ~GPIO_CRL_CNF5;
    GPIOA->BSRR = GPIO_BSRR_BR5;
}

void LED_ON(void) {
    GPIOA->BSRR = GPIO_BSRR_BS5;
}

void LED_OFF(void) {
    GPIOA->BSRR = GPIO_BSRR_BR5;
}

void LED_BLINKING(void) {
    if (GPIOA->ODR & GPIO_ODR_ODR5) {
        GPIOA->BSRR = GPIO_BSRR_BR5;
    } else {
        GPIOA->BSRR = GPIO_BSRR_BS5;
    }
}

void UpdateLEDByTemperature(float max_temp) {
    if(max_temp > TEMP_THRESHOLD) {
        if(!blinking_active) {
            blinking_active = 1;
            last_blink_time = msTicks;
            LED_ON();
        } else {
            if((msTicks - last_blink_time) >= BLINK_INTERVAL_MS) {
                LED_BLINKING();
                last_blink_time = msTicks;
            }
        }
    } else {
        if(blinking_active) {
            blinking_active = 0;
            LED_OFF();
        }
    }
}

void Init_Sensors(void) {
    for (uint8_t i = 0; i < MAX_SENSORS; i++) {
        sensors[i].raw_temp = 0x0;
        sensors[i].temp = 0.0;
        sensors[i].crc8_rom = 0x0;
        sensors[i].crc8_data = 0x0;
        sensors[i].crc8_rom_error = 0x0;
        sensors[i].crc8_data_error = 0x0;
        for (uint8_t j = 0; j < 8; j++) {
            sensors[i].ROM_code[j] = 0x00;
            previousSensors[i].ROM_code[j] = 0x00;
        }
        for (uint8_t j = 0; j < 9; j++) {
            sensors[i].scratchpad_data[j] = 0x00;
        }
    }
}

void SaveCurrentSensorState(void) {
    previousSensorCount = actualSensorCount;
    for (uint8_t i = 0; i < actualSensorCount; i++) {
        for (uint8_t j = 0; j < 8; j++) {
            previousSensors[i].ROM_code[j] = sensors[i].ROM_code[j];
        }
    }
}

int CompareROMCodes(const uint8_t* rom1, const uint8_t* rom2) {
    for (uint8_t i = 0; i < 8; i++) {
        if (rom1[i] != rom2[i]) {
            return 0;
        }
    }
    return 1;
}

void SendROMCode(const uint8_t* rom_code) {
    for (uint8_t j = 0; j < 8; j++) {
        char rom_byte[4];
        sprintf(rom_byte, "%02X", rom_code[j]);
        USART_SendString(rom_byte);
    }
}

void CompareAndReportSensorChanges(void) {
    uint8_t found;
    for (uint8_t i = 0; i < previousSensorCount; i++) {
        found = 0;
        for (uint8_t j = 0; j < actualSensorCount; j++) {
            if (CompareROMCodes(previousSensors[i].ROM_code, sensors[j].ROM_code)) {
                found = 1;
                break;
            }
        }
        if (!found) {
					USART_SendString("\r\nSensor disconnected: ");
            SendROMCode(previousSensors[i].ROM_code);
            USART_SendString("\r\n");
        }
    }
    
    for (uint8_t i = 0; i < actualSensorCount; i++) {
        found = 0;
        for (uint8_t j = 0; j < previousSensorCount; j++) {
            if (CompareROMCodes(sensors[i].ROM_code, previousSensors[j].ROM_code)) {
                found = 1;
                break;
            }
        }
        if (!found) {
					USART_SendString("\r\nSensor connected: ");
            SendROMCode(sensors[i].ROM_code);
            USART_SendString("\r\n");
        }
    }
		USART_SendString("Sensor count: ");
    char count_str[4];
    sprintf(count_str, "%d", actualSensorCount);
    USART_SendString(count_str);
    USART_SendString("\r\n");
}

uint8_t CheckAndUpdateSensors(void) {
    uint8_t newCount = 0;
    Sensor tempSensors[MAX_SENSORS];
    
    for (uint8_t i = 0; i < MAX_SENSORS; i++) {
        for (uint8_t j = 0; j < 8; j++) {
            tempSensors[i].ROM_code[j] = 0x00;
        }
    }
    
    newCount = Search_ROM(SEARCH_ROM, tempSensors);
    
    if (newCount != actualSensorCount) {
        SaveCurrentSensorState();
        
        actualSensorCount = newCount;
        
        for (uint8_t i = 0; i < actualSensorCount; i++) {
            for (uint8_t j = 0; j < 8; j++) {
                sensors[i].ROM_code[j] = tempSensors[i].ROM_code[j];
            }
        }
        
        for (uint8_t i = 0; i < actualSensorCount; i++) {
            ds18b20_Init(1, sensors[i].ROM_code, 0x1E, 0xE2, RESOLUTION_12BIT);
						sensors[i].isGenuine = TestDS18B20Genuine(&sensors[i]);
					  char genuine_str[50];
						sprintf(genuine_str, "DS18B20 #%d is %s\r\n", i, sensors[i].isGenuine ? "GENUINE" : "FAKE");
						USART_SendString(genuine_str);
        }
        CompareAndReportSensorChanges();
        
        return 1;
    }
    
    return 0;
}

uint8_t ReadTemperatureFromSensor(uint8_t index) {
    uint8_t data[9];
    
    if (index >= actualSensorCount) return 0;
    
    if (ds18b20_Reset() != 0) return 0;
    
    ds18b20_WriteByte(MATCH_ROM);
    for (uint8_t i = 0; i < 8; i++) {
        ds18b20_WriteByte(sensors[index].ROM_code[i]);
    }
    
    ds18b20_WriteByte(0xBE);
    
    for (uint8_t i = 0; i < 9; i++) {
        data[i] = ds18b20_ReadByte();
        sensors[index].scratchpad_data[i] = data[i];
    }
    
    sensors[index].crc8_data_error = Compute_CRC8(data, 9) != 0;
    
    if (!sensors[index].crc8_data_error) {
        int16_t raw_temp = (int16_t)(((uint16_t)data[1] << 8) | data[0]);
        switch(data[4]) {
            case 0x1F: // 9 bit resolution (0.5°C)
                raw_temp = raw_temp & 0xFFF8;  
                sensors[index].temp = raw_temp * 0.0625;  
                break;
            case 0x3F:  // 10 bit resolution (0.25°C)
                raw_temp = raw_temp & 0xFFFC; 
                sensors[index].temp = raw_temp * 0.0625;
                break;
            case 0x5F:  // 11 bit resolution (0.125°C)
                raw_temp = raw_temp & 0xFFFE;
                sensors[index].temp = raw_temp * 0.0625;
                break;
            case 0x7F:  // 12 bit resolution (0.0625°C)
                sensors[index].temp = raw_temp * 0.0625;
                break;
            default:
                sensors[index].temp = raw_temp * 0.0625;
                break;
        }
        return 1;
    }
    
    return 0;
}

void StartTemperatureConversion(uint8_t index) {
    if (index >= actualSensorCount) return;
    
    if (ds18b20_Reset() == 0) {
        ds18b20_WriteByte(MATCH_ROM);
        for (uint8_t i = 0; i < 8; i++) {
            ds18b20_WriteByte(sensors[index].ROM_code[i]);
        }
        ds18b20_WriteByte(0x44);
    }
}

void USART_Init(void) {
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN; 
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN; 
    
    GPIOA->CRL &= (~GPIO_CRL_CNF2_0); 
    GPIOA->CRL |= (GPIO_CRL_CNF2_1 | GPIO_CRL_MODE2);
    
    GPIOA->CRL &= (~GPIO_CRL_CNF3_0);
    GPIOA->CRL |= GPIO_CRL_CNF3_1;
    GPIOA->CRL &= (~(GPIO_CRL_MODE3));
    GPIOA->BSRR |= GPIO_ODR_ODR3;
    
    USART2->BRR = 7500; // 9600 baud
    
    USART2->CR1 |= USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
    USART2->CR2 = 0;
    USART2->CR3 = 0;
}

void USART_SendByte(unsigned char data) {
    while ((USART2->SR & USART_SR_TXE) == 0) {}
    USART2->DR = data;
}

void USART_SendString(const char* str) {
    while (*str) {
        USART_SendByte(*str++);
    }
}

static void USART_SendChar(char c) {
    while ((USART2->SR & USART_SR_TXE) == 0) {}
    USART2->DR = c;
}

unsigned char USART_ReceiveByte(void) {
    while ((USART2->SR & USART_SR_RXNE) == 0) {}
    return (unsigned char)USART2->DR;
}

void SendTemperatureData(void) {
    USART_SendString("\r\nDS18B20 Sensor count: ");
    char count_str[32];
    sprintf(count_str, "%d", actualSensorCount);
    USART_SendString(count_str);
    USART_SendString("\r\n");
    
    for (uint8_t i = 0; i < actualSensorCount; i++) {
        USART_SendString("DS18B20 Sensor ");
        char sensor_num[32];
        sprintf(sensor_num, "%d", i);
        USART_SendString(sensor_num);
        USART_SendString(" (");
        for (uint8_t j = 0; j < 8; j++) {
            char rom_byte[32];
            sprintf(rom_byte, "%02X", sensors[i].ROM_code[j]);
            USART_SendString(rom_byte);
        }
        USART_SendString("): ");
        
        char temp_str[20];
        uint8_t resolution = sensors[i].scratchpad_data[4];
        int precision = 2;
        switch(resolution) {
            case 0x1F: precision = 1; break;  // 9 bit - 0.5°C
            case 0x3F: precision = 2; break;  // 10 bit - 0.25°C
            case 0x5F: precision = 3; break;  // 11 bit - 0.125°C
            case 0x7F: precision = 4; break;  // 12 bit - 0.0625°C
            default: precision = 2; break;
        }
        sprintf(temp_str, "%.*f C\r\n", precision, sensors[i].temp);
        USART_SendString(temp_str);
    }
		for (uint8_t i = 0; i < actualSensorCount; i++) {
        char genuine_str[50];
        sprintf(genuine_str, "DS18B20 #%d is %s\r\n", i, sensors[i].isGenuine ? "GENUINE" : "FAKE");
        USART_SendString(genuine_str);
    }
		
		USART_SendString("LM75A Sensor ");
    if(lm75a_sensor.present) {
        char lm75a_str[30];
        sprintf(lm75a_str, "Address 0x%02X: %.2f C\r\n", lm75a_sensor.address, lm75a_sensor.temp);
        USART_SendString(lm75a_str);
    } else {
        USART_SendString("Not found\r\n");
    }
}

void ProcessReceivedString(void) {
    if (strcmp((char*)received_string, "temp") == 0) {
        SendTemperatureData();
    }else if (strcmp((char*)received_string, "help") == 0) {
        USART_SendString("\r\nCommands:\r\n");
        USART_SendString("  temp - Show all temperatures\r\n");
        USART_SendString("  help            - Show this help\r\n");
				USART_SendString("  set_ds idx,th,tl,res - Configure DS18B20\r\n");
			  USART_SendString("  set_lm75a th,tl - Configure LM75A\r\n");
			  USART_SendString("  rgb_set value   - Set RGB brightness (0-255)\r\n");
        USART_SendString("  rgb_test        - Run RGB test sequence\r\n");
        USART_SendString("  rgb_off         - Turn off RGB\r\n");
    }else if (strncmp((char*)received_string, "set_ds", 6) == 0) {
        int idx;
        float th, tl;
        int res;
        
        int parsed = sscanf((char*)received_string, "set_ds %d,%f,%f,%d", &idx, &th, &tl, &res);
        
        if(parsed == 4 && idx >= 0 && idx < actualSensorCount) {
            uint8_t th_reg = (uint8_t)(th);
            uint8_t tl_reg = (uint8_t)(tl);
            uint8_t res_reg;
            
            switch(res) {
                case 9:  res_reg = RESOLUTION_9BIT; break;
                case 10: res_reg = RESOLUTION_10BIT; break;
                case 11: res_reg = RESOLUTION_11BIT; break;
                default: res_reg = RESOLUTION_12BIT; break;
            }
            
            ds18b20_SetConfig(1, sensors[idx].ROM_code, th_reg, tl_reg, res_reg);
            ds18b20_CopyScratchpad(1, sensors[idx].ROM_code);
            
            uint8_t verify_data[9];
            ds18b20_ReadStratchpad(1, verify_data, sensors[idx].ROM_code);
            
            sensors[idx].scratchpad_data[2] = verify_data[2];
            sensors[idx].scratchpad_data[3] = verify_data[3];
            sensors[idx].scratchpad_data[4] = verify_data[4];
            
            char response[100];
            sprintf(response, "DS18B20 #%d configured: TH=%.1f°C, TL=%.1f°C, Resolution=%d bit\r\n", 
                    idx, th, tl, res);
            USART_SendString(response);
            
            char genuine_str[50];
            sprintf(genuine_str, "DS18B20 #%d is %s\r\n", idx, sensors[idx].isGenuine ? "GENUINE" : "FAKE");
            USART_SendString(genuine_str);
        } else {
            USART_SendString("ERROR: Invalid set_ds command format!\r\n");
            USART_SendString("Usage: set_ds idx,th,tl,res (example: set_ds 0,30.0,-10.0,12)\r\n");
        }
    }else if (strncmp((char*)received_string, "set_lm75a", 9) == 0) {
    float th, tl;
    int parsed = sscanf((char*)received_string, "set_lm75a %f,%f", &th, &tl);
    
    if(parsed == 2 && lm75a_sensor.present) {
        uint8_t th_reg = (uint8_t)(th);
        uint8_t tl_reg = (uint8_t)(tl);
        
        uint8_t write_buf_th[2] = {th_reg, 0};
        uint8_t write_buf_tl[2] = {tl_reg, 0};
        
        I2C_WriteData(lm75a_sensor.address, write_buf_th, 2);
        Delay(10);
        I2C_WriteData(lm75a_sensor.address, write_buf_tl, 2);
        
        char response[100];
        sprintf(response, "LM75A configured: TH=%.1f°C, TL=%.1f°C\r\n", th, tl);
        USART_SendString(response);
    } else {
        USART_SendString("ERROR: Invalid set_lm75a command or LM75A not found!\r\n");
    }
	}
		else if (strncmp((char*)received_string, "rgb_set", 7) == 0) {
        int value;
        int parsed = sscanf((char*)received_string, "rgb_set %d", &value);
        
        if(parsed == 1 && value >= 0 && value <= 255) {
            if(!device_initialized) {
                USART_SendString("ERROR: RGB module not initialized!\r\n");
                return;
            }
            
            for(int ch = 0; ch < 8; ch++) {
                RGB_SetBrightness(ch, value);
            }
            char response[50];
            sprintf(response, "RGB brightness set to %d\r\n", value);
            USART_SendString(response);
        } else {
            USART_SendString("ERROR: Use rgb_set 0-255\r\n");
        }
    }
    else if (strncmp((char*)received_string, "rgb_test", 8) == 0) {
        if(!device_initialized) {
            USART_SendString("ERROR: RGB module not initialized!\r\n");
            return;
        }
        USART_SendString("Running RGB test sequence...\r\n");
        RGB_Module_BlinkTest();
        USART_SendString("RGB test completed\r\n");
    }
    else if (strncmp((char*)received_string, "rgb_off", 7) == 0) {
        if(!device_initialized) {
            USART_SendString("ERROR: RGB module not initialized!\r\n");
            return;
        }
        for(int ch = 0; ch < 8; ch++) {
            RGB_SetBrightness(ch, 0);
        }
        USART_SendString("RGB turned off\r\n");
    }
}

int main(void) {
    uint8_t i = 0;
    uint32_t lastSensorRead = 0;
    uint32_t lastSensorCheck = 0;
	  uint32_t lastLM75ARead = 0;
	  float max_temperature = 0.0f;
    unsigned char received_byte;
    
    SystemCoreClockConfigure();
    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / 1000000);
    
    ds18b20_PortInit();
	  I2C_Init();
    LED_Init();
    USART_Init();
    
    while (ds18b20_Reset());
    
    Init_Sensors();
    
    actualSensorCount = Search_ROM(SEARCH_ROM, sensors);
    SaveCurrentSensorState();
    
    USART_SendString("Found ");
    char count_str[32];
    sprintf(count_str, "%d", actualSensorCount);
    USART_SendString(count_str);
    USART_SendString(" DS18B20 sensors\r\n");
    
    for (i = 0; i < actualSensorCount; i++) {
        ds18b20_Init(1, sensors[i].ROM_code, 0x1E, 0xE2, RESOLUTION_12BIT);
			  sensors[i].isGenuine = TestDS18B20Genuine(&sensors[i]);
				char genuine_str[50];
				sprintf(genuine_str, "DS18B20 #%d is %s\r\n", i, sensors[i].isGenuine ? "GENUINE" : "FAKE");
				USART_SendString(genuine_str);
    }
    
		lm75a_sensor.address = CheckAnyDevice();
    if(lm75a_sensor.address != 0) {
        lm75a_sensor.present = 1;
        USART_SendString("LM75A sensor found at address 0x");
        char addr_str[4];
        sprintf(addr_str, "%02X\r\n", lm75a_sensor.address);
        USART_SendString(addr_str);
    } else {
        lm75a_sensor.present = 0;
        USART_SendString("LM75A sensor not found\r\n");
    }
		
		if(RGB_Module_Check()) {
        if(RGB_Module_SimpleInit()) {
            device_initialized = 1;
					  last_rgb_present = 1;
            USART_SendString("RGB module initialized successfully\r\n");
        } else {
            USART_SendString("ERROR: RGB module initialization failed\r\n");
        }
    } else {
        USART_SendString("RGB module not found on I2C bus\r\n");
    }
		
    for (i = 0; i < actualSensorCount; i++) {
        StartTemperatureConversion(i);
    }
    
    lastSensorRead = msTicks;
    lastSensorCheck = msTicks;
    lastDataSend = msTicks;
    lastLM75ARead = msTicks;
    
    while (1) {
        if (USART2->SR & USART_SR_RXNE) {
            received_byte = USART_ReceiveByte();
            USART_SendChar(received_byte);
            
            if ((received_byte == '\r') || (received_byte == '\n')) {
                if (char_index > 0) {
                    USART_SendString("\r\n");
                    received_string[char_index] = '\0';
                    ProcessReceivedString();
                    char_index = 0;
                    string_received = 1;
                }
            }
            else if (received_byte == '\b' || received_byte == 0x7F) {
                if (char_index > 0) {
                    char_index--;
                    USART_SendString("\b \b");
                }
            }
            else {
                if (char_index < 99) {
                    received_string[char_index] = received_byte;
                    char_index++;
                }
            }
        }
        
        if (msTicks - lastSensorCheck > SENSOR_CHECK_INTERVAL) {
            lastSensorCheck = msTicks;
            
            if (CheckAndUpdateSensors()) {
                for (i = 0; i < actualSensorCount; i++) {
                    StartTemperatureConversion(i);
                }
                lastSensorRead = msTicks;
            }
        }
        
        if (msTicks - lastSensorRead > TEMP_READ_INTERVAL) {
            lastSensorRead = msTicks;
					  max_temperature = 0.0f;
            
            for (i = 0; i < actualSensorCount; i++) {
                if (ReadTemperatureFromSensor(i)) {
                    if (sensors[i].temp > max_temperature) {
												max_temperature = sensors[i].temp;  
                    }
                } else {
                    sensors[i].temp = 0.0;
                    sensors[i].raw_temp = 0;
                    sensors[i].crc8_data_error = 1;
                }
            }
            
            for (i = 0; i < actualSensorCount; i++) {
                StartTemperatureConversion(i);
            }
        }
				if (msTicks - lastLM75ARead > 500000) {
						lastLM75ARead = msTicks;
						
						uint8_t lm75a_online = I2C_IsDeviceReady(lm75a_sensor.address);
    
						if(lm75a_online && lm75a_sensor.address != 0) {
								float temp_value;
								if(LM75A_ReadTemperature(lm75a_sensor.address, &temp_value)) {
										lm75a_sensor.temp = temp_value;
										if(lm75a_sensor.temp > max_temperature) {
												max_temperature = lm75a_sensor.temp;
										}
										lm75a_error_counter = 0;
										
										if(!lm75a_sensor.present) {
												lm75a_sensor.present = 1;
										}
								} else {
										lm75a_error_counter++;
										if(lm75a_error_counter > 3) {
												if(lm75a_sensor.present) {
														lm75a_sensor.present = 0;
												}
										}
								}
						} else {
								if(lm75a_sensor.present) {
										lm75a_sensor.present = 0;
										lm75a_sensor.temp = 0;
										USART_SendString("\r\nLM75A sensor disconnected!\r\n");
								} else {
										uint8_t new_addr = CheckAnyDevice();
										if(new_addr != 0) {
												lm75a_sensor.address = new_addr;
												lm75a_sensor.present = 1;
												USART_SendString("\r\nLM75A sensor connected at address 0x");
												char addr_str[4];
												sprintf(addr_str, "%02X\r\n", lm75a_sensor.address);
												USART_SendString(addr_str);
										}
								}
						}
				}
				
				static uint32_t lastRGBCheck = 0;
				if(msTicks - lastRGBCheck > 2000000) {
						lastRGBCheck = msTicks;
					  uint8_t rgb_online = 0;
			
						if(I2C_IsDeviceReady(RGB_MODULE_ADDR)) {
								rgb_online = 1;
						}
						
						if(rgb_online) {
								if(!last_rgb_present) {
										last_rgb_present = 1;
										device_initialized = 1;
										USART_SendString("\r\nRGB module connected!\r\n");
										RGB_Module_SimpleInit();
								}
						} else {
								if(last_rgb_present) {
										last_rgb_present = 0;
										device_initialized = 0;
										USART_SendString("\r\nRGB module disconnected!\r\n");
								}
						}
				}
        
        UpdateLEDByTemperature(max_temperature);
        
        if (msTicks - lastDataSend > 5000000) {
            lastDataSend = msTicks;
            SendTemperatureData();
        }
        Delay(10);
    }
}