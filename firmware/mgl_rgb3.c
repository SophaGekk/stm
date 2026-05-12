#include "mgl_rgb3.h"
#include "lm75a.h"
#define I2C_TIMEOUT                     50000
volatile uint8_t device_initialized = 0;
volatile uint8_t debug_mode1 = 0;
volatile uint8_t debug_ledout0 = 0;
volatile uint8_t debug_ledout1 = 0;
uint8_t I2C_WriteReg(uint8_t dev_addr, uint8_t reg, uint8_t value) {
    uint32_t timeout;
    
    I2C1->CR1 |= I2C_CR1_START;
    timeout = I2C_TIMEOUT;
    while(!(I2C1->SR1 & I2C_SR1_SB) && timeout--);
    if(timeout == 0) {
        I2C1->CR1 |= I2C_CR1_STOP;
        return 0;
    }
    
    I2C1->DR = (dev_addr << 1);
    timeout = I2C_TIMEOUT;
    while(!(I2C1->SR1 & I2C_SR1_ADDR) && timeout--);
    if(timeout == 0) {
        I2C1->CR1 |= I2C_CR1_STOP;
        return 0;
    }
    
    (void)I2C1->SR2;
    
    I2C1->DR = reg;
    timeout = I2C_TIMEOUT;
    while(!(I2C1->SR1 & I2C_SR1_TXE) && timeout--);
    if(timeout == 0) {
        I2C1->CR1 |= I2C_CR1_STOP;
        return 0;
    }
    
    I2C1->DR = value;
    timeout = I2C_TIMEOUT;
    while(!(I2C1->SR1 & I2C_SR1_TXE) && timeout--);
    if(timeout == 0) {
        I2C1->CR1 |= I2C_CR1_STOP;
        return 0;
    }
    
    I2C1->CR1 |= I2C_CR1_STOP;
    return 1;
}

uint8_t I2C_ReadReg(uint8_t dev_addr, uint8_t reg, uint8_t *value) {
    uint32_t timeout;
    
    I2C1->CR1 |= I2C_CR1_START;
    timeout = I2C_TIMEOUT;
    while(!(I2C1->SR1 & I2C_SR1_SB) && timeout--);
    if(timeout == 0) {
        I2C1->CR1 |= I2C_CR1_STOP;
        return 0;
    }
    
    I2C1->DR = (dev_addr << 1);
    timeout = I2C_TIMEOUT;
    while(!(I2C1->SR1 & I2C_SR1_ADDR) && timeout--);
    if(timeout == 0) {
        I2C1->CR1 |= I2C_CR1_STOP;
        return 0;
    }
    
    (void)I2C1->SR2;
    
    I2C1->DR = reg;
    timeout = I2C_TIMEOUT;
    while(!(I2C1->SR1 & I2C_SR1_TXE) && timeout--);
    if(timeout == 0) {
        I2C1->CR1 |= I2C_CR1_STOP;
        return 0;
    }
    
    I2C1->CR1 |= I2C_CR1_START;
    timeout = I2C_TIMEOUT;
    while(!(I2C1->SR1 & I2C_SR1_SB) && timeout--);
    if(timeout == 0) {
        I2C1->CR1 |= I2C_CR1_STOP;
        return 0;
    }
    
    I2C1->DR = (dev_addr << 1) | 0x01;
    timeout = I2C_TIMEOUT;
    while(!(I2C1->SR1 & I2C_SR1_ADDR) && timeout--);
    if(timeout == 0) {
        I2C1->CR1 |= I2C_CR1_STOP;
        return 0;
    }
    
    (void)I2C1->SR2;
    
    I2C1->CR1 &= ~I2C_CR1_ACK;
    I2C1->CR1 |= I2C_CR1_STOP;
    
    timeout = I2C_TIMEOUT;
    while(!(I2C1->SR1 & I2C_SR1_RXNE) && timeout--);
    if(timeout == 0) {
        return 0;
    }
    *value = I2C1->DR;
    
    I2C1->CR1 |= I2C_CR1_ACK;
    return 1;
}

void Delay_ms(uint32_t ms) {
    Delay(ms * 1000);
}

