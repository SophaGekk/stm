#ifndef MAIN_H
#define MAIN_H

#include "stm32f10x.h"
#include "ds18b20.h"
#include "lm75a.h"
#include <stdio.h>
#include <string.h>
#include "mgl_rgb3.h"

#define MAX_SENSORS 32
#define SENSOR_CHECK_INTERVAL 500000
#define TEMP_READ_INTERVAL 800000
#define TEMP_THRESHOLD 29.0f
#define BLINK_INTERVAL_MS 50

extern volatile uint32_t msTicks;
extern volatile uint32_t error_counter;

extern Sensor sensors[MAX_SENSORS];
extern Sensor previousSensors[MAX_SENSORS];
extern uint8_t actualSensorCount;
extern uint8_t previousSensorCount;

void LED_Init(void);
void LED_ON(void);
void LED_OFF(void);
void LED_BLINKING(void);
void Init_Sensors(void);
void SaveCurrentSensorState(void);
void CompareAndReportSensorChanges(void);
uint8_t CheckAndUpdateSensors(void);
uint8_t ReadTemperatureFromSensor(uint8_t index);
void StartTemperatureConversion(uint8_t index);
void USART_Init(void);
void USART_SendByte(unsigned char data);
void USART_SendString(const char* str);
unsigned char USART_ReceiveByte(void);
static void USART_SendChar(char c);
void ProcessReceivedString(void);
void SendTemperatureData(void);
void SendROMCode(const uint8_t* rom_code);
int CompareROMCodes(const uint8_t* rom1, const uint8_t* rom2);
void SystemCoreClockConfigure(void);
float GetLM75ATemperature(void);
void UpdateLEDByTemperature(float max_temp);
void Delay(uint32_t dlyTicks); 
uint8_t TestDS18B20Genuine(Sensor *sensor);
#endif 