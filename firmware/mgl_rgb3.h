#ifndef MGL_RGB3_H
#define MGL_RGB3_H

#include "stm32f10x.h"

#define RGB_MODULE_ADDR         0x70
#define RGB_MODULE_MODE1        0x00
#define RGB_MODULE_MODE2        0x01
#define RGB_MODULE_PWM_BASE     0x02
#define RGB_MODULE_LEDOUT_BASE  0x0C
#define MODE1_ALLCALL           0x01
#define MODE2_NONE              0x00
#define LED_PWM                 0x02

uint8_t I2C_WriteReg(uint8_t dev_addr, uint8_t reg_addr, uint8_t data);
uint8_t I2C_ReadReg(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data);
void Delay_ms(uint32_t ms);

extern volatile uint8_t device_initialized;
extern volatile uint8_t debug_mode1;
extern volatile uint8_t debug_ledout0;
extern volatile uint8_t debug_ledout1;

uint8_t RGB_Module_Init(void);
void RGB_SetBrightness(uint8_t channel, uint8_t value);
uint8_t RGB_Module_Check(void);
uint8_t RGB_Module_SimpleInit(void);
void RGB_Module_BlinkTest(void);

#endif 