uint8_t RGB_Module_Init(void) {
    uint8_t mode1;
    uint8_t ch;
    uint8_t reg_val;
    
    if (!I2C_ReadReg(RGB_MODULE_ADDR, RGB_MODULE_MODE1, &mode1)) return 0;
    
    I2C_WriteReg(RGB_MODULE_ADDR, RGB_MODULE_MODE1, MODE1_ALLCALL);
    Delay(10);
    I2C_WriteReg(RGB_MODULE_ADDR, RGB_MODULE_MODE2, MODE2_NONE);
    
    for (ch = 0; ch < 8; ch++) {
        uint8_t reg_addr = RGB_MODULE_LEDOUT_BASE + (ch >> 2);
        uint8_t shift = (ch & 0x03) * 2;
        
        if (!I2C_ReadReg(RGB_MODULE_ADDR, reg_addr, &reg_val)) return 0;
        reg_val &= ~(0x03 << shift);
        reg_val |= (LED_PWM << shift);
        I2C_WriteReg(RGB_MODULE_ADDR, reg_addr, reg_val);
    }
    
    for (ch = 0; ch < 8; ch++) {
        uint8_t pwm_reg = 0x80 | (RGB_MODULE_PWM_BASE + ch);
        I2C_WriteReg(RGB_MODULE_ADDR, pwm_reg, 0x11);
    }
    
    device_initialized = 1;
    return 1;
}


void RGB_SetBrightness(uint8_t channel, uint8_t value) {
    if (channel >= 8) return;
    uint8_t pwm_reg = 0x80 | (RGB_MODULE_PWM_BASE + channel);
    I2C_WriteReg(RGB_MODULE_ADDR, pwm_reg, value);
}

uint8_t RGB_Module_Check(void) {
    uint8_t mode1;
    
    if(I2C_IsDeviceReady(RGB_MODULE_ADDR)) {
        if(I2C_ReadReg(RGB_MODULE_ADDR, RGB_MODULE_MODE1, &mode1)) {
            debug_mode1 = mode1;
            return 1;
        }
    }
    return 0;
}
uint8_t RGB_Module_SimpleInit(void) {
    uint8_t ch;
    
    if(!RGB_Module_Check()) {
        return 0;
    }
    
    if(!I2C_WriteReg(RGB_MODULE_ADDR, RGB_MODULE_MODE1, MODE1_ALLCALL)) {
        return 0;
    }
    Delay_ms(10);
    
    if(!I2C_WriteReg(RGB_MODULE_ADDR, RGB_MODULE_MODE2, MODE2_NONE)) {
        return 0;
    }
    
    for(ch = 0; ch < 8; ch++) {
        uint8_t reg_addr = RGB_MODULE_LEDOUT_BASE + (ch >> 2);
        uint8_t shift = (ch & 0x03) * 2;
        uint8_t reg_val;
        
        if(!I2C_ReadReg(RGB_MODULE_ADDR, reg_addr, &reg_val)) {
            return 0;
        }
        
        reg_val &= ~(0x03 << shift);
        reg_val |= (LED_PWM << shift);
        
        if(!I2C_WriteReg(RGB_MODULE_ADDR, reg_addr, reg_val)) {
            return 0;
        }
        
        if(ch == 0) debug_ledout0 = reg_val;
        if(ch == 4) debug_ledout1 = reg_val;
    }
    
    for(ch = 0; ch < 8; ch++) {
        uint8_t pwm_reg = 0x80 | (RGB_MODULE_PWM_BASE + ch);
        I2C_WriteReg(RGB_MODULE_ADDR, pwm_reg, 0x11);
    }
    
    return 1;
}

void RGB_Module_BlinkTest(void) {
    uint8_t ch;
    for(ch = 0; ch < 8; ch++) {
        uint8_t pwm_reg = 0x80 | (RGB_MODULE_PWM_BASE + ch);
        I2C_WriteReg(RGB_MODULE_ADDR, pwm_reg, 0x11);
    }
    Delay_ms(1000);
    
    for(ch = 0; ch < 8; ch++) {
        uint8_t pwm_reg = 0x80 | (RGB_MODULE_PWM_BASE + ch);
        I2C_WriteReg(RGB_MODULE_ADDR, pwm_reg, 0x00);
    }
    Delay_ms(1000);
    
    for(ch = 0; ch < 8; ch++) {
        uint8_t pwm_reg = 0x80 | (RGB_MODULE_PWM_BASE + ch);
        I2C_WriteReg(RGB_MODULE_ADDR, pwm_reg, 0x11);
        Delay_ms(500);
        I2C_WriteReg(RGB_MODULE_ADDR, pwm_reg, 0x00);
    }
}

int main_rgb(void) {
		if(RGB_Module_Check()) {
				if(!device_initialized) {
						if(RGB_Module_SimpleInit()) {
								device_initialized++;
						}
				}
				else{
					RGB_Module_BlinkTest();
				}
		} else {
				device_initialized--;
				Delay_ms(500);
		}
}